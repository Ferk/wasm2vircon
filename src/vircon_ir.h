/* Small assembly-oriented V32 IR used between frontend lowering and emission.
 */

#ifndef WASM2VIRCON_VIRCON_IR_H
#define WASM2VIRCON_VIRCON_IR_H

#include "diagnostics.h"
#include <stddef.h>

/* A deliberately small assembly-oriented V32 IR. The Wasm adapter never
 * reaches the emitter, and the emitter never sees Binaryen. */
/* Owns one emitted Vircon32 assembly line. */
typedef struct VirconIrInstruction {
  char *text;
} VirconIrInstruction;
/* Growable sequence of V32 IR lines. */
typedef struct VirconIrProgram {
  VirconIrInstruction *instructions;
  size_t count, capacity;
} VirconIrProgram;
/* Initializes an empty program. */
void vircon_ir_init(VirconIrProgram *program);
/* Releases all program-owned instruction text. */
void vircon_ir_dispose(VirconIrProgram *program);
/* Takes ownership of text and appends it to program. */
bool vircon_ir_append_text(VirconIrProgram *program, char *text,
                           Diagnostics *diagnostics);
#endif
