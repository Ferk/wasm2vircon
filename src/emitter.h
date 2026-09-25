/* Public boundary from compiler-owned V32 IR to textual Vircon32 assembly. */

#ifndef WASM2VIRCON_EMITTER_H
#define WASM2VIRCON_EMITTER_H

#include "diagnostics.h"
#include "vircon_ir.h"

/* Writes program to path, reporting a file error through diagnostics. */
bool emit_vircon_assembly(const VirconIrProgram *program, const char *path, Diagnostics *diagnostics);

#endif
