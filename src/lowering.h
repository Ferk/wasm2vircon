#ifndef WASM2VIRCON_LOWERING_H
#define WASM2VIRCON_LOWERING_H
#include "diagnostics.h"
#include "validator.h"
#include "vircon_ir.h"
bool lower_module_to_vircon_ir(const ValidatedModule *module, VirconIrProgram *program, Diagnostics *diagnostics);
#endif
