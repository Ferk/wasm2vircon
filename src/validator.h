#ifndef WASM2VIRCON_VALIDATOR_H
#define WASM2VIRCON_VALIDATOR_H

#include "diagnostics.h"
#include "wasm_module.h"

typedef struct ValidatedModule {
    const WasmModule *module;
    const WasmFunction *entry;
    bool *reachable;
} ValidatedModule;

bool validate_virconwasm_v1(const WasmModule *module, const char *entry_name,
                            bool allow_stack_pointer,
                            ValidatedModule *validated, Diagnostics *diagnostics);
void validated_module_dispose(ValidatedModule *validated);

#endif
