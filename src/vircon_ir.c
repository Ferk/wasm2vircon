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
    "acos", "log",  "pow",   "atan2", "imin", "imax", "iabs", "fmin", "fmax", "in",    "out",
    "sets"};

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

/* Returns whether an operand names a compiler frame word through BP. */
static bool frame_slot_operand(const VirconIrOperand *operand, int32_t *slot) {
  if (operand->kind != VIRCON_IR_OPERAND_MEMORY_REGISTER || operand->reg != VIRCON_IR_REGISTER_BP)
    return false;
  *slot = operand->has_displacement ? operand->displacement : 0;
  return true;
}

/* Returns whether an opcode replaces the value of its first register operand. */
static bool opcode_writes_first_register(VirconIrOpcode opcode) {
  switch (opcode) {
  case VIRCON_IR_OPCODE_MOV:
  case VIRCON_IR_OPCODE_POP:
  case VIRCON_IR_OPCODE_IADD:
  case VIRCON_IR_OPCODE_ISUB:
  case VIRCON_IR_OPCODE_IMUL:
  case VIRCON_IR_OPCODE_IDIV:
  case VIRCON_IR_OPCODE_IMOD:
  case VIRCON_IR_OPCODE_AND:
  case VIRCON_IR_OPCODE_OR:
  case VIRCON_IR_OPCODE_XOR:
  case VIRCON_IR_OPCODE_SHL:
  case VIRCON_IR_OPCODE_IEQ:
  case VIRCON_IR_OPCODE_INE:
  case VIRCON_IR_OPCODE_ILT:
  case VIRCON_IR_OPCODE_ILE:
  case VIRCON_IR_OPCODE_IGT:
  case VIRCON_IR_OPCODE_IGE:
  case VIRCON_IR_OPCODE_FADD:
  case VIRCON_IR_OPCODE_FSUB:
  case VIRCON_IR_OPCODE_FMUL:
  case VIRCON_IR_OPCODE_FDIV:
  case VIRCON_IR_OPCODE_FMOD:
  case VIRCON_IR_OPCODE_FEQ:
  case VIRCON_IR_OPCODE_FNE:
  case VIRCON_IR_OPCODE_FLT:
  case VIRCON_IR_OPCODE_FLE:
  case VIRCON_IR_OPCODE_FGT:
  case VIRCON_IR_OPCODE_FGE:
  case VIRCON_IR_OPCODE_CIF:
  case VIRCON_IR_OPCODE_CFI:
  case VIRCON_IR_OPCODE_FSGN:
  case VIRCON_IR_OPCODE_FABS:
  case VIRCON_IR_OPCODE_FLR:
  case VIRCON_IR_OPCODE_CEIL:
  case VIRCON_IR_OPCODE_ROUND:
  case VIRCON_IR_OPCODE_SIN:
  case VIRCON_IR_OPCODE_ACOS:
  case VIRCON_IR_OPCODE_LOG:
  case VIRCON_IR_OPCODE_POW:
  case VIRCON_IR_OPCODE_ATAN2:
  case VIRCON_IR_OPCODE_IMIN:
  case VIRCON_IR_OPCODE_IMAX:
  case VIRCON_IR_OPCODE_IABS:
  case VIRCON_IR_OPCODE_FMIN:
  case VIRCON_IR_OPCODE_FMAX:
  case VIRCON_IR_OPCODE_IN:
    return true;
  default:
    return false;
  }
}

/* One symbolic value currently stored in a compiler frame word. */
typedef struct FrameValueFact {
  int32_t slot;
  uint64_t value;
  bool constant_known;
  uint32_t constant;
} FrameValueFact;

/* Finds or creates the symbolic-value entry for one BP-relative frame word. */
static FrameValueFact *frame_value_fact(FrameValueFact *facts, size_t *count, int32_t slot) {
  size_t index;
  for (index = 0; index < *count; ++index)
    if (facts[index].slot == slot)
      return &facts[index];
  if (*count == 128)
    return NULL;
  facts[*count] = (FrameValueFact){.slot = slot};
  return &facts[(*count)++];
}

/* Invalidates symbolic and constant values at a control-flow join or unknown store. */
static void clear_value_facts(uint64_t *register_values, bool *register_constant_known,
                              size_t *frame_value_count) {
  memset(register_values, 0, sizeof(uint64_t) * (VIRCON_IR_REGISTER_BP + 1));
  memset(register_constant_known, 0, sizeof(bool) * (VIRCON_IR_REGISTER_BP + 1));
  *frame_value_count = 0;
}

/* Returns whether a register is one of the sixteen general target registers. */
static bool is_general_register(VirconIrRegister reg) {
  return reg >= VIRCON_IR_REGISTER_R0 && reg <= VIRCON_IR_REGISTER_R15;
}

/* Interprets one target word as signed two's-complement without relying on an
 * implementation-defined unsigned-to-signed conversion. */
static int32_t signed_word(uint32_t value) {
  int32_t result;
  memcpy(&result, &value, sizeof(result));
  return result;
}

/* Returns a known immediate or register value for local constant folding. */
static bool constant_operand_value(const VirconIrOperand *operand, const bool *register_constant_known,
                                   const uint32_t *register_constants, uint32_t *value) {
  if (operand->kind == VIRCON_IR_OPERAND_IMMEDIATE) {
    *value = operand->immediate;
    return true;
  }
  if (operand->kind == VIRCON_IR_OPERAND_REGISTER && is_general_register(operand->reg) &&
      register_constant_known[operand->reg]) {
    *value = register_constants[operand->reg];
    return true;
  }
  return false;
}

/* Evaluates target integer operations whose 32-bit behavior is independent
 * of traps, floating-point state, and source-language undefined behavior. */
static bool fold_integer_operation(VirconIrOpcode opcode, uint32_t left, uint32_t right, uint32_t *result,
                                   bool *comparison) {
  *comparison = false;
  switch (opcode) {
  case VIRCON_IR_OPCODE_IADD:
    *result = left + right;
    return true;
  case VIRCON_IR_OPCODE_ISUB:
    *result = left - right;
    return true;
  case VIRCON_IR_OPCODE_IMUL:
    *result = left * right;
    return true;
  case VIRCON_IR_OPCODE_AND:
    *result = left & right;
    return true;
  case VIRCON_IR_OPCODE_OR:
    *result = left | right;
    return true;
  case VIRCON_IR_OPCODE_XOR:
    *result = left ^ right;
    return true;
  case VIRCON_IR_OPCODE_IEQ:
    *result = left == right;
    *comparison = true;
    return true;
  case VIRCON_IR_OPCODE_INE:
    *result = left != right;
    *comparison = true;
    return true;
  case VIRCON_IR_OPCODE_ILT:
    *result = signed_word(left) < signed_word(right);
    *comparison = true;
    return true;
  case VIRCON_IR_OPCODE_ILE:
    *result = signed_word(left) <= signed_word(right);
    *comparison = true;
    return true;
  case VIRCON_IR_OPCODE_IGT:
    *result = signed_word(left) > signed_word(right);
    *comparison = true;
    return true;
  case VIRCON_IR_OPCODE_IGE:
    *result = signed_word(left) >= signed_word(right);
    *comparison = true;
    return true;
  default:
    return false;
  }
}

/* Replaces one folded operation with a typed constant move. */
static void rewrite_as_constant_move(VirconIrNode *node, uint32_t value, bool comparison) {
  VirconIrOperand *source = &node->operands[1];
  free(source->symbol);
  memset(source, 0, sizeof(*source));
  source->kind = VIRCON_IR_OPERAND_IMMEDIATE;
  source->immediate = value;
  source->integer_format = comparison ? VIRCON_IR_INTEGER_UNSIGNED_DECIMAL : VIRCON_IR_INTEGER_HEXADECIMAL;
  source->hexadecimal_digits = comparison ? 0 : 8;
  node->opcode = VIRCON_IR_OPCODE_MOV;
}

/* Simplifies frame copies and constant integer expressions that are provably
 * local to one straight-line region. Labels and calls deliberately end the
 * region; this pass is not a register allocator and never carries facts across
 * a control-flow join. */
void vircon_ir_optimize_local(VirconIrProgram *program) {
  uint64_t register_values[VIRCON_IR_REGISTER_BP + 1] = {0};
  bool register_constant_known[VIRCON_IR_REGISTER_BP + 1] = {0};
  uint32_t register_constants[VIRCON_IR_REGISTER_BP + 1] = {0};
  FrameValueFact frame_values[128] = {{0}};
  size_t frame_value_count = 0;
  uint64_t next_value = 1;
  size_t read_index, write_index = 0;

  for (read_index = 0; read_index < program->count; ++read_index) {
    VirconIrNode *node = &program->nodes[read_index];
    bool remove = false;

    if (node->kind != VIRCON_IR_NODE_INSTRUCTION) {
      clear_value_facts(register_values, register_constant_known, &frame_value_count);
    } else if (node->operand_count == 2 && node->operands[0].kind == VIRCON_IR_OPERAND_REGISTER &&
               is_general_register(node->operands[0].reg) &&
               register_constant_known[node->operands[0].reg]) {
      VirconIrRegister destination_reg = node->operands[0].reg;
      uint32_t right, result;
      bool comparison;

      if (constant_operand_value(&node->operands[1], register_constant_known, register_constants, &right) &&
          fold_integer_operation(node->opcode, register_constants[destination_reg], right, &result,
                                 &comparison)) {
        VirconIrNode *previous = write_index == 0 ? NULL : &program->nodes[write_index - 1];
        rewrite_as_constant_move(node, result, comparison);

        /* The common lowering shape materializes a constant immediately before
         * mutating it. Replace that pair with one move instead of retaining a
         * dead initial value. */
        if (previous != NULL && previous->kind == VIRCON_IR_NODE_INSTRUCTION &&
            previous->opcode == VIRCON_IR_OPCODE_MOV && previous->operand_count == 2 &&
            previous->comment == NULL && previous->operands[0].kind == VIRCON_IR_OPERAND_REGISTER &&
            previous->operands[0].reg == destination_reg) {
          dispose_node(previous);
          --write_index;
          register_values[destination_reg] = 0;
          register_constant_known[destination_reg] = false;
        }
      }
    }

    if (node->kind != VIRCON_IR_NODE_INSTRUCTION) {
      /* The control-flow boundary above has already invalidated all facts. */
    } else if (node->opcode == VIRCON_IR_OPCODE_MOV && node->operand_count == 2) {
      VirconIrOperand *destination = &node->operands[0];
      VirconIrOperand *source = &node->operands[1];
      int32_t slot;

      if (destination->kind == VIRCON_IR_OPERAND_REGISTER && frame_slot_operand(source, &slot)) {
        VirconIrRegister reg = destination->reg;
        FrameValueFact *fact = frame_value_fact(frame_values, &frame_value_count, slot);
        if (fact == NULL) {
          frame_value_count = 0;
          fact = frame_value_fact(frame_values, &frame_value_count, slot);
        }
        if (fact->value == 0)
          fact->value = next_value++;
        if (register_values[reg] == fact->value)
          remove = true;
        else
          register_values[reg] = fact->value;
        register_constant_known[reg] = is_general_register(reg) && fact->constant_known;
        register_constants[reg] = fact->constant;
      } else if (frame_slot_operand(destination, &slot) && source->kind == VIRCON_IR_OPERAND_REGISTER) {
        VirconIrRegister reg = source->reg;
        FrameValueFact *fact = frame_value_fact(frame_values, &frame_value_count, slot);
        if (fact == NULL) {
          frame_value_count = 0;
          fact = frame_value_fact(frame_values, &frame_value_count, slot);
        }
        if (register_values[reg] == 0)
          register_values[reg] = next_value++;
        if (fact->value == register_values[reg])
          remove = true;
        else
          fact->value = register_values[reg];
        fact->constant_known = register_constant_known[reg];
        fact->constant = register_constants[reg];
      } else if (destination->kind == VIRCON_IR_OPERAND_REGISTER && source->kind == VIRCON_IR_OPERAND_REGISTER) {
        VirconIrRegister destination_reg = destination->reg;
        VirconIrRegister source_reg = source->reg;
        if (destination_reg == source_reg)
          remove = true;
        else {
          if (register_values[source_reg] == 0)
            register_values[source_reg] = next_value++;
          register_values[destination_reg] = register_values[source_reg];
        }
        register_constant_known[destination_reg] = is_general_register(destination_reg) &&
                                                   is_general_register(source_reg) &&
                                                   register_constant_known[source_reg];
        register_constants[destination_reg] = register_constants[source_reg];
      } else if (destination->kind == VIRCON_IR_OPERAND_REGISTER &&
                 source->kind == VIRCON_IR_OPERAND_IMMEDIATE) {
        if (register_constant_known[destination->reg] && register_constants[destination->reg] == source->immediate)
          remove = true;
        else
          register_values[destination->reg] = next_value++;
        register_constant_known[destination->reg] = is_general_register(destination->reg);
        register_constants[destination->reg] = source->immediate;
      } else if (destination->kind == VIRCON_IR_OPERAND_REGISTER) {
        register_values[destination->reg] = next_value++;
        register_constant_known[destination->reg] = false;
      } else if (destination->kind == VIRCON_IR_OPERAND_MEMORY_REGISTER ||
                 destination->kind == VIRCON_IR_OPERAND_MEMORY_ABSOLUTE) {
        /* An indirect store could alias a compiler frame word. */
        clear_value_facts(register_values, register_constant_known, &frame_value_count);
      }
    } else {
      if (node->operand_count != 0 && node->operands[0].kind == VIRCON_IR_OPERAND_REGISTER &&
          opcode_writes_first_register(node->opcode)) {
        register_values[node->operands[0].reg] = next_value++;
        register_constant_known[node->operands[0].reg] = false;
      }
      if (node->opcode == VIRCON_IR_OPCODE_SETS)
        clear_value_facts(register_values, register_constant_known, &frame_value_count);
      if (node->opcode == VIRCON_IR_OPCODE_CALL || node->opcode == VIRCON_IR_OPCODE_JMP ||
          node->opcode == VIRCON_IR_OPCODE_RET || node->opcode == VIRCON_IR_OPCODE_HLT)
        clear_value_facts(register_values, register_constant_known, &frame_value_count);
    }

    if (remove) {
      dispose_node(node);
      continue;
    }
    if (write_index != read_index) {
      program->nodes[write_index] = *node;
      memset(node, 0, sizeof(*node));
    }
    ++write_index;
  }
  program->count = write_index;
}

/* A compact bit set used by the frame-slot liveness pass. The pass is kept
 * independent of Wasm and only reasons about compiler-owned BP-relative words. */
#define FRAME_LIVENESS_SLOT_LIMIT 512u

typedef struct FrameLivenessSet {
  uint64_t *words;
} FrameLivenessSet;

/* Sets one slot bit in a fixed-width liveness set. */
static void frame_liveness_set(FrameLivenessSet set, size_t index) {
  set.words[index / 64u] |= UINT64_C(1) << (index % 64u);
}

/* Returns whether one slot bit is present in a liveness set. */
static bool frame_liveness_contains(FrameLivenessSet set, size_t index) {
  return (set.words[index / 64u] & (UINT64_C(1) << (index % 64u))) != 0;
}

/* Unions one liveness set into another and reports whether it changed. */
static bool frame_liveness_union(FrameLivenessSet destination, FrameLivenessSet source, size_t word_count) {
  bool changed = false;
  size_t index;
  for (index = 0; index < word_count; ++index) {
    uint64_t before = destination.words[index];
    destination.words[index] |= source.words[index];
    changed |= before != destination.words[index];
  }
  return changed;
}

/* Finds one BP-relative word in the liveness slot table. */
static bool find_frame_liveness_slot(const int32_t *slots, size_t slot_count, int32_t slot, size_t *index) {
  size_t cursor;
  for (cursor = 0; cursor < slot_count; ++cursor)
    if (slots[cursor] == slot) {
      *index = cursor;
      return true;
    }
  return false;
}

/* Finds a named label node used as a branch target. */
static bool find_ir_label(const VirconIrProgram *program, const char *name, size_t *index) {
  size_t cursor;
  if (name == NULL)
    return false;
  for (cursor = 0; cursor < program->count; ++cursor)
    if (program->nodes[cursor].kind == VIRCON_IR_NODE_LABEL && strcmp(program->nodes[cursor].name, name) == 0) {
      *index = cursor;
      return true;
    }
  return false;
}

/* Records BP-relative reads and writes for one instruction. Unknown memory
 * operations are conservatively treated as reads, so they cannot make a dead
 * store disappear. */
static bool collect_frame_liveness_use_def(const VirconIrNode *node, const int32_t *slots, size_t slot_count,
                                           FrameLivenessSet use, FrameLivenessSet definition) {
  size_t operand_index, slot_index;
  if (node->kind != VIRCON_IR_NODE_INSTRUCTION)
    return true;
  for (operand_index = 0; operand_index < node->operand_count; ++operand_index) {
    const VirconIrOperand *operand = &node->operands[operand_index];
    if (operand->kind != VIRCON_IR_OPERAND_MEMORY_REGISTER || operand->reg != VIRCON_IR_REGISTER_BP)
      continue;
    if (!find_frame_liveness_slot(slots, slot_count,
                                  operand->has_displacement ? operand->displacement : 0, &slot_index))
      return false;
    if (node->opcode == VIRCON_IR_OPCODE_MOV && operand_index == 0)
      frame_liveness_set(definition, slot_index);
    else
      frame_liveness_set(use, slot_index);
  }
  return true;
}

/* Adds one CFG successor while rejecting malformed branch targets. */
static bool add_ir_successor(const VirconIrProgram *program, const VirconIrNode *node, size_t successor,
                             size_t *successors, size_t *count) {
  if (successor >= program->count || *count == 2)
    return false;
  successors[(*count)++] = successor;
  (void)node;
  return true;
}

/* Computes the at-most-two successors of one structured V32 instruction. */
static bool ir_successors(const VirconIrProgram *program, size_t index, size_t *successors, size_t *count) {
  const VirconIrNode *node = &program->nodes[index];
  size_t target;
  *count = 0;
  if (node->kind != VIRCON_IR_NODE_INSTRUCTION) {
    return index + 1 == program->count || add_ir_successor(program, node, index + 1, successors, count);
  }
  if (node->opcode == VIRCON_IR_OPCODE_RET || node->opcode == VIRCON_IR_OPCODE_HLT)
    return true;
  if (node->opcode == VIRCON_IR_OPCODE_JMP || node->opcode == VIRCON_IR_OPCODE_JT ||
      node->opcode == VIRCON_IR_OPCODE_JF) {
    if (node->operand_count == 0 || node->operands[0].kind != VIRCON_IR_OPERAND_SYMBOL ||
        !find_ir_label(program, node->operands[0].symbol, &target) ||
        !add_ir_successor(program, node, target, successors, count))
      return false;
    if ((node->opcode == VIRCON_IR_OPCODE_JT || node->opcode == VIRCON_IR_OPCODE_JF) &&
        index + 1 < program->count && !add_ir_successor(program, node, index + 1, successors, count))
      return false;
    return true;
  }
  return index + 1 == program->count || add_ir_successor(program, node, index + 1, successors, count);
}

/* Removes stores whose frame word is not live after the store. Liveness is
 * solved over the generated control-flow graph, so stores needed by a branch
 * target or a later function path remain intact. If the IR is malformed or a
 * slot table would be too large, this optimization simply declines to act. */
void vircon_ir_eliminate_dead_frame_stores(VirconIrProgram *program) {
  int32_t slots[FRAME_LIVENESS_SLOT_LIMIT];
  size_t slot_count = 0, word_count, node_count = program->count, index, operand_index;
  FrameLivenessSet *uses = NULL, *definitions = NULL, *live_ins = NULL, *live_outs = NULL;
  bool changed, valid = true;

  for (index = 0; index < node_count && valid; ++index) {
    const VirconIrNode *node = &program->nodes[index];
    if (node->kind != VIRCON_IR_NODE_INSTRUCTION)
      continue;
    for (operand_index = 0; operand_index < node->operand_count; ++operand_index) {
      const VirconIrOperand *operand = &node->operands[operand_index];
      int32_t slot;
      size_t slot_index;
      if (operand->kind != VIRCON_IR_OPERAND_MEMORY_REGISTER || operand->reg != VIRCON_IR_REGISTER_BP)
        continue;
      slot = operand->has_displacement ? operand->displacement : 0;
      if (!find_frame_liveness_slot(slots, slot_count, slot, &slot_index)) {
        if (slot_count == FRAME_LIVENESS_SLOT_LIMIT) {
          valid = false;
          break;
        }
        slots[slot_count++] = slot;
      }
    }
  }
  if (!valid || slot_count == 0)
    return;
  word_count = (slot_count + 63u) / 64u;
  uses = calloc(node_count, sizeof(*uses));
  definitions = calloc(node_count, sizeof(*definitions));
  live_ins = calloc(node_count, sizeof(*live_ins));
  live_outs = calloc(node_count, sizeof(*live_outs));
  if (uses == NULL || definitions == NULL || live_ins == NULL || live_outs == NULL)
    goto done;
  for (index = 0; index < node_count; ++index) {
    uses[index].words = calloc(word_count, sizeof(uint64_t));
    definitions[index].words = calloc(word_count, sizeof(uint64_t));
    live_ins[index].words = calloc(word_count, sizeof(uint64_t));
    live_outs[index].words = calloc(word_count, sizeof(uint64_t));
    if (uses[index].words == NULL || definitions[index].words == NULL || live_ins[index].words == NULL ||
        live_outs[index].words == NULL ||
        !collect_frame_liveness_use_def(&program->nodes[index], slots, slot_count, uses[index], definitions[index]))
      goto done;
  }
  do {
    changed = false;
    for (index = node_count; index != 0; --index) {
      size_t cursor = index - 1, successor_count, successor_index;
      size_t successors[2];
      uint64_t *old_in = malloc(word_count * sizeof(uint64_t));
      if (old_in == NULL || !ir_successors(program, cursor, successors, &successor_count)) {
        free(old_in);
        goto done;
      }
      memcpy(old_in, live_ins[cursor].words, word_count * sizeof(uint64_t));
      memset(live_outs[cursor].words, 0, word_count * sizeof(uint64_t));
      for (successor_index = 0; successor_index < successor_count; ++successor_index)
        frame_liveness_union(live_outs[cursor], live_ins[successors[successor_index]], word_count);
      memcpy(live_ins[cursor].words, live_outs[cursor].words, word_count * sizeof(uint64_t));
      for (size_t word = 0; word < word_count; ++word)
        live_ins[cursor].words[word] =
            (live_ins[cursor].words[word] & ~definitions[cursor].words[word]) | uses[cursor].words[word];
      for (size_t word = 0; word < word_count; ++word)
        changed |= old_in[word] != live_ins[cursor].words[word];
      free(old_in);
    }
  } while (changed);
  {
    size_t write_index = 0;
    for (index = 0; index < node_count; ++index) {
      VirconIrNode *node = &program->nodes[index];
      bool remove = false;
      if (node->kind == VIRCON_IR_NODE_INSTRUCTION && node->opcode == VIRCON_IR_OPCODE_MOV &&
          node->operand_count == 2 && node->operands[0].kind == VIRCON_IR_OPERAND_MEMORY_REGISTER &&
          node->operands[0].reg == VIRCON_IR_REGISTER_BP) {
        size_t slot_index;
        int32_t slot = node->operands[0].has_displacement ? node->operands[0].displacement : 0;
        if (find_frame_liveness_slot(slots, slot_count, slot, &slot_index) &&
            !frame_liveness_contains(live_outs[index], slot_index))
          remove = true;
      }
      if (remove) {
        dispose_node(node);
        continue;
      }
      if (write_index != index) {
        program->nodes[write_index] = *node;
        memset(node, 0, sizeof(*node));
      }
      ++write_index;
    }
    program->count = write_index;
  }
done:
  if (uses != NULL) {
    for (index = 0; index < node_count; ++index) {
      free(uses[index].words);
      free(definitions[index].words);
      free(live_ins[index].words);
      free(live_outs[index].words);
    }
  }
  free(uses);
  free(definitions);
  free(live_ins);
  free(live_outs);
}
