/* WebAssembly DWARF line extraction and debug-custom-section filtering. */

#include "wasm_dwarf.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* One bounded byte reader used only for sections and DWARF line programs. */
typedef struct Reader {
  const unsigned char *bytes;
  size_t size;
  size_t offset;
} Reader;

/* One decoded DWARF file-table entry. */
typedef struct DwarfFile {
  char *path;
} DwarfFile;

/* One emitted DWARF line-program row. */
typedef struct DwarfRow {
  uint32_t address;
  uint32_t line;
  uint32_t column;
  char *file;
  bool end_sequence;
} DwarfRow;

/* One original Wasm function-body address span. */
typedef struct FunctionSpan {
  uint32_t start;
  uint32_t end;
} FunctionSpan;

/* Copies a nullable string into owned storage. */
static char *copy_string(const char *source) {
  size_t size;
  char *copy;
  if (source == NULL)
    return NULL;
  size = strlen(source) + 1;
  copy = malloc(size);
  if (copy != NULL)
    memcpy(copy, source, size);
  return copy;
}

/* Reads one byte while enforcing the current section boundary. */
static bool read_u8(Reader *reader, uint8_t *value) {
  if (reader->offset >= reader->size)
    return false;
  *value = reader->bytes[reader->offset++];
  return true;
}

/* Reads a little-endian 16-bit value. */
static bool read_u16(Reader *reader, uint16_t *value) {
  if (reader->size - reader->offset < 2)
    return false;
  *value = (uint16_t)(reader->bytes[reader->offset] | ((uint16_t)reader->bytes[reader->offset + 1] << 8));
  reader->offset += 2;
  return true;
}

/* Reads a little-endian 32-bit value. */
static bool read_u32(Reader *reader, uint32_t *value) {
  if (reader->size - reader->offset < 4)
    return false;
  *value = (uint32_t)reader->bytes[reader->offset] | ((uint32_t)reader->bytes[reader->offset + 1] << 8) |
           ((uint32_t)reader->bytes[reader->offset + 2] << 16) |
           ((uint32_t)reader->bytes[reader->offset + 3] << 24);
  reader->offset += 4;
  return true;
}

/* Reads one bounded unsigned LEB128 value up to 32 bits. */
static bool read_uleb32(Reader *reader, uint32_t *value) {
  uint32_t result = 0;
  unsigned shift = 0;
  uint8_t byte;
  while (shift < 35 && read_u8(reader, &byte)) {
    if (shift == 28 && (byte & 0x70u) != 0)
      return false;
    result |= (uint32_t)(byte & 0x7fu) << shift;
    if ((byte & 0x80u) == 0) {
      *value = result;
      return true;
    }
    shift += 7;
  }
  return false;
}

/* Reads one bounded signed LEB128 value up to 32 bits. */
static bool read_sleb32(Reader *reader, int32_t *value) {
  uint32_t result = 0;
  unsigned shift = 0;
  uint8_t byte;
  do {
    if (shift >= 35 || !read_u8(reader, &byte))
      return false;
    result |= (uint32_t)(byte & 0x7fu) << shift;
    shift += 7;
  } while ((byte & 0x80u) != 0);
  if (shift < 32 && (byte & 0x40u) != 0)
    result |= UINT32_MAX << shift;
  *value = (int32_t)result;
  return true;
}

/* Reads a Wasm/DWARF byte string without assuming UTF-8 validity. */
static bool read_string(Reader *reader, char **text) {
  size_t start = reader->offset;
  size_t length;
  while (reader->offset < reader->size && reader->bytes[reader->offset] != 0)
    ++reader->offset;
  if (reader->offset >= reader->size)
    return false;
  length = reader->offset - start;
  *text = malloc(length + 1);
  if (*text == NULL)
    return false;
  memcpy(*text, reader->bytes + start, length);
  (*text)[length] = '\0';
  ++reader->offset;
  return true;
}

/* Returns whether a custom section is DWARF data or its relocation metadata. */
static bool is_dwarf_section_name(const char *name) {
  const char *candidate = name;
  if (strncmp(candidate, "reloc.", 6) == 0)
    candidate += 6;
  return strncmp(candidate, ".debug_", 7) == 0 || strncmp(candidate, ".zdebug_", 8) == 0;
}

/* Appends a discovered DWARF custom-section name to reportable metadata. */
static bool append_section_name(WasmDwarfInfo *info, const char *name, Diagnostics *diagnostics) {
  char **names;
  char *copy;
  if (info->section_count == SIZE_MAX / sizeof(*names))
    return false;
  copy = copy_string(name);
  names = realloc(info->section_names, (info->section_count + 1) * sizeof(*names));
  if (copy == NULL || names == NULL) {
    free(copy);
    diagnostics_error(diagnostics, "out of memory recording Wasm DWARF sections");
    return false;
  }
  info->section_names = names;
  info->section_names[info->section_count++] = copy;
  info->present = true;
  return true;
}

/* Joins one DWARF directory and filename without normalizing source identity. */
static char *join_path(const char *directory, const char *name) {
  size_t directory_length = directory == NULL ? 0 : strlen(directory);
  size_t name_length = strlen(name);
  bool separator = directory_length != 0 && directory[directory_length - 1] != '/' &&
                   directory[directory_length - 1] != '\\';
  char *path = malloc(directory_length + (separator ? 1 : 0) + name_length + 1);
  if (path == NULL)
    return NULL;
  if (directory_length != 0)
    memcpy(path, directory, directory_length);
  if (separator)
    path[directory_length++] = '/';
  memcpy(path + directory_length, name, name_length + 1);
  return path;
}

/* Releases one temporary DWARF directory/file table. */
static void dispose_strings(char **items, size_t count) {
  size_t index;
  for (index = 0; index < count; ++index)
    free(items[index]);
  free(items);
}

/* Appends one string to a growable temporary table. */
static bool append_string(char ***items, size_t *count, char *text) {
  char **grown;
  if (*count == SIZE_MAX / sizeof(*grown))
    return false;
  grown = realloc(*items, (*count + 1) * sizeof(*grown));
  if (grown == NULL)
    return false;
  *items = grown;
  (*items)[(*count)++] = text;
  return true;
}

/* Reads and appends a DWARF v2-v4 file entry. */
static bool read_file_entry(Reader *reader, char **directories, size_t directory_count, DwarfFile **files,
                            size_t *file_count) {
  char *name = NULL;
  char *path = NULL;
  DwarfFile *grown;
  uint32_t directory_index, ignored;
  if (!read_string(reader, &name) || !read_uleb32(reader, &directory_index) || !read_uleb32(reader, &ignored) ||
      !read_uleb32(reader, &ignored))
    goto fail;
  if (directory_index > directory_count)
    goto fail;
  path = join_path(directory_index == 0 ? NULL : directories[directory_index - 1], name);
  if (path == NULL)
    goto fail;
  grown = realloc(*files, (*file_count + 1) * sizeof(*grown));
  if (grown == NULL)
    goto fail;
  *files = grown;
  (*files)[*file_count].path = path;
  ++*file_count;
  free(name);
  return true;
fail:
  free(name);
  free(path);
  return false;
}

/* Appends one line row using a stable copy of its current file path. */
static bool append_row(DwarfRow **rows, size_t *count, uint32_t address, uint32_t line, uint32_t column,
                       uint32_t file, const DwarfFile *files, size_t file_count, bool end_sequence) {
  DwarfRow *grown;
  char *path;
  if (file == 0 || file > file_count)
    return true;
  path = copy_string(files[file - 1].path);
  grown = realloc(*rows, (*count + 1) * sizeof(*grown));
  if (path == NULL || grown == NULL) {
    free(path);
    return false;
  }
  *rows = grown;
  (*rows)[*count].address = address;
  (*rows)[*count].line = line;
  (*rows)[*count].column = column;
  (*rows)[*count].file = path;
  (*rows)[*count].end_sequence = end_sequence;
  ++*count;
  return true;
}

/* Releases temporary rows after their best function locations are copied. */
static void dispose_rows(DwarfRow *rows, size_t count) {
  size_t index;
  for (index = 0; index < count; ++index)
    free(rows[index].file);
  free(rows);
}

/* Records a nonfatal reason why source lines could not be decoded. */
static bool set_line_error(WasmDwarfInfo *info, const char *message, Diagnostics *diagnostics) {
  free(info->line_error);
  info->line_error = copy_string(message);
  if (info->line_error == NULL) {
    diagnostics_error(diagnostics, "out of memory recording DWARF line-table status");
    return false;
  }
  info->line_table_supported = false;
  return true;
}

/* Decodes one DWARF v2-v4 line unit, including define_file and unknown extensions. */
static bool parse_line_unit(Reader *section, DwarfRow **rows, size_t *row_count, WasmDwarfInfo *info,
                            Diagnostics *diagnostics) {
  Reader unit, header;
  uint32_t unit_length, header_length;
  uint16_t version;
  uint8_t minimum_instruction_length, maximum_operations = 1, default_is_stmt;
  uint8_t line_range, opcode_base, byte;
  int8_t line_base;
  uint8_t *standard_lengths = NULL;
  char **directories = NULL;
  size_t directory_count = 0;
  DwarfFile *files = NULL;
  size_t file_count = 0, index;
  uint32_t address = 0, file = 1, line = 1, column = 0;
  bool address_valid = true;
  bool success = false;

  if (!read_u32(section, &unit_length) || unit_length == UINT32_MAX || unit_length > section->size - section->offset)
    return set_line_error(info, "unsupported or malformed DWARF64 line unit", diagnostics);
  unit.bytes = section->bytes + section->offset;
  unit.size = unit_length;
  unit.offset = 0;
  section->offset += unit_length;
  if (!read_u16(&unit, &version))
    return set_line_error(info, "truncated DWARF line-unit header", diagnostics);
  info->line_version = version;
  if (version < 2 || version > 4)
    return set_line_error(info, "only DWARF v2-v4 line tables are currently supported", diagnostics);
  if (!read_u32(&unit, &header_length) || header_length > unit.size - unit.offset)
    return set_line_error(info, "malformed DWARF line-table prologue", diagnostics);
  header.bytes = unit.bytes + unit.offset;
  header.size = header_length;
  header.offset = 0;
  unit.offset += header_length;
  if (!read_u8(&header, &minimum_instruction_length) ||
      (version >= 4 && !read_u8(&header, &maximum_operations)) || !read_u8(&header, &default_is_stmt) ||
      !read_u8(&header, &byte))
    goto malformed;
  line_base = (int8_t)byte;
  if (!read_u8(&header, &line_range) || !read_u8(&header, &opcode_base) || line_range == 0 || opcode_base == 0 ||
      maximum_operations != 1)
    goto malformed;
  standard_lengths = calloc(opcode_base, 1);
  if (standard_lengths == NULL)
    goto memory_failure;
  for (index = 1; index < opcode_base; ++index)
    if (!read_u8(&header, &standard_lengths[index]))
      goto malformed;
  for (;;) {
    char *directory = NULL;
    if (!read_string(&header, &directory))
      goto malformed;
    if (directory[0] == '\0') {
      free(directory);
      break;
    }
    if (!append_string(&directories, &directory_count, directory)) {
      free(directory);
      goto memory_failure;
    }
  }
  for (;;) {
    if (header.offset >= header.size)
      goto malformed;
    if (header.bytes[header.offset] == 0) {
      ++header.offset;
      break;
    }
    if (!read_file_entry(&header, directories, directory_count, &files, &file_count))
      goto malformed;
  }
  if (header.offset != header.size)
    goto malformed;

  while (unit.offset < unit.size) {
    uint8_t opcode;
    if (!read_u8(&unit, &opcode))
      goto malformed;
    if (opcode == 0) {
      uint32_t length;
      size_t end;
      uint8_t extended;
      if (!read_uleb32(&unit, &length) || length == 0 || length > unit.size - unit.offset)
        goto malformed;
      end = unit.offset + length;
      if (!read_u8(&unit, &extended))
        goto malformed;
      if (extended == 1) {
        if (address_valid && !append_row(rows, row_count, address, line, column, file, files, file_count, true))
          goto memory_failure;
        address = 0;
        address_valid = true;
        file = 1;
        line = 1;
        column = 0;
      } else if (extended == 2) {
        uint32_t value = 0;
        size_t shift = 0;
        while (unit.offset < end && shift < 32)
          value |= (uint32_t)unit.bytes[unit.offset++] << (shift++ * 8);
        address = value;
        /* Some linked Wasm producers retain 0xffffffff placeholder sequences.
         * Advancing those would wrap into plausible but misleading addresses. */
        address_valid = value != UINT32_MAX;
      } else if (extended == 3) {
        Reader entry = {unit.bytes + unit.offset, end - unit.offset, 0};
        if (!read_file_entry(&entry, directories, directory_count, &files, &file_count) || entry.offset != entry.size)
          goto malformed;
      }
      /* Unknown/newer extended opcodes are bounded and safely skipped. */
      unit.offset = end;
      continue;
    }
    if (opcode >= opcode_base) {
      uint8_t adjusted = (uint8_t)(opcode - opcode_base);
      uint32_t advance = (uint32_t)(adjusted / line_range) * minimum_instruction_length;
      int64_t next_line = (int64_t)line + line_base + adjusted % line_range;
      if (address_valid && advance > UINT32_MAX - address)
        address_valid = false;
      else if (address_valid)
        address += advance;
      if (next_line < 0 || next_line > UINT32_MAX)
        goto malformed;
      line = (uint32_t)next_line;
      if (address_valid && !append_row(rows, row_count, address, line, column, file, files, file_count, false))
        goto memory_failure;
      continue;
    }
    if (opcode == 1) {
      if (address_valid && !append_row(rows, row_count, address, line, column, file, files, file_count, false))
        goto memory_failure;
    } else if (opcode == 2) {
      uint32_t advance;
      if (!read_uleb32(&unit, &advance))
        goto malformed;
      if (minimum_instruction_length != 0 && advance > UINT32_MAX / minimum_instruction_length)
        address_valid = false;
      else {
        advance *= minimum_instruction_length;
        if (address_valid && advance > UINT32_MAX - address)
          address_valid = false;
        else if (address_valid)
          address += advance;
      }
    } else if (opcode == 3) {
      int32_t advance;
      int64_t next_line;
      if (!read_sleb32(&unit, &advance))
        goto malformed;
      next_line = (int64_t)line + advance;
      if (next_line < 0 || next_line > UINT32_MAX)
        goto malformed;
      line = (uint32_t)next_line;
    } else if (opcode == 4) {
      if (!read_uleb32(&unit, &file))
        goto malformed;
    } else if (opcode == 5) {
      if (!read_uleb32(&unit, &column))
        goto malformed;
    } else if (opcode == 8) {
      uint32_t advance = (uint32_t)((255u - opcode_base) / line_range) * minimum_instruction_length;
      if (address_valid && advance > UINT32_MAX - address)
        address_valid = false;
      else if (address_valid)
        address += advance;
    } else if (opcode == 9) {
      uint16_t advance;
      if (!read_u16(&unit, &advance))
        goto malformed;
      if (address_valid && advance > UINT32_MAX - address)
        address_valid = false;
      else if (address_valid)
        address += advance;
    } else {
      uint8_t operand_count = standard_lengths[opcode];
      while (operand_count-- != 0) {
        uint32_t ignored;
        if (!read_uleb32(&unit, &ignored))
          goto malformed;
      }
    }
  }
  success = true;
  goto done;

malformed:
  success = set_line_error(info, "malformed or unsupported DWARF v2-v4 line program", diagnostics);
  goto done;
memory_failure:
  diagnostics_error(diagnostics, "out of memory decoding DWARF line information");
done:
  free(standard_lengths);
  dispose_strings(directories, directory_count);
  for (index = 0; index < file_count; ++index)
    free(files[index].path);
  free(files);
  return success;
}

/* Parses every line unit while keeping unsupported DWARF nonfatal to translation. */
static bool parse_line_section(const unsigned char *bytes, size_t size, DwarfRow **rows, size_t *row_count,
                               WasmDwarfInfo *info, Diagnostics *diagnostics) {
  Reader section = {bytes, size, 0};
  if (info->line_table_present && !info->line_table_supported)
    return true;
  info->line_table_present = true;
  info->line_table_supported = true;
  while (section.offset < section.size) {
    if (!parse_line_unit(&section, rows, row_count, info, diagnostics))
      return false;
    if (!info->line_table_supported)
      break;
  }
  return true;
}

/* Reads original defined-function spans from the Wasm code section. */
static bool read_function_spans(const unsigned char *bytes, size_t size, FunctionSpan **spans, size_t *span_count,
                                Diagnostics *diagnostics) {
  Reader module = {bytes, size, 8};
  while (module.offset < module.size) {
    uint8_t id;
    uint32_t section_size;
    size_t end;
    if (!read_u8(&module, &id) || !read_uleb32(&module, &section_size) || section_size > module.size - module.offset) {
      diagnostics_error(diagnostics, "malformed Wasm section while locating DWARF function ranges");
      return false;
    }
    end = module.offset + section_size;
    if (id == 10) {
      Reader code = {module.bytes + module.offset, section_size, 0};
      uint32_t count, index;
      if (!read_uleb32(&code, &count))
        goto malformed_code;
      *spans = calloc(count, sizeof(**spans));
      if (count != 0 && *spans == NULL) {
        diagnostics_error(diagnostics, "out of memory recording Wasm function ranges");
        return false;
      }
      *span_count = count;
      for (index = 0; index < count; ++index) {
        uint32_t body_size;
        if (!read_uleb32(&code, &body_size) || body_size > code.size - code.offset)
          goto malformed_code;
        /* WebAssembly DWARF addresses are offsets within the code-section
         * payload, including its function-count prefix. */
        (*spans)[index].start = (uint32_t)code.offset;
        (*spans)[index].end = (*spans)[index].start + body_size;
        code.offset += body_size;
      }
      return true;
    }
    module.offset = end;
  }
  return true;

malformed_code:
  free(*spans);
  *spans = NULL;
  *span_count = 0;
  diagnostics_error(diagnostics, "malformed Wasm code section while locating DWARF function ranges");
  return false;
}

/* Chooses the earliest usable line row in each original function body. */
static bool associate_function_rows(WasmDwarfInfo *info, const FunctionSpan *spans, size_t span_count,
                                    const DwarfRow *rows, size_t row_count, Diagnostics *diagnostics) {
  size_t function_index;
  info->functions = calloc(span_count, sizeof(*info->functions));
  if (span_count != 0 && info->functions == NULL) {
    diagnostics_error(diagnostics, "out of memory associating DWARF functions");
    return false;
  }
  info->function_count = span_count;
  for (function_index = 0; function_index < span_count; ++function_index) {
    const DwarfRow *best = NULL;
    size_t row_index;
    info->functions[function_index].defined_function_index = function_index;
    for (row_index = 0; row_index < row_count; ++row_index) {
      const DwarfRow *row = &rows[row_index];
      if (!row->end_sequence && row->line != 0 && row->address >= spans[function_index].start &&
          row->address < spans[function_index].end && (best == NULL || row->address < best->address))
        best = row;
    }
    if (best != NULL) {
      info->functions[function_index].file = copy_string(best->file);
      if (info->functions[function_index].file == NULL) {
        diagnostics_error(diagnostics, "out of memory associating DWARF source paths");
        return false;
      }
      info->functions[function_index].line = best->line;
      info->functions[function_index].column = best->column;
    }
  }
  return true;
}

/* Builds a Binaryen-safe module view and independently extracts line metadata. */
bool wasm_dwarf_prepare_input(const char *contents, size_t size, char **filtered, size_t *filtered_size,
                              WasmDwarfInfo *info, Diagnostics *diagnostics) {
  const unsigned char *bytes = (const unsigned char *)contents;
  Reader module = {bytes, size, 8};
  unsigned char *output;
  size_t output_size = 8;
  DwarfRow *rows = NULL;
  size_t row_count = 0;
  FunctionSpan *spans = NULL;
  size_t span_count = 0;
  bool success = false;

  memset(info, 0, sizeof(*info));
  *filtered = NULL;
  *filtered_size = 0;
  if (size < 8 || memcmp(bytes, "\0asm\1\0\0\0", 8) != 0) {
    diagnostics_error(diagnostics, "input has no valid Wasm header while scanning custom sections");
    return false;
  }
  output = malloc(size == 0 ? 1 : size);
  if (output == NULL) {
    diagnostics_error(diagnostics, "out of memory filtering Wasm debug sections");
    return false;
  }
  memcpy(output, bytes, 8);
  while (module.offset < module.size) {
    size_t section_start = module.offset;
    uint8_t id;
    uint32_t section_size;
    size_t section_end;
    bool omit = false;
    if (!read_u8(&module, &id) || !read_uleb32(&module, &section_size) || section_size > module.size - module.offset) {
      diagnostics_error(diagnostics, "malformed Wasm section while scanning custom sections");
      goto done;
    }
    section_end = module.offset + section_size;
    if (id == 0) {
      Reader custom = {module.bytes + module.offset, section_size, 0};
      uint32_t name_length;
      if (read_uleb32(&custom, &name_length) && name_length <= custom.size - custom.offset) {
        char *name = malloc((size_t)name_length + 1);
        if (name == NULL) {
          diagnostics_error(diagnostics, "out of memory scanning Wasm custom-section name");
          goto done;
        }
        memcpy(name, custom.bytes + custom.offset, name_length);
        name[name_length] = '\0';
        custom.offset += name_length;
        omit = is_dwarf_section_name(name);
        if (omit && !append_section_name(info, name, diagnostics)) {
          free(name);
          goto done;
        }
        if (strcmp(name, ".debug_line") == 0 &&
            !parse_line_section(custom.bytes + custom.offset, custom.size - custom.offset, &rows, &row_count, info,
                                diagnostics)) {
          free(name);
          goto done;
        }
        if (strcmp(name, ".zdebug_line") == 0) {
          info->line_table_present = true;
          if (!set_line_error(info, "compressed .zdebug_line data is accepted but not decoded", diagnostics)) {
            free(name);
            goto done;
          }
        }
        free(name);
      }
    }
    if (!omit) {
      size_t section_bytes = section_end - section_start;
      memcpy(output + output_size, bytes + section_start, section_bytes);
      output_size += section_bytes;
    }
    module.offset = section_end;
  }
  if (info->line_table_supported) {
    if (!read_function_spans(bytes, size, &spans, &span_count, diagnostics) ||
        !associate_function_rows(info, spans, span_count, rows, row_count, diagnostics))
      goto done;
  }
  *filtered = (char *)output;
  *filtered_size = output_size;
  output = NULL;
  success = true;
done:
  free(output);
  free(spans);
  dispose_rows(rows, row_count);
  if (!success)
    wasm_dwarf_dispose(info);
  return success;
}

/* Records a Binaryen-retained internal name for one original defined function. */
bool wasm_dwarf_set_function_name(WasmDwarfInfo *info, size_t defined_index, const char *name,
                                  Diagnostics *diagnostics) {
  char *copy;
  if (defined_index >= info->function_count)
    return true;
  copy = copy_string(name);
  if (copy == NULL) {
    diagnostics_error(diagnostics, "out of memory recording DWARF function identity");
    return false;
  }
  free(info->functions[defined_index].function_name);
  info->functions[defined_index].function_name = copy;
  return true;
}

/* Finds the best source record after normalization may have removed functions. */
const WasmDwarfFunctionLocation *wasm_dwarf_find_function(const WasmDwarfInfo *info, const char *name,
                                                          size_t defined_index) {
  size_t index;
  if (name != NULL)
    for (index = 0; index < info->function_count; ++index)
      if (info->functions[index].function_name != NULL && strcmp(info->functions[index].function_name, name) == 0)
        return &info->functions[index];
  return defined_index < info->function_count ? &info->functions[defined_index] : NULL;
}

/* Releases all retained DWARF metadata. */
void wasm_dwarf_dispose(WasmDwarfInfo *info) {
  size_t index;
  for (index = 0; index < info->section_count; ++index)
    free(info->section_names[index]);
  for (index = 0; index < info->function_count; ++index) {
    free(info->functions[index].function_name);
    free(info->functions[index].file);
  }
  free(info->section_names);
  free(info->functions);
  free(info->line_error);
  memset(info, 0, sizeof(*info));
}
