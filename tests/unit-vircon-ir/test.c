/* Focused checks for structured V32 IR opcode and operand decoding. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics.h"
#include "vircon_ir.h"

/* Copies a compiler-generated instruction because the append API takes ownership. */
static char *copy_instruction(const char *text) {
  size_t size = strlen(text) + 1;
  char *copy = malloc(size);
  if (copy != NULL)
    memcpy(copy, text, size);
  return copy;
}

/* Appends one test instruction while preserving the ownership contract. */
static bool append_instruction(VirconIrProgram *program, Diagnostics *diagnostics, const char *text) {
  char *copy = copy_instruction(text);
  return copy != NULL && vircon_ir_append_generated_instruction(program, copy, diagnostics);
}

/* Returns failure after disposing all test-owned compiler state. */
static int fail(VirconIrProgram *program, FILE *diagnostic_stream, int code) {
  vircon_ir_dispose(program);
  fclose(diagnostic_stream);
  return code;
}

/* Verifies that generated target operations become structured, typed nodes. */
int main(void) {
  Diagnostics diagnostics;
  VirconIrProgram program;
  const VirconIrNode *node;
  const VirconIrOperand direct_operands[2] = {
      {.kind = VIRCON_IR_OPERAND_REGISTER, .reg = VIRCON_IR_REGISTER_R2},
      {.kind = VIRCON_IR_OPERAND_IMMEDIATE,
       .immediate = 7,
       .integer_format = VIRCON_IR_INTEGER_UNSIGNED_DECIMAL}};
  FILE *diagnostic_stream = tmpfile();

  if (diagnostic_stream == NULL)
    return 1;
  diagnostics_init(&diagnostics, diagnostic_stream);
  vircon_ir_init(&program);

  if (!append_instruction(&program, &diagnostics, "  mov R1, 0xFFFFFFFF") ||
      !append_instruction(&program, &diagnostics, "  mov [BP-3], R1") ||
      !append_instruction(&program, &diagnostics, "  mov [SP+0], R1") ||
      !append_instruction(&program, &diagnostics, "  mov R0, [1016384]") ||
      !append_instruction(&program, &diagnostics, "  out GPU_Command, GPUCommand_ClearScreen") ||
      !append_instruction(&program, &diagnostics, "  hlt  ; Wasm memory/unreachable trap") ||
      !vircon_ir_append_label(&program, "__test_label", &diagnostics) ||
      !vircon_ir_append_pointer(&program, "__test_target", &diagnostics) ||
      !vircon_ir_append_instruction(&program, VIRCON_IR_OPCODE_IADD, direct_operands, 2, NULL, &diagnostics))
    return fail(&program, diagnostic_stream, 2);

  node = &program.nodes[0];
  if (node->kind != VIRCON_IR_NODE_INSTRUCTION || node->opcode != VIRCON_IR_OPCODE_MOV ||
      node->operand_count != 2 || node->operands[0].kind != VIRCON_IR_OPERAND_REGISTER ||
      node->operands[0].reg != VIRCON_IR_REGISTER_R1 ||
      node->operands[1].kind != VIRCON_IR_OPERAND_IMMEDIATE || node->operands[1].immediate != UINT32_MAX ||
      node->operands[1].integer_format != VIRCON_IR_INTEGER_HEXADECIMAL ||
      node->operands[1].hexadecimal_digits != 8)
    return fail(&program, diagnostic_stream, 3);

  node = &program.nodes[1];
  if (node->operands[0].kind != VIRCON_IR_OPERAND_MEMORY_REGISTER ||
      node->operands[0].reg != VIRCON_IR_REGISTER_BP || !node->operands[0].has_displacement ||
      node->operands[0].displacement != -3 || node->operands[1].reg != VIRCON_IR_REGISTER_R1)
    return fail(&program, diagnostic_stream, 4);

  node = &program.nodes[2];
  if (node->operands[0].kind != VIRCON_IR_OPERAND_MEMORY_REGISTER ||
      node->operands[0].reg != VIRCON_IR_REGISTER_SP || !node->operands[0].has_displacement ||
      node->operands[0].displacement != 0)
    return fail(&program, diagnostic_stream, 5);

  node = &program.nodes[3];
  if (node->operands[1].kind != VIRCON_IR_OPERAND_MEMORY_ABSOLUTE ||
      node->operands[1].immediate != UINT32_C(1016384))
    return fail(&program, diagnostic_stream, 6);

  node = &program.nodes[4];
  if (node->opcode != VIRCON_IR_OPCODE_OUT || node->operands[0].kind != VIRCON_IR_OPERAND_SYMBOL ||
      strcmp(node->operands[0].symbol, "GPU_Command") != 0 ||
      strcmp(node->operands[1].symbol, "GPUCommand_ClearScreen") != 0)
    return fail(&program, diagnostic_stream, 7);

  node = &program.nodes[5];
  if (node->opcode != VIRCON_IR_OPCODE_HLT || node->comment == NULL ||
      strcmp(node->comment, "Wasm memory/unreachable trap") != 0)
    return fail(&program, diagnostic_stream, 8);
  if (program.nodes[6].kind != VIRCON_IR_NODE_LABEL || strcmp(program.nodes[6].name, "__test_label") != 0 ||
      program.nodes[7].kind != VIRCON_IR_NODE_POINTER || strcmp(program.nodes[7].name, "__test_target") != 0)
    return fail(&program, diagnostic_stream, 9);
  if (program.nodes[8].kind != VIRCON_IR_NODE_INSTRUCTION ||
      program.nodes[8].opcode != VIRCON_IR_OPCODE_IADD ||
      program.nodes[8].operands[0].reg != VIRCON_IR_REGISTER_R2 || program.nodes[8].operands[1].immediate != 7)
    return fail(&program, diagnostic_stream, 10);

  if (append_instruction(&program, &diagnostics, "  imaginary R1") || diagnostics.errors != 1)
    return fail(&program, diagnostic_stream, 11);

  vircon_ir_dispose(&program);
  vircon_ir_init(&program);
  if (!append_instruction(&program, &diagnostics, "  mov [BP-1], R1") ||
      !append_instruction(&program, &diagnostics, "  mov R1, [BP-1]") ||
      !append_instruction(&program, &diagnostics, "  iadd R1, 1") ||
      !append_instruction(&program, &diagnostics, "  mov R1, [BP-1]") ||
      !vircon_ir_append_label(&program, "__copy_join", &diagnostics) ||
      !append_instruction(&program, &diagnostics, "  mov R1, [BP-1]"))
    return fail(&program, diagnostic_stream, 12);
  vircon_ir_optimize_local(&program);
  if (program.count != 5 || program.nodes[0].opcode != VIRCON_IR_OPCODE_MOV ||
      program.nodes[1].opcode != VIRCON_IR_OPCODE_IADD || program.nodes[2].opcode != VIRCON_IR_OPCODE_MOV ||
      program.nodes[3].kind != VIRCON_IR_NODE_LABEL || program.nodes[4].opcode != VIRCON_IR_OPCODE_MOV)
    return fail(&program, diagnostic_stream, 13);

  vircon_ir_dispose(&program);
  vircon_ir_init(&program);
  if (!append_instruction(&program, &diagnostics, "  mov R1, 40") ||
      !append_instruction(&program, &diagnostics, "  iadd R1, 2") ||
      !append_instruction(&program, &diagnostics, "  imul R1, 3") ||
      !append_instruction(&program, &diagnostics, "  ieq R1, 126") ||
      !append_instruction(&program, &diagnostics, "  mov R2, -1") ||
      !append_instruction(&program, &diagnostics, "  ilt R2, 0") ||
      !append_instruction(&program, &diagnostics, "  mov R3, 8") ||
      !append_instruction(&program, &diagnostics, "  and R3, 0") ||
      !append_instruction(&program, &diagnostics, "  imul R3, 4") ||
      !vircon_ir_append_label(&program, "__constant_join", &diagnostics) ||
      !append_instruction(&program, &diagnostics, "  iadd R1, 1"))
    return fail(&program, diagnostic_stream, 14);
  vircon_ir_optimize_local(&program);
  if (program.count != 5 || program.nodes[0].opcode != VIRCON_IR_OPCODE_MOV ||
      program.nodes[0].operands[1].kind != VIRCON_IR_OPERAND_IMMEDIATE ||
      program.nodes[0].operands[1].immediate != 1 || program.nodes[1].opcode != VIRCON_IR_OPCODE_MOV ||
      program.nodes[1].operands[1].kind != VIRCON_IR_OPERAND_IMMEDIATE ||
      program.nodes[1].operands[1].immediate != 1 || program.nodes[2].opcode != VIRCON_IR_OPCODE_MOV ||
      program.nodes[2].operands[1].kind != VIRCON_IR_OPERAND_IMMEDIATE ||
      program.nodes[2].operands[1].immediate != 0 || program.nodes[3].kind != VIRCON_IR_NODE_LABEL ||
      program.nodes[4].opcode != VIRCON_IR_OPCODE_IADD)
    return fail(&program, diagnostic_stream, 15);

  vircon_ir_dispose(&program);
  vircon_ir_init(&program);
  if (!append_instruction(&program, &diagnostics, "  mov R1, 1") ||
      !append_instruction(&program, &diagnostics, "  mov [BP-3], R1") ||
      !append_instruction(&program, &diagnostics, "  mov R2, 2") ||
      !append_instruction(&program, &diagnostics, "  mov [BP-3], R2") ||
      !append_instruction(&program, &diagnostics, "  mov [BP-4], R1") ||
      !vircon_ir_append_label(&program, "__frame_live", &diagnostics) ||
      !append_instruction(&program, &diagnostics, "  mov R3, [BP-4]") ||
      !append_instruction(&program, &diagnostics, "  ret"))
    return fail(&program, diagnostic_stream, 16);
  vircon_ir_eliminate_dead_frame_stores(&program);
  if (program.count != 6 || program.nodes[0].operands[1].immediate != 1 ||
      program.nodes[1].operands[1].immediate != 2 ||
      program.nodes[2].operands[0].kind != VIRCON_IR_OPERAND_MEMORY_REGISTER ||
      program.nodes[2].operands[0].displacement != -4 || program.nodes[3].kind != VIRCON_IR_NODE_LABEL ||
      program.nodes[4].opcode != VIRCON_IR_OPCODE_MOV ||
      program.nodes[4].operands[1].kind != VIRCON_IR_OPERAND_MEMORY_REGISTER ||
      program.nodes[4].operands[1].displacement != -4 || program.nodes[5].opcode != VIRCON_IR_OPCODE_RET)
    return fail(&program, diagnostic_stream, 17);

  vircon_ir_dispose(&program);
  fclose(diagnostic_stream);
  return 0;
}
