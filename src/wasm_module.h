#ifndef WASM2VIRCON_WASM_MODULE_H
#define WASM2VIRCON_WASM_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics.h"

typedef enum WasmValueType { WASM_VALUE_NONE, WASM_VALUE_I32, WASM_VALUE_F32, WASM_VALUE_OTHER } WasmValueType;
typedef enum WasmExprKind {
    WASM_EXPR_BLOCK, WASM_EXPR_LOOP, WASM_EXPR_BR, WASM_EXPR_BR_IF, WASM_EXPR_CALL,
    WASM_EXPR_I32_CONST, WASM_EXPR_F32_CONST, WASM_EXPR_UNREACHABLE, WASM_EXPR_IF,
    WASM_EXPR_LOCAL_GET, WASM_EXPR_LOCAL_SET, WASM_EXPR_LOAD,
    WASM_EXPR_STORE, WASM_EXPR_UNARY, WASM_EXPR_BINARY, WASM_EXPR_SELECT,
    WASM_EXPR_RETURN, WASM_EXPR_DROP
} WasmExprKind;
typedef enum WasmUnaryOp { WASM_UNARY_EQZ, WASM_UNARY_CONVERT_I32_S_TO_F32, WASM_UNARY_OTHER } WasmUnaryOp;
typedef enum WasmBinaryOp {
    WASM_BINARY_ADD, WASM_BINARY_SUB, WASM_BINARY_MUL, WASM_BINARY_DIV_U,
    WASM_BINARY_REM_S, WASM_BINARY_SHL, WASM_BINARY_AND, WASM_BINARY_EQ,
    WASM_BINARY_NE, WASM_BINARY_LT_S, WASM_BINARY_LT_U, WASM_BINARY_GT_S,
    WASM_BINARY_GT_U, WASM_BINARY_GE_S, WASM_BINARY_F32_MUL, WASM_BINARY_OTHER
} WasmBinaryOp;

typedef struct WasmExpr {
    WasmExprKind kind;
    char *name;
    int32_t i32_value;
    float f32_value;
    uint32_t index, offset, bytes, align;
    bool is_signed, is_tee;
    WasmUnaryOp unary_op;
    WasmBinaryOp binary_op;
    struct WasmExpr **children;
    size_t child_count;
} WasmExpr;

typedef struct WasmFunction {
    char *name;
    bool is_import;
    char *import_module, *import_name;
    WasmValueType params[4];
    size_t param_count;
    WasmValueType result;
    WasmValueType *locals;
    size_t local_count;
    WasmExpr *body;
} WasmFunction;

typedef struct WasmExport { char *name, *value; bool is_function; } WasmExport;
typedef struct WasmDataSegment {
    uint32_t offset;
    unsigned char *bytes;
    size_t size;
    bool is_passive, offset_is_i32_const;
} WasmDataSegment;

typedef struct WasmModule {
    WasmFunction *functions; size_t function_count;
    WasmExport *exports; size_t export_count;
    bool has_memory, has_imported_memory, memory_is_shared, memory_is_64, memory_has_max;
    uint32_t memory_initial_pages, memory_max_pages;
    size_t memory_count, table_count, global_count, element_segment_count;
    WasmDataSegment *data_segments; size_t data_segment_count;
} WasmModule;

bool wasm_module_load(const char *path, WasmModule *module, Diagnostics *diagnostics);
void wasm_module_dispose(WasmModule *module);
const WasmFunction *wasm_module_find_function(const WasmModule *module, const char *name);

#endif
