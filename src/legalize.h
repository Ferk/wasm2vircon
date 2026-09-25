/* Compiler-owned cleanup of proven-unobservable raw linker artifacts. */

#ifndef WASM2VIRCON_LEGALIZE_H
#define WASM2VIRCON_LEGALIZE_H

#include "diagnostics.h"
#include "wasm_module.h"

/* Removes only unreferenced linker scaffolding and legalizes proven-diverging
 * typed loops. This pass is independent of optional input optimization. */
bool wasm_module_legalize_linker_artifacts(WasmModule *module, Diagnostics *diagnostics);

#endif
