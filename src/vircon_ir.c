/* Structured V32 IR storage plus strict decoding of compiler-generated operations. */

#include "vircon_ir.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Legacy selector spellings accepted only by the strict lowering adapter. */
static const char *const GENERATED_OPCODE_NAMES[VIRCON_IR_OPCODE_COUNT] = {
    "mov",  "push", "pop",   "call", "ret",   "hlt",  "jmp",  "jt",   "jf",   "wait", "iadd",
    "isub", "imul", "idiv",  "imod", "and",   "or",   "xor",  "shl",  "ieq",  "ine",  "ilt",
    "ile",  "igt",  "ige",   "fadd", "fsub",  "fmul", "fdiv", "fmod", "feq",  "fne",  "flt",
    "fle",  "fgt",  "fge",   "cif",  "cfi",   "fsgn", "fabs", "flr",  "ceil", "round", "sin",
    "acos", "log",  "pow",   "atan2", "imin", "imax", "iabs", "fmin", "fmax", "in",    "out"};

/* Copies one null-terminated identifier into IR-owned storage. */
static char *copy_string(const char *text) {
  size_t size;
  char *copy;
  if (text == NULL)
    return NULL;
  size = strlen(text) + 1;
  copy = malloc(size);
  if (copy != NULL)
    memcpy(copy, text, size);
  return copy;
}

/* Removes leading and trailing ASCII whitespace in place. */
static char *trim(char *text) {
  char *end;
  while (isspace((unsigned char)*text))
    ++text;
  end = text + strlen(text);
  while (end != text && isspace((unsigned char)end[-1]))
    --end;
  *end = '\0';
  return text;
}

/* Releases heap fields owned by one node without touching program storage. */
static void dispose_node(VirconIrNode *node) {
  size_t index;
  free(node->name);
  free(node->comment);
  for (index = 0; index < node->operand_count; ++index)
    free(node->operands[index].symbol);
  memset(node, 0, sizeof(*node));
}

/* Moves one fully owned node into the program's growable sequence. */
static bool append_node(VirconIrProgram *program, VirconIrNode *node, Diagnostics *diagnostics) {
  VirconIrNode *nodes;
  size_t capacity;
  if (program->count == program->capacity) {
    capacity = program->capacity == 0 ? 32 : program->capacity * 2;
    if (capacity < program->capacity || capacity > SIZE_MAX / sizeof(*nodes)) {
      diagnostics_error(diagnostics, "V32 IR is too large to represent");
      dispose_node(node);
      return false;
    }
    nodes = realloc(program->nodes, capacity * sizeof(*nodes));
    if (nodes == NULL) {
      diagnostics_error(diagnostics, "out of memory while building V32 IR");
      dispose_node(node);
      return false;
    }
    program->nodes = nodes;
    program->capacity = capacity;
  }
  program->nodes[program->count++] = *node;
  memset(node, 0, sizeof(*node));
  return true;
}

/* Converts one canonical register spelling into its enum value. */
static bool parse_register(const char *text, VirconIrRegister *reg) {
  char *end;
  long index;
  if (strcmp(text, "SP") == 0) {
    *reg = VIRCON_IR_REGISTER_SP;
    return true;
  }
  if (strcmp(text, "BP") == 0) {
    *reg = VIRCON_IR_REGISTER_BP;
    return true;
  }
  if (text[0] != 'R' || !isdigit((unsigned char)text[1]))
    return false;
  errno = 0;
  index = strtol(text + 1, &end, 10);
  if (errno != 0 || *end != '\0' || index < 0 || index > 15)
    return false;
  *reg = (VirconIrRegister)(VIRCON_IR_REGISTER_R0 + index);
  return true;
}

/* Parses a signed, unsigned, or hexadecimal 32-bit immediate. */
static bool parse_immediate(const char *text, uint32_t *value, VirconIrIntegerFormat *format,
                            unsigned *hexadecimal_digits) {
  char *end;
  unsigned long long unsigned_value;
  long long signed_value;
  errno = 0;
  if (text[0] == '-' || text[0] == '+') {
    signed_value = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0' || signed_value < INT32_MIN || signed_value > INT32_MAX)
      return false;
    *value = (uint32_t)(int32_t)signed_value;
    *format = VIRCON_IR_INTEGER_SIGNED_DECIMAL;
    *hexadecimal_digits = 0;
    return true;
  }
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    unsigned_value = strtoull(text + 2, &end, 16);
    if (errno != 0 || *end != '\0' || end == text + 2 || unsigned_value > UINT32_MAX)
      return false;
    *value = (uint32_t)unsigned_value;
    *format = VIRCON_IR_INTEGER_HEXADECIMAL;
    *hexadecimal_digits = (unsigned)strlen(text + 2);
    return true;
  }
  unsigned_value = strtoull(text, &end, 10);
  if (errno != 0 || *end != '\0' || end == text || unsigned_value > UINT32_MAX)
    return false;
  *value = (uint32_t)unsigned_value;
  *format = VIRCON_IR_INTEGER_UNSIGNED_DECIMAL;
  *hexadecimal_digits = 0;
  return true;
}

/* Parses a register-relative or absolute memory address without brackets. */
static bool parse_memory_operand(char *text, VirconIrOperand *operand) {
  VirconIrRegister reg;
  char *sign = NULL;
  uint32_t displacement;
  VirconIrIntegerFormat format;
  unsigned hexadecimal_digits;
  char *cursor;

  if (parse_register(text, &reg)) {
    operand->kind = VIRCON_IR_OPERAND_MEMORY_REGISTER;
    operand->reg = reg;
    return true;
  }
  for (cursor = text + 1; *cursor != '\0'; ++cursor)
    if (*cursor == '+' || *cursor == '-') {
      sign = cursor;
      break;
    }
  if (sign != NULL) {
    int32_t signed_displacement;
    char sign_character = *sign;
    *sign = '\0';
    if (!parse_register(text, &reg) || !parse_immediate(sign + 1, &displacement, &format, &hexadecimal_digits) ||
        format == VIRCON_IR_INTEGER_HEXADECIMAL || displacement > INT32_MAX)
      return false;
    signed_displacement = (int32_t)displacement;
    operand->kind = VIRCON_IR_OPERAND_MEMORY_REGISTER;
    operand->reg = reg;
    operand->displacement = sign_character == '-' ? -signed_displacement : signed_displacement;
    operand->has_displacement = true;
    return true;
  }
  if (!parse_immediate(text, &operand->immediate, &operand->integer_format, &operand->hexadecimal_digits))
    return false;
  operand->kind = VIRCON_IR_OPERAND_MEMORY_ABSOLUTE;
  return true;
}

/* Parses one typed register, constant, symbol, or memory operand. */
static bool parse_operand(char *text, VirconIrOperand *operand) {
  size_t length;
  text = trim(text);
  length = strlen(text);
  if (length >= 2 && text[0] == '[' && text[length - 1] == ']') {
    text[length - 1] = '\0';
    return parse_memory_operand(trim(text + 1), operand);
  }
  if (parse_register(text, &operand->reg)) {
    operand->kind = VIRCON_IR_OPERAND_REGISTER;
    return true;
  }
  if (parse_immediate(text, &operand->immediate, &operand->integer_format, &operand->hexadecimal_digits)) {
    operand->kind = VIRCON_IR_OPERAND_IMMEDIATE;
    return true;
  }
  if (*text == '\0')
    return false;
  operand->kind = VIRCON_IR_OPERAND_SYMBOL;
  operand->symbol = copy_string(text);
  return operand->symbol != NULL;
}

/* Finds a supported opcode by its canonical assembler spelling. */
static bool parse_opcode(const char *name, VirconIrOpcode *opcode) {
  size_t index;
  for (index = 0; index < VIRCON_IR_OPCODE_COUNT; ++index)
    if (strcmp(name, GENERATED_OPCODE_NAMES[index]) == 0) {
      *opcode = (VirconIrOpcode)index;
      return true;
    }
  return false;
}

/* Initializes an empty program. */
void vircon_ir_init(VirconIrProgram *program) { memset(program, 0, sizeof(*program)); }

/* Releases every structured node and the program's backing allocation. */
void vircon_ir_dispose(VirconIrProgram *program) {
  size_t index;
  for (index = 0; index < program->count; ++index)
    dispose_node(&program->nodes[index]);
  free(program->nodes);
  memset(program, 0, sizeof(*program));
}

/* Copies one structured instruction so callers retain ownership of operands. */
bool vircon_ir_append_instruction(VirconIrProgram *program, VirconIrOpcode opcode, const VirconIrOperand *operands,
                                  size_t operand_count, const char *comment, Diagnostics *diagnostics) {
  VirconIrNode node = {0};
  size_t index;

  if (opcode >= VIRCON_IR_OPCODE_COUNT || operand_count > 2 || (operand_count != 0 && operands == NULL)) {
    diagnostics_error(diagnostics, "internal error: invalid structured V32 instruction");
    return false;
  }
  node.kind = VIRCON_IR_NODE_INSTRUCTION;
  node.opcode = opcode;
  node.operand_count = operand_count;
  for (index = 0; index < operand_count; ++index) {
    node.operands[index] = operands[index];
    node.operands[index].symbol = NULL;
    if (operands[index].kind == VIRCON_IR_OPERAND_SYMBOL) {
      node.operands[index].symbol = copy_string(operands[index].symbol);
      if (node.operands[index].symbol == NULL)
        goto out_of_memory;
    }
  }
  if (comment != NULL) {
    node.comment = copy_string(comment);
    if (node.comment == NULL)
      goto out_of_memory;
  }
  return append_node(program, &node, diagnostics);

out_of_memory:
  diagnostics_error(diagnostics, "out of memory recording structured V32 instruction");
  dispose_node(&node);
  return false;
}

/* Appends one structured label. */
bool vircon_ir_append_label(VirconIrProgram *program, const char *name, Diagnostics *diagnostics) {
  VirconIrNode node = {0};
  node.kind = VIRCON_IR_NODE_LABEL;
  node.name = copy_string(name);
  if (node.name == NULL) {
    diagnostics_error(diagnostics, "out of memory recording V32 IR label");
    return false;
  }
  return append_node(program, &node, diagnostics);
}

/* Appends one structured immutable pointer-data entry. */
bool vircon_ir_append_pointer(VirconIrProgram *program, const char *target, Diagnostics *diagnostics) {
  VirconIrNode node = {0};
  node.kind = VIRCON_IR_NODE_POINTER;
  node.name = copy_string(target);
  if (node.name == NULL) {
    diagnostics_error(diagnostics, "out of memory recording V32 IR pointer");
    return false;
  }
  return append_node(program, &node, diagnostics);
}

/* Strictly converts one compiler-generated assembly-shaped operation into IR. */
bool vircon_ir_append_generated_instruction(VirconIrProgram *program, char *text, Diagnostics *diagnostics) {
  VirconIrNode node = {0};
  char *operation = trim(text);
  char *comment = strchr(operation, ';');
  char *operands;
  char *comma;

  node.kind = VIRCON_IR_NODE_INSTRUCTION;
  if (comment != NULL) {
    *comment++ = '\0';
    comment = trim(comment);
    node.comment = copy_string(comment);
    if (node.comment == NULL)
      goto out_of_memory;
  }
  operation = trim(operation);
  operands = operation;
  while (*operands != '\0' && !isspace((unsigned char)*operands))
    ++operands;
  if (*operands != '\0')
    *operands++ = '\0';
  operands = trim(operands);
  if (!parse_opcode(operation, &node.opcode)) {
    diagnostics_error(diagnostics, "internal error: unsupported V32 IR opcode '%s'", operation);
    goto fail;
  }
  if (*operands != '\0') {
    comma = strchr(operands, ',');
    if (comma != NULL)
      *comma++ = '\0';
    if (!parse_operand(operands, &node.operands[0])) {
      diagnostics_error(diagnostics, "internal error: invalid first operand for V32 opcode '%s'", operation);
      goto fail;
    }
    node.operand_count = 1;
    if (comma != NULL) {
      if (strchr(comma, ',') != NULL || !parse_operand(comma, &node.operands[1])) {
        diagnostics_error(diagnostics, "internal error: invalid second operand for V32 opcode '%s'", operation);
        goto fail;
      }
      node.operand_count = 2;
    }
  }
  free(text);
  return append_node(program, &node, diagnostics);

out_of_memory:
  diagnostics_error(diagnostics, "out of memory decoding compiler-generated V32 IR");
fail:
  dispose_node(&node);
  free(text);
  return false;
}
