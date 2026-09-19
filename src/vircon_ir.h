#ifndef WASM2VIRCON_VIRCON_IR_H
#define WASM2VIRCON_VIRCON_IR_H

#include <stddef.h>
#include "diagnostics.h"

/* A deliberately small assembly-oriented V32 IR. The Wasm adapter never
 * reaches the emitter, and the emitter never sees Binaryen. */
typedef struct VirconIrInstruction { char *text; } VirconIrInstruction;
typedef struct VirconIrProgram { VirconIrInstruction *instructions; size_t count, capacity; } VirconIrProgram;
void vircon_ir_init(VirconIrProgram *program);
void vircon_ir_dispose(VirconIrProgram *program);
bool vircon_ir_append_text(VirconIrProgram *program, char *text, Diagnostics *diagnostics);
#endif
