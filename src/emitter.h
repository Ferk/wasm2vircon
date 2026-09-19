#ifndef WASM2VIRCON_EMITTER_H
#define WASM2VIRCON_EMITTER_H

#include "diagnostics.h"
#include "vircon_ir.h"

bool emit_vircon_assembly(const VirconIrProgram *program, const char *path,
                          Diagnostics *diagnostics);

#endif
