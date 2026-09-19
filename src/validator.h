#ifndef WASM2VIRCON_VALIDATOR_H
#define WASM2VIRCON_VALIDATOR_H

#include "diagnostics.h"
#include "wasm_module.h"

typedef struct ValidatedModule {
    const WasmModule *module;
    const WasmFunction *entry;
} ValidatedModule;

bool validate_virconwasm_v0(const WasmModule *module, const char *entry_name,
                            ValidatedModule *validated, Diagnostics *diagnostics);

#endif
