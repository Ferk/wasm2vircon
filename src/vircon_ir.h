/* Compiler-owned structured V32 IR between frontend lowering and assembly emission. */

#ifndef WASM2VIRCON_VIRCON_IR_H
#define WASM2VIRCON_VIRCON_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics.h"

/* Node categories retained independently from their eventual assembly syntax. */
typedef enum VirconIrNodeKind {
  VIRCON_IR_NODE_INSTRUCTION,
  VIRCON_IR_NODE_LABEL,
  VIRCON_IR_NODE_POINTER
} VirconIrNodeKind;

/* Vircon32 operations currently selectable by the backend. */
typedef enum VirconIrOpcode {
  VIRCON_IR_OPCODE_MOV,
  VIRCON_IR_OPCODE_PUSH,
  VIRCON_IR_OPCODE_POP,
  VIRCON_IR_OPCODE_CALL,
  VIRCON_IR_OPCODE_RET,
  VIRCON_IR_OPCODE_HLT,
  VIRCON_IR_OPCODE_JMP,
  VIRCON_IR_OPCODE_JT,
  VIRCON_IR_OPCODE_JF,
  VIRCON_IR_OPCODE_WAIT,
  VIRCON_IR_OPCODE_IADD,
  VIRCON_IR_OPCODE_ISUB,
  VIRCON_IR_OPCODE_IMUL,
  VIRCON_IR_OPCODE_IDIV,
  VIRCON_IR_OPCODE_IMOD,
  VIRCON_IR_OPCODE_AND,
  VIRCON_IR_OPCODE_OR,
  VIRCON_IR_OPCODE_XOR,
  VIRCON_IR_OPCODE_SHL,
  VIRCON_IR_OPCODE_IEQ,
  VIRCON_IR_OPCODE_INE,
  VIRCON_IR_OPCODE_ILT,
  VIRCON_IR_OPCODE_ILE,
  VIRCON_IR_OPCODE_IGT,
  VIRCON_IR_OPCODE_IGE,
  VIRCON_IR_OPCODE_FADD,
  VIRCON_IR_OPCODE_FSUB,
  VIRCON_IR_OPCODE_FMUL,
  VIRCON_IR_OPCODE_FDIV,
  VIRCON_IR_OPCODE_FMOD,
  VIRCON_IR_OPCODE_FEQ,
  VIRCON_IR_OPCODE_FNE,
  VIRCON_IR_OPCODE_FLT,
  VIRCON_IR_OPCODE_FLE,
  VIRCON_IR_OPCODE_FGT,
  VIRCON_IR_OPCODE_FGE,
  VIRCON_IR_OPCODE_CIF,
  VIRCON_IR_OPCODE_CFI,
  VIRCON_IR_OPCODE_FSGN,
  VIRCON_IR_OPCODE_FABS,
  VIRCON_IR_OPCODE_FLR,
  VIRCON_IR_OPCODE_CEIL,
  VIRCON_IR_OPCODE_ROUND,
  VIRCON_IR_OPCODE_SIN,
  VIRCON_IR_OPCODE_ACOS,
  VIRCON_IR_OPCODE_LOG,
  VIRCON_IR_OPCODE_POW,
  VIRCON_IR_OPCODE_ATAN2,
  VIRCON_IR_OPCODE_IMIN,
  VIRCON_IR_OPCODE_IMAX,
  VIRCON_IR_OPCODE_IABS,
  VIRCON_IR_OPCODE_FMIN,
  VIRCON_IR_OPCODE_FMAX,
  VIRCON_IR_OPCODE_IN,
  VIRCON_IR_OPCODE_OUT,
  VIRCON_IR_OPCODE_COUNT
} VirconIrOpcode;

/* Target registers are values rather than textual assembler names. */
typedef enum VirconIrRegister {
  VIRCON_IR_REGISTER_R0,
  VIRCON_IR_REGISTER_R1,
  VIRCON_IR_REGISTER_R2,
  VIRCON_IR_REGISTER_R3,
  VIRCON_IR_REGISTER_R4,
  VIRCON_IR_REGISTER_R5,
  VIRCON_IR_REGISTER_R6,
  VIRCON_IR_REGISTER_R7,
  VIRCON_IR_REGISTER_R8,
  VIRCON_IR_REGISTER_R9,
  VIRCON_IR_REGISTER_R10,
  VIRCON_IR_REGISTER_R11,
  VIRCON_IR_REGISTER_R12,
  VIRCON_IR_REGISTER_R13,
  VIRCON_IR_REGISTER_R14,
  VIRCON_IR_REGISTER_R15,
  VIRCON_IR_REGISTER_SP,
  VIRCON_IR_REGISTER_BP
} VirconIrRegister;

/* Typed operand forms needed for analysis and target-specific optimization. */
typedef enum VirconIrOperandKind {
  VIRCON_IR_OPERAND_REGISTER,
  VIRCON_IR_OPERAND_IMMEDIATE,
  VIRCON_IR_OPERAND_SYMBOL,
  VIRCON_IR_OPERAND_MEMORY_REGISTER,
  VIRCON_IR_OPERAND_MEMORY_ABSOLUTE
} VirconIrOperandKind;

/* Retains numeric meaning while allowing the emitter to preserve readable notation. */
typedef enum VirconIrIntegerFormat {
  VIRCON_IR_INTEGER_SIGNED_DECIMAL,
  VIRCON_IR_INTEGER_UNSIGNED_DECIMAL,
  VIRCON_IR_INTEGER_HEXADECIMAL
} VirconIrIntegerFormat;

/* One instruction operand; symbol is owned only for VIRCON_IR_OPERAND_SYMBOL. */
typedef struct VirconIrOperand {
  VirconIrOperandKind kind;
  VirconIrRegister reg;
  uint32_t immediate;
  VirconIrIntegerFormat integer_format;
  unsigned hexadecimal_digits;
  int32_t displacement;
  bool has_displacement;
  char *symbol;
} VirconIrOperand;

/* One structured instruction, label, or immutable pointer-data entry. */
typedef struct VirconIrNode {
  VirconIrNodeKind kind;
  VirconIrOpcode opcode;
  VirconIrOperand operands[2];
  size_t operand_count;
  char *name;
  char *comment;
} VirconIrNode;

/* Growable sequence of structured V32 IR nodes. */
typedef struct VirconIrProgram {
  VirconIrNode *nodes;
  size_t count, capacity;
} VirconIrProgram;

/* Initializes an empty program. */
void vircon_ir_init(VirconIrProgram *program);
/* Releases all node, operand, and comment storage owned by a program. */
void vircon_ir_dispose(VirconIrProgram *program);
/* Copies and appends one structured target instruction. */
bool vircon_ir_append_instruction(VirconIrProgram *program, VirconIrOpcode opcode, const VirconIrOperand *operands,
                                  size_t operand_count, const char *comment, Diagnostics *diagnostics);
/* Appends a named assembly target without exposing label punctuation to lowering. */
bool vircon_ir_append_label(VirconIrProgram *program, const char *name, Diagnostics *diagnostics);
/* Appends one immutable ROM pointer entry used by generated jump tables. */
bool vircon_ir_append_pointer(VirconIrProgram *program, const char *target, Diagnostics *diagnostics);
/* Strictly decodes one compiler-generated instruction line into structured V32 IR and frees text. */
bool vircon_ir_append_generated_instruction(VirconIrProgram *program, char *text, Diagnostics *diagnostics);
/* Removes redundant target-register/frame-slot copies within straight-line regions. */
void vircon_ir_optimize_copies(VirconIrProgram *program);

#endif
