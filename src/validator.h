/* VirconWasm policy validation interface. */

#ifndef WASM2VIRCON_VALIDATOR_H
#define WASM2VIRCON_VALIDATOR_H

#include "diagnostics.h"
#include "wasm_module.h"

/* Records the validated module, entry function, and reachability map. */
typedef struct ValidatedModule {
  const WasmModule *module;
  const WasmFunction *entry;
  bool *reachable;
} ValidatedModule;

/* Checks one decoded module against the current restricted VirconWasm profile.
 */
bool validate_virconwasm_v1(const WasmModule *module, const char *entry_name, bool allow_stack_pointer,
                            ValidatedModule *validated, Diagnostics *diagnostics);
/* Releases validation-owned reachability state. */
void validated_module_dispose(ValidatedModule *validated);

#endif
