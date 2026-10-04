/* Appends deterministic DWARF v4 custom sections to a Wasm test module. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Growable byte buffer used to construct section payloads. */
typedef struct Buffer {
  unsigned char *bytes;
  size_t size, capacity;
} Buffer;

/* Ensures enough capacity for one bounded append. */
static int reserve(Buffer *buffer, size_t extra) {
  size_t capacity;
  unsigned char *bytes;
  if (extra > SIZE_MAX - buffer->size)
    return 0;
  if (buffer->size + extra <= buffer->capacity)
    return 1;
  capacity = buffer->capacity == 0 ? 128 : buffer->capacity;
  while (capacity < buffer->size + extra) {
    if (capacity > SIZE_MAX / 2)
      return 0;
    capacity *= 2;
  }
  bytes = realloc(buffer->bytes, capacity);
  if (bytes == NULL)
    return 0;
  buffer->bytes = bytes;
  buffer->capacity = capacity;
  return 1;
}

/* Appends one byte. */
static int append_u8(Buffer *buffer, uint8_t value) {
  if (!reserve(buffer, 1))
    return 0;
  buffer->bytes[buffer->size++] = value;
  return 1;
}

/* Appends a bounded byte sequence. */
static int append_bytes(Buffer *buffer, const void *bytes, size_t size) {
  if (!reserve(buffer, size))
    return 0;
  memcpy(buffer->bytes + buffer->size, bytes, size);
  buffer->size += size;
  return 1;
}

/* Appends an unsigned LEB32 value. */
static int append_uleb(Buffer *buffer, uint32_t value) {
  do {
    uint8_t byte = (uint8_t)(value & 0x7fu);
    value >>= 7;
    if (value != 0)
      byte |= 0x80u;
    if (!append_u8(buffer, byte))
      return 0;
  } while (value != 0);
  return 1;
}

/* Appends one little-endian 16-bit value. */
static int append_u16(Buffer *buffer, uint16_t value) {
  return append_u8(buffer, (uint8_t)value) && append_u8(buffer, (uint8_t)(value >> 8));
}

/* Appends one little-endian 32-bit value. */
static int append_u32(Buffer *buffer, uint32_t value) {
  return append_u8(buffer, (uint8_t)value) && append_u8(buffer, (uint8_t)(value >> 8)) &&
         append_u8(buffer, (uint8_t)(value >> 16)) && append_u8(buffer, (uint8_t)(value >> 24));
}

/* Patches one previously reserved little-endian 32-bit field. */
static void patch_u32(Buffer *buffer, size_t offset, uint32_t value) {
  buffer->bytes[offset] = (uint8_t)value;
  buffer->bytes[offset + 1] = (uint8_t)(value >> 8);
  buffer->bytes[offset + 2] = (uint8_t)(value >> 16);
  buffer->bytes[offset + 3] = (uint8_t)(value >> 24);
}

/* Appends one Wasm custom section with its encoded name. */
static int append_custom_section(Buffer *module, const char *name, const unsigned char *payload, size_t size) {
  Buffer section = {0};
  size_t name_length = strlen(name);
  int success = append_uleb(&section, (uint32_t)name_length) && append_bytes(&section, name, name_length) &&
                append_bytes(&section, payload, size) && append_u8(module, 0) &&
                append_uleb(module, (uint32_t)section.size) && append_bytes(module, section.bytes, section.size);
  free(section.bytes);
  return success;
}

/* Constructs a v4 line program with define_file and an unknown extension. */
static int build_debug_line(Buffer *line) {
  static const uint8_t standard_lengths[12] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};
  static const char filename[] = "fixture.zig";
  size_t unit_length_offset, header_length_offset, header_start;

  unit_length_offset = line->size;
  if (!append_u32(line, 0) || !append_u16(line, 4))
    return 0;
  header_length_offset = line->size;
  if (!append_u32(line, 0))
    return 0;
  header_start = line->size;
  if (!append_u8(line, 1) || !append_u8(line, 1) || !append_u8(line, 1) || !append_u8(line, (uint8_t)-5) ||
      !append_u8(line, 14) || !append_u8(line, 13) ||
      !append_bytes(line, standard_lengths, sizeof(standard_lengths)) || !append_u8(line, 0) || !append_u8(line, 0))
    return 0;
  patch_u32(line, header_length_offset, (uint32_t)(line->size - header_start));

  /* DW_LNE_define_file defines file 1 after the initially empty file table. */
  if (!append_u8(line, 0) || !append_uleb(line, 16) || !append_u8(line, 3) ||
      !append_bytes(line, filename, sizeof(filename)) || !append_u8(line, 0) || !append_u8(line, 0) ||
      !append_u8(line, 0) || !append_u8(line, 4) || !append_u8(line, 1) || !append_u8(line, 3) ||
      !append_u8(line, 36))
    return 0;
  /* Unknown extended opcodes must be skipped according to their payload size. */
  if (!append_u8(line, 0) || !append_u8(line, 2) || !append_u8(line, 0x7f) || !append_u8(line, 0x55))
    return 0;
  /* Address 2 is the first body payload after count and body-size prefixes. */
  if (!append_u8(line, 0) || !append_u8(line, 5) || !append_u8(line, 2) || !append_u32(line, 2) ||
      !append_u8(line, 1) || !append_u8(line, 0) || !append_u8(line, 1) || !append_u8(line, 1))
    return 0;
  patch_u32(line, unit_length_offset, (uint32_t)(line->size - unit_length_offset - 4));
  return 1;
}

/* Reads the complete base module into owned memory. */
static int read_file(const char *path, Buffer *buffer) {
  FILE *file = fopen(path, "rb");
  long length;
  if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
      fseek(file, 0, SEEK_SET) != 0 || !reserve(buffer, (size_t)length)) {
    if (file != NULL)
      fclose(file);
    return 0;
  }
  buffer->size = (size_t)length;
  if (fread(buffer->bytes, 1, buffer->size, file) != buffer->size) {
    fclose(file);
    return 0;
  }
  return fclose(file) == 0;
}

/* Writes the augmented module used by CLI regression tests. */
int main(int argc, char **argv) {
  static const unsigned char debug_info[] = {4, 0, 0, 0};
  static const unsigned char relocation[] = {0, 0};
  Buffer module = {0}, line = {0};
  FILE *output;
  int status = 1;

  if (argc != 3 || !read_file(argv[1], &module) || !build_debug_line(&line) ||
      !append_custom_section(&module, ".debug_info", debug_info, sizeof(debug_info)) ||
      !append_custom_section(&module, ".debug_line", line.bytes, line.size) ||
      !append_custom_section(&module, "reloc..debug_line", relocation, sizeof(relocation)))
    goto done;
  output = fopen(argv[2], "wb");
  if (output == NULL)
    goto done;
  if (fwrite(module.bytes, 1, module.size, output) == module.size)
    status = 0;
  if (fclose(output) != 0)
    status = 1;
done:
  free(module.bytes);
  free(line.bytes);
  return status;
}
