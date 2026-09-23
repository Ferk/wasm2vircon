/* Compiler-owned decoded Wasm model, isolated from Binaryen's C API. */

#ifndef WASM2VIRCON_WASM_MODULE_H
#define WASM2VIRCON_WASM_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics.h"

/* Value types retained by the restricted Wasm frontend. */
typedef enum WasmValueType {
  WASM_VALUE_NONE,
  WASM_VALUE_I32,
  WASM_VALUE_F32,
  WASM_VALUE_I64,
  WASM_VALUE_OTHER
} WasmValueType;
/* Expression forms that can cross from Binaryen into compiler-owned code. */
typedef enum WasmExprKind {
  WASM_EXPR_BLOCK,
  WASM_EXPR_LOOP,
  WASM_EXPR_BR,
  WASM_EXPR_BR_IF,
  WASM_EXPR_CALL,
  WASM_EXPR_I32_CONST,
  WASM_EXPR_F32_CONST,
  WASM_EXPR_UNREACHABLE,
  WASM_EXPR_IF,
  WASM_EXPR_LOCAL_GET,
  WASM_EXPR_LOCAL_SET,
  WASM_EXPR_LOAD,
  WASM_EXPR_STORE,
  WASM_EXPR_I64_CONST_STORE,
  WASM_EXPR_I64_LOAD_STORE,
  WASM_EXPR_I64_WORD_EXTRACT,
  WASM_EXPR_I64_LOAD_STORE_LOCAL_TEE,
  WASM_EXPR_I64_LOCAL_WORD_EXTRACT,
  WASM_EXPR_I64_LOCAL_TEE_WORD_EXTRACT,
  WASM_EXPR_I64_PACKED_I32_STORE,
  WASM_EXPR_UNARY,
  WASM_EXPR_BINARY,
  WASM_EXPR_SELECT,
  WASM_EXPR_RETURN,
  WASM_EXPR_DROP,
  WASM_EXPR_STACK_POINTER_GET,
  WASM_EXPR_STACK_POINTER_SET,
  WASM_EXPR_GLOBAL_GET,
  WASM_EXPR_GLOBAL_SET,
  WASM_EXPR_MEMORY_COPY,
  WASM_EXPR_MEMORY_FILL
} WasmExprKind;
/* Supported unary operations; OTHER preserves a useful rejection diagnostic. */
typedef enum WasmUnaryOp {
  WASM_UNARY_EQZ,
  WASM_UNARY_EXTEND8_S,
  WASM_UNARY_EXTEND16_S,
  WASM_UNARY_CONVERT_I32_S_TO_F32,
  WASM_UNARY_CONVERT_I32_U_TO_F32,
  WASM_UNARY_TRUNC_SAT_F32_TO_I32,
  WASM_UNARY_REINTERPRET_F32_TO_I32,
  WASM_UNARY_REINTERPRET_I32_TO_F32,
  WASM_UNARY_F32_NEG,
  WASM_UNARY_F32_ABS,
  WASM_UNARY_F32_FLOOR,
  WASM_UNARY_F32_CEIL,
  WASM_UNARY_OTHER
} WasmUnaryOp;
/* Supported binary operations; OTHER preserves a useful rejection diagnostic.
 */
typedef enum WasmBinaryOp {
  WASM_BINARY_ADD,
  WASM_BINARY_SUB,
  WASM_BINARY_MUL,
  WASM_BINARY_DIV_S,
  WASM_BINARY_DIV_U,
  WASM_BINARY_REM_S,
  WASM_BINARY_SHL,
  WASM_BINARY_SHR_S,
  WASM_BINARY_AND,
  WASM_BINARY_XOR,
  WASM_BINARY_EQ,
  WASM_BINARY_NE,
  WASM_BINARY_LT_S,
  WASM_BINARY_LT_U,
  WASM_BINARY_GT_S,
  WASM_BINARY_GT_U,
  WASM_BINARY_GE_S,
  WASM_BINARY_GE_U,
  WASM_BINARY_LE_S,
  WASM_BINARY_LE_U,
  WASM_BINARY_OR,
  WASM_BINARY_REM_U,
  WASM_BINARY_SHR_U,
  WASM_BINARY_ROTL,
  WASM_BINARY_ROTR,
  WASM_BINARY_F32_ADD,
  WASM_BINARY_F32_SUB,
  WASM_BINARY_F32_MUL,
  WASM_BINARY_F32_DIV,
  WASM_BINARY_F32_EQ,
  WASM_BINARY_F32_NE,
  WASM_BINARY_F32_LE,
  WASM_BINARY_F32_LT,
  WASM_BINARY_F32_GT,
  WASM_BINARY_F32_GE,
  WASM_BINARY_OTHER
} WasmBinaryOp;

/* One decoded expression with frontend-only diagnostics and child ownership. */
typedef struct WasmExpr {
  WasmExprKind kind;
  /* Frontend-only source identity used in diagnostics, not by V32 IR. */
  const char *opcode;
  char *path;
  char *name;
  int32_t i32_value;
  uint64_t i64_value;
  float f32_value;
  WasmValueType value_type;
  uint32_t index, offset, bytes, align, source_offset;
  bool is_signed, is_tee;
  WasmUnaryOp unary_op;
  WasmBinaryOp binary_op;
  struct WasmExpr **children;
  size_t child_count;
} WasmExpr;

/* One Wasm function, including imports and its compiler-owned body tree. */
typedef struct WasmFunction {
  /* Stable Wasm function index, including imports. */
  size_t index;
  char *name;
  char *diagnostic_name;
  bool is_import;
  char *import_module, *import_name;
  WasmValueType params[4];
  size_t param_count;
  WasmValueType result;
  WasmValueType *locals;
  size_t local_count;
  WasmExpr *body;
} WasmFunction;

/* An exported internal Wasm function name. */
typedef struct WasmExport {
  char *name, *value;
  bool is_function;
} WasmExport;
/* An active or passive Wasm data segment in byte-addressed source memory. */
typedef struct WasmDataSegment {
  uint32_t offset;
  unsigned char *bytes;
  size_t size;
  bool is_passive, offset_is_i32_const;
} WasmDataSegment;

/* Decoded module state required by validation and lowering. */
typedef struct WasmModule {
  WasmFunction *functions;
  size_t function_count;
  WasmExport *exports;
  size_t export_count;
  bool has_memory, has_imported_memory, memory_is_shared, memory_is_64,
      memory_has_max;
  uint32_t memory_initial_pages, memory_max_pages;
  size_t memory_count, table_count, global_count, element_segment_count;
  /* The only global that can be accepted by the restricted opt-in ABI. */
  bool has_stack_pointer_global, stack_pointer_global_is_valid;
  uint32_t stack_pointer_initial;
  char *stack_pointer_name;
  WasmDataSegment *data_segments;
  size_t data_segment_count;
} WasmModule;

/* Decodes a Wasm file through Binaryen into the compiler-owned module model. */
bool wasm_module_load(const char *path, WasmModule *module,
                      Diagnostics *diagnostics);
/* Releases every allocation owned by a decoded module. */
void wasm_module_dispose(WasmModule *module);
/* Finds a function by its internal Wasm name. */
const WasmFunction *wasm_module_find_function(const WasmModule *module,
                                              const char *name);

#endif
