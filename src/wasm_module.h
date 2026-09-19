#ifndef WASM2VIRCON_WASM_MODULE_H
#define WASM2VIRCON_WASM_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics.h"

typedef enum WasmValueType {
    WASM_VALUE_NONE,
    WASM_VALUE_I32,
    WASM_VALUE_OTHER
} WasmValueType;

typedef enum WasmExprKind {
    WASM_EXPR_BLOCK,
    WASM_EXPR_LOOP,
    WASM_EXPR_BR,
    WASM_EXPR_CALL,
    WASM_EXPR_I32_CONST,
    WASM_EXPR_UNREACHABLE
} WasmExprKind;

typedef struct WasmExpr {
    WasmExprKind kind;
    char *name;
    int32_t i32_value;
    struct WasmExpr **children;
    size_t child_count;
} WasmExpr;

typedef struct WasmFunction {
    char *name;
    bool is_import;
    char *import_module;
    char *import_name;
    WasmValueType params[4];
    size_t param_count;
    WasmValueType result;
    WasmExpr *body;
} WasmFunction;

typedef struct WasmExport {
    char *name;
    char *value;
    bool is_function;
} WasmExport;

typedef struct WasmModule {
    WasmFunction *functions;
    size_t function_count;
    WasmExport *exports;
    size_t export_count;
    bool has_memory;
    bool has_imported_memory;
    size_t table_count;
    size_t global_count;
    size_t element_segment_count;
    size_t data_segment_count;
} WasmModule;

bool wasm_module_load(const char *path, WasmModule *module, Diagnostics *diagnostics);
void wasm_module_dispose(WasmModule *module);
const WasmFunction *wasm_module_find_function(const WasmModule *module,
                                              const char *name);

#endif
