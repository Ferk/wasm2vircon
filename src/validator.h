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
bool validate_virconwasm_v1(const WasmModule *module, const char *entry_name, ValidatedModule *validated,
                            Diagnostics *diagnostics);
/* Releases validation-owned reachability state. */
void validated_module_dispose(ValidatedModule *validated);

/* Returns whether one registered platform import is a direct hardware operation
 * that cannot read or write the Wasm linear-memory region. Unknown, defined,
 * and future unclassified calls are deliberately treated as memory clobbers. */
bool platform_import_preserves_linear_memory(const WasmFunction *function);

#endif
