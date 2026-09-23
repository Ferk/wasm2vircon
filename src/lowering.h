/* Public boundary from validated VirconWasm to compiler-owned V32 IR. */

#ifndef WASM2VIRCON_LOWERING_H
#define WASM2VIRCON_LOWERING_H
#include "diagnostics.h"
#include "validator.h"
#include "vircon_ir.h"
/* Lowers one validated module without exposing Binaryen to the backend. */
bool lower_module_to_vircon_ir(const ValidatedModule *module,
                               VirconIrProgram *program,
                               Diagnostics *diagnostics);
#endif
