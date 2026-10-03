/*
 * Bounded WebAssembly custom-section and DWARF line-table handling.
 *
 * This module does not decode Wasm instructions. It extracts source-line
 * metadata from debug custom sections and builds a Binaryen-safe view that
 * omits only DWARF sections known to be irrelevant to core Wasm semantics.
 */

#ifndef WASM2VIRCON_WASM_DWARF_H
#define WASM2VIRCON_WASM_DWARF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics.h"

/* Best source location found for one original defined-function ordinal. */
typedef struct WasmDwarfFunctionLocation {
  size_t defined_function_index;
  char *function_name;
  char *file;
  uint32_t line;
  uint32_t column;
} WasmDwarfFunctionLocation;

/* DWARF metadata retained outside Binaryen-owned module state. */
typedef struct WasmDwarfInfo {
  bool present;
  bool line_table_present;
  bool line_table_supported;
  uint16_t line_version;
  char *line_error;
  char **section_names;
  size_t section_count;
  WasmDwarfFunctionLocation *functions;
  size_t function_count;
} WasmDwarfInfo;

/* Builds a byte-for-byte core/custom module view with DWARF sections omitted. */
bool wasm_dwarf_prepare_input(const char *contents, size_t size, char **filtered, size_t *filtered_size,
                              WasmDwarfInfo *info, Diagnostics *diagnostics);
/* Associates original function locations with names before Binaryen cleanup. */
bool wasm_dwarf_set_function_name(WasmDwarfInfo *info, size_t defined_index, const char *name,
                                  Diagnostics *diagnostics);
/* Finds a source location by retained function name, then stable ordinal. */
const WasmDwarfFunctionLocation *wasm_dwarf_find_function(const WasmDwarfInfo *info, const char *name,
                                                          size_t defined_index);
/* Releases all section, function-name, and source-path storage. */
void wasm_dwarf_dispose(WasmDwarfInfo *info);

#endif
