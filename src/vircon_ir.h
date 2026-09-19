#ifndef WASM2VIRCON_VIRCON_IR_H
#define WASM2VIRCON_VIRCON_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics.h"

typedef enum VirconIrOp {
    VIRCON_IR_LABEL,
    VIRCON_IR_MOV_I32,
    VIRCON_IR_SET_BACKGROUND_COLOR,
    VIRCON_IR_END_FRAME,
    VIRCON_IR_JUMP,
    VIRCON_IR_UNREACHABLE,
    VIRCON_IR_HALT
} VirconIrOp;

typedef struct VirconIrInstruction {
    VirconIrOp op;
    char *label;
    int register_number;
    int32_t i32_value;
} VirconIrInstruction;

typedef struct VirconIrProgram {
    VirconIrInstruction *instructions;
    size_t count;
    size_t capacity;
} VirconIrProgram;

void vircon_ir_init(VirconIrProgram *program);
void vircon_ir_dispose(VirconIrProgram *program);
bool vircon_ir_append(VirconIrProgram *program, VirconIrInstruction instruction,
                      Diagnostics *diagnostics);

#endif
