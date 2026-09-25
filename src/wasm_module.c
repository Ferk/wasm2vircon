/* Binaryen adapter: decodes Wasm into the compiler-owned frontend model. */

#include "wasm_module.h"

#include <binaryen-c.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Duplicates a nullable C string for module-owned storage. */
static char *copy_string(const char *source) {
  size_t length;
  char *copy;
  if (source == NULL)
    return NULL;
  length = strlen(source) + 1;
  copy = malloc(length);
  if (copy != NULL)
    memcpy(copy, source, length);
  return copy;
}

/* Identifies a function while Binaryen expressions are converted. */
typedef struct DecodeContext {
  size_t function_index;
  const char *function_name;
  const WasmModule *module;
} DecodeContext;

/* Returns whether a Binaryen function name is useful beyond its numeric index.
 */
static bool has_descriptive_function_name(const char *name) {
  const unsigned char *cursor = (const unsigned char *)name;
  if (name == NULL || name[0] == '\0')
    return false;
  while (*cursor != '\0') {
    if (!isdigit(*cursor))
      return true;
    ++cursor;
  }
  return false;
}

/* Reports a module-conversion failure with a stable Wasm function identity. */
static void function_error(Diagnostics *diagnostics, const DecodeContext *context, const char *format, ...) {
  char reason[512];
  va_list arguments;
  va_start(arguments, format);
  vsnprintf(reason, sizeof(reason), format, arguments);
  va_end(arguments);
  if (has_descriptive_function_name(context->function_name))
    diagnostics_error(diagnostics, "function %zu '%s': %s", context->function_index, context->function_name, reason);
  else
    diagnostics_error(diagnostics, "function %zu: %s", context->function_index, reason);
}

/* Reports an expression-conversion failure with opcode and structural path. */
static void expression_error(Diagnostics *diagnostics, const DecodeContext *context, const char *opcode,
                             const char *path, const char *format, ...) {
  char reason[512];
  va_list arguments;
  va_start(arguments, format);
  vsnprintf(reason, sizeof(reason), format, arguments);
  va_end(arguments);
  if (has_descriptive_function_name(context->function_name))
    diagnostics_error(diagnostics, "Wasm %s in function %zu '%s' at expression path %s: %s", opcode,
                      context->function_index, context->function_name, path, reason);
  else
    diagnostics_error(diagnostics, "Wasm %s in function %zu at expression path %s: %s", opcode, context->function_index,
                      path, reason);
}

/* Recursively releases one compiler-owned expression tree. */
static void free_expression(WasmExpr *expression) {
  size_t index;
  if (expression == NULL)
    return;
  for (index = 0; index < expression->child_count; ++index)
    free_expression(expression->children[index]);
  free(expression->children);
  for (index = 0; index < expression->branch_target_count; ++index)
    free(expression->branch_targets[index]);
  free(expression->branch_targets);
  free(expression->path);
  free(expression->name);
  free(expression);
}

/* Maps one Binaryen value type into the restricted frontend type enum. */
static WasmValueType convert_type(BinaryenType type) {
  if (type == BinaryenTypeNone())
    return WASM_VALUE_NONE;
  if (type == BinaryenTypeInt32())
    return WASM_VALUE_I32;
  if (type == BinaryenTypeFloat32())
    return WASM_VALUE_F32;
  if (type == BinaryenTypeInt64())
    return WASM_VALUE_I64;
  return WASM_VALUE_OTHER;
}

/* Expands a Binaryen parameter tuple into module-owned parameter storage. */
static bool convert_tuple_type(BinaryenType type, WasmValueType **values, size_t *count, Diagnostics *diagnostics,
                               const DecodeContext *context) {
  BinaryenIndex arity = BinaryenTypeArity(type), index;
  BinaryenType *expanded;
  WasmValueType *converted;

  *values = NULL;
  *count = 0;
  if (arity == 0)
    return true;
  expanded = calloc(arity, sizeof(*expanded));
  converted = calloc(arity, sizeof(*converted));
  if (expanded == NULL || converted == NULL) {
    free(expanded);
    free(converted);
    function_error(diagnostics, context, "ran out of memory decoding parameters");
    return false;
  }
  BinaryenTypeExpand(type, expanded);
  for (index = 0; index < arity; ++index) {
    converted[index] = convert_type(expanded[index]);
    if (converted[index] == WASM_VALUE_OTHER) {
      free(expanded);
      free(converted);
      function_error(diagnostics, context, "has an unsupported parameter type");
      return false;
    }
  }
  free(expanded);
  *values = converted;
  *count = arity;
  return true;
}

/* Allocates an expression and records its stable diagnostic path. */
static WasmExpr *new_expression(WasmExprKind kind, const char *opcode, const char *path, Diagnostics *diagnostics) {
  WasmExpr *expression = calloc(1, sizeof(*expression));
  if (expression == NULL)
    diagnostics_error(diagnostics, "out of memory while reading Wasm expression");
  else {
    expression->kind = kind;
    expression->opcode = opcode;
    expression->path = copy_string(path);
    if (expression->path == NULL) {
      diagnostics_error(diagnostics, "out of memory while recording Wasm expression path");
      free(expression);
      expression = NULL;
    }
  }
  return expression;
}

/* Allocates the child-pointer array owned by one decoded expression. */
static bool allocate_children(WasmExpr *expression, size_t count, Diagnostics *diagnostics) {
  expression->child_count = count;
  if (count == 0)
    return true;
  expression->children = calloc(count, sizeof(*expression->children));
  if (expression->children == NULL) {
    diagnostics_error(diagnostics, "out of memory while reading Wasm expression children");
    return false;
  }
  return true;
}

static WasmExpr *convert_expression(BinaryenExpressionRef source, Diagnostics *diagnostics,
                                    const DecodeContext *context, const char *path);

/* Builds a stable child path without requiring Wasm byte offsets or DWARF. */
static WasmExpr *convert_child(BinaryenExpressionRef source, Diagnostics *diagnostics, const DecodeContext *context,
                               const char *parent_path, size_t child_index) {
  char path[128];
  snprintf(path, sizeof(path), "%s/%zu", parent_path, child_index);
  return convert_expression(source, diagnostics, context, path);
}

/* Returns a local's declared type while decoding that function's body. */
static WasmValueType decoded_local_type(const DecodeContext *context, uint32_t index) {
  const WasmFunction *function = &context->module->functions[context->function_index];
  if (index < function->param_count)
    return function->params[index];
  index -= (uint32_t)function->param_count;
  return index < function->local_count ? function->locals[index] : WASM_VALUE_OTHER;
}

/* Matches Zig's two-i32 aggregate packing form without exposing an i64 value.
 */
static bool packed_i64_words(BinaryenExpressionRef source, BinaryenExpressionRef *high, BinaryenExpressionRef *low) {
  BinaryenExpressionRef shifted, shift_amount, extended_high;
  if (BinaryenExpressionGetId(source) != BinaryenBinaryId() || BinaryenBinaryGetOp(source) != BinaryenOrInt64())
    return false;

  shifted = BinaryenBinaryGetLeft(source);
  if (BinaryenExpressionGetId(shifted) != BinaryenBinaryId() || BinaryenBinaryGetOp(shifted) != BinaryenShlInt64())
    return false;
  extended_high = BinaryenBinaryGetLeft(shifted);
  shift_amount = BinaryenBinaryGetRight(shifted);
  if (BinaryenExpressionGetId(extended_high) != BinaryenUnaryId() ||
      BinaryenUnaryGetOp(extended_high) != BinaryenExtendUInt32() ||
      BinaryenExpressionGetId(shift_amount) != BinaryenConstId() ||
      BinaryenExpressionGetType(shift_amount) != BinaryenTypeInt64() || BinaryenConstGetValueI64(shift_amount) != 32)
    return false;

  *low = BinaryenBinaryGetRight(source);
  if (BinaryenExpressionGetId(*low) != BinaryenUnaryId() || BinaryenUnaryGetOp(*low) != BinaryenExtendUInt32())
    return false;
  *high = BinaryenUnaryGetValue(extended_high);
  *low = BinaryenUnaryGetValue(*low);
  return true;
}

/* Names the Binaryen unary operations that can reach the restricted frontend.
 */
/* Returns a diagnostic opcode name for one Binaryen unary operation. */
static const char *unary_opcode(BinaryenOp op) {
  if (op == BinaryenEqZInt32())
    return "i32.eqz";
  if (op == BinaryenExtendS8Int32())
    return "i32.extend8_s";
  if (op == BinaryenExtendS16Int32())
    return "i32.extend16_s";
  if (op == BinaryenConvertSInt32ToFloat32())
    return "f32.convert_i32_s";
  if (op == BinaryenConvertUInt32ToFloat32())
    return "f32.convert_i32_u";
  if (op == BinaryenTruncSatSFloat32ToInt32())
    return "i32.trunc_sat_f32_s";
  if (op == BinaryenReinterpretFloat32())
    return "i32.reinterpret_f32";
  if (op == BinaryenReinterpretInt32())
    return "f32.reinterpret_i32";
  if (op == BinaryenNegFloat32())
    return "f32.neg";
  if (op == BinaryenAbsFloat32())
    return "f32.abs";
  if (op == BinaryenFloorFloat32())
    return "f32.floor";
  if (op == BinaryenCeilFloat32())
    return "f32.ceil";
  if (op == BinaryenClzInt32())
    return "i32.clz";
  if (op == BinaryenCtzInt32())
    return "i32.ctz";
  if (op == BinaryenPopcntInt32())
    return "i32.popcnt";
  return "unknown unary operation";
}

/* Names common Binaryen binary operations, including unsupported ones. */
/* Returns a diagnostic opcode name for one Binaryen binary operation. */
static const char *binary_opcode(BinaryenOp op) {
  if (op == BinaryenAddInt32())
    return "i32.add";
  if (op == BinaryenSubInt32())
    return "i32.sub";
  if (op == BinaryenMulInt32())
    return "i32.mul";
  if (op == BinaryenDivSInt32())
    return "i32.div_s";
  if (op == BinaryenDivUInt32())
    return "i32.div_u";
  if (op == BinaryenRemSInt32())
    return "i32.rem_s";
  if (op == BinaryenRemUInt32())
    return "i32.rem_u";
  if (op == BinaryenAndInt32())
    return "i32.and";
  if (op == BinaryenOrInt32())
    return "i32.or";
  if (op == BinaryenXorInt32())
    return "i32.xor";
  if (op == BinaryenShlInt32())
    return "i32.shl";
  if (op == BinaryenShrUInt32())
    return "i32.shr_u";
  if (op == BinaryenShrSInt32())
    return "i32.shr_s";
  if (op == BinaryenRotLInt32())
    return "i32.rotl";
  if (op == BinaryenRotRInt32())
    return "i32.rotr";
  if (op == BinaryenEqInt32())
    return "i32.eq";
  if (op == BinaryenNeInt32())
    return "i32.ne";
  if (op == BinaryenLtSInt32())
    return "i32.lt_s";
  if (op == BinaryenLtUInt32())
    return "i32.lt_u";
  if (op == BinaryenLeSInt32())
    return "i32.le_s";
  if (op == BinaryenLeUInt32())
    return "i32.le_u";
  if (op == BinaryenGtSInt32())
    return "i32.gt_s";
  if (op == BinaryenGtUInt32())
    return "i32.gt_u";
  if (op == BinaryenGeSInt32())
    return "i32.ge_s";
  if (op == BinaryenGeUInt32())
    return "i32.ge_u";
  if (op == BinaryenAddFloat32())
    return "f32.add";
  if (op == BinaryenSubFloat32())
    return "f32.sub";
  if (op == BinaryenMulFloat32())
    return "f32.mul";
  if (op == BinaryenDivFloat32())
    return "f32.div";
  if (op == BinaryenEqFloat32())
    return "f32.eq";
  if (op == BinaryenNeFloat32())
    return "f32.ne";
  if (op == BinaryenLtFloat32())
    return "f32.lt";
  if (op == BinaryenLeFloat32())
    return "f32.le";
  if (op == BinaryenGtFloat32())
    return "f32.gt";
  if (op == BinaryenGeFloat32())
    return "f32.ge";
  return "unknown binary operation";
}

/* Describes a load using its byte width, signedness, and result type. */
/* Names a Binaryen load using the width and signedness seen by the frontend. */
static const char *load_opcode(BinaryenExpressionRef source) {
  uint32_t bytes = BinaryenLoadGetBytes(source);
  bool signed_load = BinaryenLoadIsSigned(source);
  BinaryenType type = BinaryenExpressionGetType(source);
  if (type == BinaryenTypeInt64())
    return "i64.load";
  if (type == BinaryenTypeFloat32())
    return "f32.load";
  if (bytes == 1)
    return signed_load ? "i32.load8_s" : "i32.load8_u";
  if (bytes == 2)
    return signed_load ? "i32.load16_s" : "i32.load16_u";
  return "i32.load";
}

/* Names core expression kinds that the restricted frontend does not lower. */
/* Produces a useful opcode family name for unsupported Binaryen expressions. */
static const char *unsupported_expression_opcode(BinaryenExpressionId id) {
  if (id == BinaryenNopId())
    return "nop";
  if (id == BinaryenCallIndirectId())
    return "call_indirect";
  if (id == BinaryenGlobalGetId())
    return "global.get";
  if (id == BinaryenGlobalSetId())
    return "global.set";
  if (id == BinaryenMemoryInitId())
    return "memory.init";
  if (id == BinaryenDataDropId())
    return "data.drop";
  if (id == BinaryenMemoryCopyId())
    return "memory.copy";
  if (id == BinaryenMemoryFillId())
    return "memory.fill";
  if (id == BinaryenMemorySizeId())
    return "memory.size";
  if (id == BinaryenMemoryGrowId())
    return "memory.grow";
  if (id == BinaryenRefNullId())
    return "ref.null";
  if (id == BinaryenRefFuncId())
    return "ref.func";
  if (id == BinaryenTableGetId())
    return "table.get";
  if (id == BinaryenTableSetId())
    return "table.set";
  return "unknown expression";
}

/* Converts one Binaryen expression recursively into a compiler-owned tree. */
static WasmExpr *convert_expression(BinaryenExpressionRef source, Diagnostics *diagnostics,
                                    const DecodeContext *context, const char *path) {
  BinaryenExpressionId id;
  WasmExpr *expression;
  BinaryenIndex index;
  if (source == NULL) {
    expression_error(diagnostics, context, "missing expression", path, "expression is missing");
    return NULL;
  }
  id = BinaryenExpressionGetId(source);
  if (id == BinaryenBlockId()) {
    expression = new_expression(WASM_EXPR_BLOCK, "block", path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->name = copy_string(BinaryenBlockGetName(source));
    if (!allocate_children(expression, BinaryenBlockGetNumChildren(source), diagnostics))
      goto fail;
    for (index = 0; index < expression->child_count; ++index) {
      expression->children[index] =
          convert_child(BinaryenBlockGetChildAt(source, index), diagnostics, context, path, index);
      if (expression->children[index] == NULL)
        goto fail;
    }
    return expression;
  }
  if (id == BinaryenLoopId()) {
    BinaryenType loop_type;
    expression = new_expression(WASM_EXPR_LOOP, "loop", path, diagnostics);
    if (expression == NULL)
      return NULL;
    loop_type = BinaryenExpressionGetType(source);
    expression->value_type = loop_type == BinaryenTypeNone() || loop_type == BinaryenTypeUnreachable()
                                 ? WASM_VALUE_NONE
                                 : convert_type(loop_type);
    expression->name = copy_string(BinaryenLoopGetName(source));
    if (expression->name == NULL) {
      expression_error(diagnostics, context, expression->opcode, path, "loop has no target name");
      goto fail;
    }
    if (!allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenLoopGetBody(source), diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenBreakId()) {
    expression = new_expression(WASM_EXPR_BR, "br", path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->name = copy_string(BinaryenBreakGetName(source));
    if (expression->name == NULL) {
      expression_error(diagnostics, context, expression->opcode, path, "branch has no target");
      goto fail;
    }
    if (BinaryenBreakGetValue(source) != NULL) {
      expression_error(diagnostics, context, expression->opcode, path, "value-carrying branches are unsupported");
      goto fail;
    }
    if (BinaryenBreakGetCondition(source) != NULL) {
      expression->kind = WASM_EXPR_BR_IF;
      expression->opcode = "br_if";
      if (!allocate_children(expression, 1, diagnostics))
        goto fail;
      expression->children[0] = convert_child(BinaryenBreakGetCondition(source), diagnostics, context, path, 0);
      if (expression->children[0] == NULL)
        goto fail;
    }
    return expression;
  }
  if (id == BinaryenSwitchId()) {
    BinaryenExpressionRef condition;
    expression = new_expression(WASM_EXPR_BR_TABLE, "br_table", path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->name = copy_string(BinaryenSwitchGetDefaultName(source));
    if (expression->name == NULL) {
      expression_error(diagnostics, context, expression->opcode, path, "br_table has no default target");
      goto fail;
    }
    if (BinaryenSwitchGetValue(source) != NULL) {
      expression_error(diagnostics, context, expression->opcode, path,
                       "value-carrying br_table branches are unsupported");
      goto fail;
    }
    condition = BinaryenSwitchGetCondition(source);
    if (BinaryenExpressionGetType(condition) != BinaryenTypeInt32()) {
      expression_error(diagnostics, context, expression->opcode, path, "br_table selector must be i32");
      goto fail;
    }
    expression->branch_target_count = BinaryenSwitchGetNumNames(source);
    if (expression->branch_target_count != 0) {
      expression->branch_targets = calloc(expression->branch_target_count, sizeof(*expression->branch_targets));
      if (expression->branch_targets == NULL) {
        diagnostics_error(diagnostics, "out of memory decoding br_table");
        goto fail;
      }
      for (index = 0; index < expression->branch_target_count; ++index) {
        expression->branch_targets[index] = copy_string(BinaryenSwitchGetNameAt(source, index));
        if (expression->branch_targets[index] == NULL) {
          expression_error(diagnostics, context, expression->opcode, path, "br_table case has no target");
          goto fail;
        }
      }
    }
    if (!allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(condition, diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenCallId()) {
    expression = new_expression(WASM_EXPR_CALL, "call", path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->name = copy_string(BinaryenCallGetTarget(source));
    if (expression->name == NULL || !allocate_children(expression, BinaryenCallGetNumOperands(source), diagnostics))
      goto fail;
    for (index = 0; index < expression->child_count; ++index) {
      expression->children[index] =
          convert_child(BinaryenCallGetOperandAt(source, index), diagnostics, context, path, index);
      if (expression->children[index] == NULL)
        goto fail;
    }
    return expression;
  }
  if (id == BinaryenConstId()) {
    if (BinaryenExpressionGetType(source) == BinaryenTypeInt32()) {
      expression = new_expression(WASM_EXPR_I32_CONST, "i32.const", path, diagnostics);
      if (expression != NULL)
        expression->i32_value = BinaryenConstGetValueI32(source);
      return expression;
    }
    if (BinaryenExpressionGetType(source) == BinaryenTypeFloat32()) {
      expression = new_expression(WASM_EXPR_F32_CONST, "f32.const", path, diagnostics);
      if (expression != NULL)
        expression->f32_value = BinaryenConstGetValueF32(source);
      return expression;
    }
    expression_error(diagnostics, context,
                     BinaryenExpressionGetType(source) == BinaryenTypeInt64() ? "i64.const" : "const", path,
                     "unsupported constant type");
    return NULL;
  }
  if (id == BinaryenUnreachableId())
    return new_expression(WASM_EXPR_UNREACHABLE, "unreachable", path, diagnostics);
  if (id == BinaryenIfId()) {
    expression = new_expression(WASM_EXPR_IF, "if", path, diagnostics);
    if (expression == NULL)
      return NULL;
    if (BinaryenExpressionGetType(source) != BinaryenTypeNone()) {
      expression_error(diagnostics, context, expression->opcode, path, "value-producing if is unsupported");
      goto fail;
    }
    if (!allocate_children(expression, BinaryenIfGetIfFalse(source) == NULL ? 2 : 3, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenIfGetCondition(source), diagnostics, context, path, 0);
    expression->children[1] = convert_child(BinaryenIfGetIfTrue(source), diagnostics, context, path, 1);
    if (expression->children[0] == NULL || expression->children[1] == NULL)
      goto fail;
    if (expression->child_count == 3) {
      expression->children[2] = convert_child(BinaryenIfGetIfFalse(source), diagnostics, context, path, 2);
      if (expression->children[2] == NULL)
        goto fail;
    }
    return expression;
  }
  if (id == BinaryenLocalGetId()) {
    expression = new_expression(WASM_EXPR_LOCAL_GET, "local.get", path, diagnostics);
    if (expression != NULL)
      expression->index = BinaryenLocalGetGetIndex(source);
    return expression;
  }
  if (id == BinaryenLocalSetId()) {
    expression = new_expression(WASM_EXPR_LOCAL_SET, "local.set", path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->index = BinaryenLocalSetGetIndex(source);
    expression->is_tee = BinaryenLocalSetIsTee(source);
    if (expression->is_tee)
      expression->opcode = "local.tee";
    if (!allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenLocalSetGetValue(source), diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenGlobalGetId()) {
    const char *name = BinaryenGlobalGetGetName(source);
    bool is_stack_pointer =
        context->module->stack_pointer_global_is_valid && strcmp(name, context->module->stack_pointer_name) == 0;
    expression = new_expression(is_stack_pointer ? WASM_EXPR_STACK_POINTER_GET : WASM_EXPR_GLOBAL_GET, "global.get",
                                path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->name = copy_string(name);
    if (expression->name == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenGlobalSetId()) {
    const char *name = BinaryenGlobalSetGetName(source);
    bool is_stack_pointer =
        context->module->stack_pointer_global_is_valid && strcmp(name, context->module->stack_pointer_name) == 0;
    expression = new_expression(is_stack_pointer ? WASM_EXPR_STACK_POINTER_SET : WASM_EXPR_GLOBAL_SET, "global.set",
                                path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->name = copy_string(name);
    if (expression->name == NULL || !allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenGlobalSetGetValue(source), diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenLoadId()) {
    expression = new_expression(WASM_EXPR_LOAD, load_opcode(source), path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->bytes = BinaryenLoadGetBytes(source);
    expression->value_type = convert_type(BinaryenExpressionGetType(source));
    expression->offset = BinaryenLoadGetOffset(source);
    expression->align = BinaryenLoadGetAlign(source);
    expression->is_signed = BinaryenLoadIsSigned(source);
    if (!allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenLoadGetPtr(source), diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenStoreId()) {
    expression = new_expression(WASM_EXPR_STORE,
                                BinaryenStoreGetBytes(source) == 8                           ? "i64.store"
                                : BinaryenStoreGetBytes(source) == 1                         ? "i32.store8"
                                : BinaryenStoreGetValueType(source) == BinaryenTypeFloat32() ? "f32.store"
                                                                                             : "i32.store",
                                path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->bytes = BinaryenStoreGetBytes(source);
    expression->value_type = convert_type(BinaryenStoreGetValueType(source));
    expression->offset = BinaryenStoreGetOffset(source);
    expression->align = BinaryenStoreGetAlign(source);
    if (expression->bytes == 8) {
      BinaryenExpressionRef stored_value = BinaryenStoreGetValue(source);
      BinaryenExpressionRef packed_high, packed_low;
      if (BinaryenExpressionGetId(stored_value) == BinaryenLocalSetId() && BinaryenLocalSetIsTee(stored_value) &&
          BinaryenExpressionGetType(stored_value) == BinaryenTypeInt64() &&
          decoded_local_type(context, BinaryenLocalSetGetIndex(stored_value)) == WASM_VALUE_I64) {
        BinaryenExpressionRef loaded = BinaryenLocalSetGetValue(stored_value);
        if (BinaryenExpressionGetId(loaded) == BinaryenLoadId() &&
            BinaryenExpressionGetType(loaded) == BinaryenTypeInt64() && BinaryenLoadGetBytes(loaded) == 8) {
          /* Zig's observable aggregate copy holds this exact loaded
           * pair in a local before storing and extracting its words. */
          expression->kind = WASM_EXPR_I64_LOAD_STORE_LOCAL_TEE;
          expression->index = BinaryenLocalSetGetIndex(stored_value);
          expression->source_offset = BinaryenLoadGetOffset(loaded);
          if (!allocate_children(expression, 2, diagnostics))
            goto fail;
          expression->children[0] = convert_child(BinaryenStoreGetPtr(source), diagnostics, context, path, 0);
          expression->children[1] = convert_child(BinaryenLoadGetPtr(loaded), diagnostics, context, path, 1);
          if (expression->children[0] == NULL || expression->children[1] == NULL)
            goto fail;
          return expression;
        }
      }
      if (BinaryenStoreGetValueType(source) == BinaryenTypeInt64() &&
          BinaryenExpressionGetId(stored_value) == BinaryenLoadId() && BinaryenLoadGetBytes(stored_value) == 8) {
        /* Zig uses an i64 load/store pair as an eight-byte aggregate
         * transport. Keep it separate from general i64 values. */
        expression->kind = WASM_EXPR_I64_LOAD_STORE;
        expression->source_offset = BinaryenLoadGetOffset(stored_value);
        if (!allocate_children(expression, 2, diagnostics))
          goto fail;
        expression->children[0] = convert_child(BinaryenStoreGetPtr(source), diagnostics, context, path, 0);
        expression->children[1] = convert_child(BinaryenLoadGetPtr(stored_value), diagnostics, context, path, 1);
        if (expression->children[0] == NULL || expression->children[1] == NULL)
          goto fail;
        return expression;
      }
      if (BinaryenStoreGetValueType(source) == BinaryenTypeInt64() &&
          packed_i64_words(stored_value, &packed_high, &packed_low)) {
        /* Zig represents a computed two-i32 aggregate as a zero-extend,
         * high-word shift, and OR immediately consumed by i64.store. */
        expression->kind = WASM_EXPR_I64_PACKED_I32_STORE;
        if (!allocate_children(expression, 3, diagnostics))
          goto fail;
        expression->children[0] = convert_child(BinaryenStoreGetPtr(source), diagnostics, context, path, 0);
        expression->children[1] = convert_child(packed_high, diagnostics, context, path, 1);
        expression->children[2] = convert_child(packed_low, diagnostics, context, path, 2);
        if (expression->children[0] == NULL || expression->children[1] == NULL || expression->children[2] == NULL)
          goto fail;
        return expression;
      }
      /* This is intentionally not general i64 support. Clang can fold
       * neighbouring i32 initializers into this exact store shape. */
      if (BinaryenStoreGetValueType(source) != BinaryenTypeInt64() ||
          BinaryenExpressionGetId(stored_value) != BinaryenConstId() ||
          BinaryenExpressionGetType(stored_value) != BinaryenTypeInt64()) {
        expression_error(diagnostics, context, expression->opcode, path,
                         "only a literal i64.const initializer, direct i64.load aggregate "
                         "transfer, or exact two-i32 aggregate packing form is accepted");
        goto fail;
      }
      expression->kind = WASM_EXPR_I64_CONST_STORE;
      expression->i64_value = (uint64_t)BinaryenConstGetValueI64(stored_value);
      if (!allocate_children(expression, 1, diagnostics))
        goto fail;
      expression->children[0] = convert_child(BinaryenStoreGetPtr(source), diagnostics, context, path, 0);
      if (expression->children[0] == NULL)
        goto fail;
      return expression;
    }
    if (!allocate_children(expression, 2, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenStoreGetPtr(source), diagnostics, context, path, 0);
    expression->children[1] = convert_child(BinaryenStoreGetValue(source), diagnostics, context, path, 1);
    if (expression->children[0] == NULL || expression->children[1] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenMemoryCopyId()) {
    expression = new_expression(WASM_EXPR_MEMORY_COPY, "memory.copy", path, diagnostics);
    if (expression == NULL || !allocate_children(expression, 3, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenMemoryCopyGetDest(source), diagnostics, context, path, 0);
    expression->children[1] = convert_child(BinaryenMemoryCopyGetSource(source), diagnostics, context, path, 1);
    expression->children[2] = convert_child(BinaryenMemoryCopyGetSize(source), diagnostics, context, path, 2);
    if (expression->children[0] == NULL || expression->children[1] == NULL || expression->children[2] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenMemoryFillId()) {
    expression = new_expression(WASM_EXPR_MEMORY_FILL, "memory.fill", path, diagnostics);
    if (expression == NULL || !allocate_children(expression, 3, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenMemoryFillGetDest(source), diagnostics, context, path, 0);
    expression->children[1] = convert_child(BinaryenMemoryFillGetValue(source), diagnostics, context, path, 1);
    expression->children[2] = convert_child(BinaryenMemoryFillGetSize(source), diagnostics, context, path, 2);
    if (expression->children[0] == NULL || expression->children[1] == NULL || expression->children[2] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenUnaryId()) {
    BinaryenOp op = BinaryenUnaryGetOp(source);
    if (op == BinaryenWrapInt64()) {
      BinaryenExpressionRef input = BinaryenUnaryGetValue(source);
      BinaryenExpressionRef loaded = input;
      uint64_t shift = 0;
      if (BinaryenExpressionGetId(input) == BinaryenBinaryId() && BinaryenBinaryGetOp(input) == BinaryenShrUInt64()) {
        BinaryenExpressionRef amount = BinaryenBinaryGetRight(input);
        if (BinaryenExpressionGetId(amount) != BinaryenConstId() ||
            BinaryenExpressionGetType(amount) != BinaryenTypeInt64())
          loaded = NULL;
        else {
          loaded = BinaryenBinaryGetLeft(input);
          shift = (uint64_t)BinaryenConstGetValueI64(amount) & 63u;
        }
      }
      if (loaded != NULL && BinaryenExpressionGetId(loaded) == BinaryenLoadId() &&
          BinaryenExpressionGetType(loaded) == BinaryenTypeInt64() && BinaryenLoadGetBytes(loaded) == 8) {
        /* This keeps a common word extraction in the Wasm frontend;
         * it does not create a general i64 compiler value. */
        expression = new_expression(WASM_EXPR_I64_WORD_EXTRACT, "i32.wrap_i64", path, diagnostics);
        if (expression == NULL || !allocate_children(expression, 1, diagnostics))
          goto fail;
        expression->source_offset = BinaryenLoadGetOffset(loaded);
        expression->i64_value = shift;
        expression->children[0] = convert_child(BinaryenLoadGetPtr(loaded), diagnostics, context, path, 0);
        if (expression->children[0] == NULL)
          goto fail;
        return expression;
      }
      if (loaded != NULL && BinaryenExpressionGetId(loaded) == BinaryenLocalGetId() &&
          BinaryenExpressionGetType(loaded) == BinaryenTypeInt64() &&
          decoded_local_type(context, BinaryenLocalGetGetIndex(loaded)) == WASM_VALUE_I64) {
        /* This consumes only a compiler-owned two-word i64 local. */
        expression = new_expression(WASM_EXPR_I64_LOCAL_WORD_EXTRACT, "i32.wrap_i64", path, diagnostics);
        if (expression == NULL)
          goto fail;
        expression->index = BinaryenLocalGetGetIndex(loaded);
        expression->i64_value = shift;
        return expression;
      }
      if (loaded != NULL && BinaryenExpressionGetId(loaded) == BinaryenLocalSetId() && BinaryenLocalSetIsTee(loaded) &&
          BinaryenExpressionGetType(loaded) == BinaryenTypeInt64() &&
          decoded_local_type(context, BinaryenLocalSetGetIndex(loaded)) == WASM_VALUE_I64) {
        BinaryenExpressionRef load = BinaryenLocalSetGetValue(loaded);
        if (BinaryenExpressionGetId(load) == BinaryenLoadId() &&
            BinaryenExpressionGetType(load) == BinaryenTypeInt64() && BinaryenLoadGetBytes(load) == 8) {
          /* Zig seeds a pair local while extracting its first
           * computed field. Keep the pair as two frame words. */
          expression = new_expression(WASM_EXPR_I64_LOCAL_TEE_WORD_EXTRACT, "i32.wrap_i64", path, diagnostics);
          if (expression == NULL || !allocate_children(expression, 1, diagnostics))
            goto fail;
          expression->index = BinaryenLocalSetGetIndex(loaded);
          expression->source_offset = BinaryenLoadGetOffset(load);
          expression->i64_value = shift;
          expression->children[0] = convert_child(BinaryenLoadGetPtr(load), diagnostics, context, path, 0);
          if (expression->children[0] == NULL)
            goto fail;
          return expression;
        }
      }
    }
    expression = new_expression(WASM_EXPR_UNARY, unary_opcode(op), path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->unary_op = op == BinaryenEqZInt32()                  ? WASM_UNARY_EQZ
                           : op == BinaryenExtendS8Int32()           ? WASM_UNARY_EXTEND8_S
                           : op == BinaryenExtendS16Int32()          ? WASM_UNARY_EXTEND16_S
                           : op == BinaryenConvertSInt32ToFloat32()  ? WASM_UNARY_CONVERT_I32_S_TO_F32
                           : op == BinaryenConvertUInt32ToFloat32()  ? WASM_UNARY_CONVERT_I32_U_TO_F32
                           : op == BinaryenTruncSatSFloat32ToInt32() ? WASM_UNARY_TRUNC_SAT_F32_TO_I32
                           : op == BinaryenReinterpretFloat32()      ? WASM_UNARY_REINTERPRET_F32_TO_I32
                           : op == BinaryenReinterpretInt32()        ? WASM_UNARY_REINTERPRET_I32_TO_F32
                           : op == BinaryenNegFloat32()              ? WASM_UNARY_F32_NEG
                           : op == BinaryenAbsFloat32()              ? WASM_UNARY_F32_ABS
                           : op == BinaryenFloorFloat32()            ? WASM_UNARY_F32_FLOOR
                           : op == BinaryenCeilFloat32()             ? WASM_UNARY_F32_CEIL
                                                                     : WASM_UNARY_OTHER;
    if (!allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenUnaryGetValue(source), diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenBinaryId()) {
    BinaryenOp op = BinaryenBinaryGetOp(source);
    expression = new_expression(WASM_EXPR_BINARY, binary_opcode(op), path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->binary_op = op == BinaryenAddInt32()     ? WASM_BINARY_ADD
                            : op == BinaryenSubInt32()   ? WASM_BINARY_SUB
                            : op == BinaryenMulInt32()   ? WASM_BINARY_MUL
                            : op == BinaryenDivSInt32()  ? WASM_BINARY_DIV_S
                            : op == BinaryenDivUInt32()  ? WASM_BINARY_DIV_U
                            : op == BinaryenRemSInt32()  ? WASM_BINARY_REM_S
                            : op == BinaryenShlInt32()   ? WASM_BINARY_SHL
                            : op == BinaryenShrSInt32()  ? WASM_BINARY_SHR_S
                            : op == BinaryenAndInt32()   ? WASM_BINARY_AND
                            : op == BinaryenXorInt32()   ? WASM_BINARY_XOR
                            : op == BinaryenEqInt32()    ? WASM_BINARY_EQ
                            : op == BinaryenNeInt32()    ? WASM_BINARY_NE
                            : op == BinaryenLtSInt32()   ? WASM_BINARY_LT_S
                            : op == BinaryenLtUInt32()   ? WASM_BINARY_LT_U
                            : op == BinaryenGtSInt32()   ? WASM_BINARY_GT_S
                            : op == BinaryenGtUInt32()   ? WASM_BINARY_GT_U
                            : op == BinaryenGeSInt32()   ? WASM_BINARY_GE_S
                            : op == BinaryenGeUInt32()   ? WASM_BINARY_GE_U
                            : op == BinaryenLeSInt32()   ? WASM_BINARY_LE_S
                            : op == BinaryenLeUInt32()   ? WASM_BINARY_LE_U
                            : op == BinaryenOrInt32()    ? WASM_BINARY_OR
                            : op == BinaryenRemUInt32()  ? WASM_BINARY_REM_U
                            : op == BinaryenShrUInt32()  ? WASM_BINARY_SHR_U
                            : op == BinaryenRotLInt32()  ? WASM_BINARY_ROTL
                            : op == BinaryenRotRInt32()  ? WASM_BINARY_ROTR
                            : op == BinaryenAddFloat32() ? WASM_BINARY_F32_ADD
                            : op == BinaryenSubFloat32() ? WASM_BINARY_F32_SUB
                            : op == BinaryenEqFloat32()  ? WASM_BINARY_F32_EQ
                            : op == BinaryenNeFloat32()  ? WASM_BINARY_F32_NE
                            : op == BinaryenLeFloat32()  ? WASM_BINARY_F32_LE
                            : op == BinaryenLtFloat32()  ? WASM_BINARY_F32_LT
                            : op == BinaryenMulFloat32() ? WASM_BINARY_F32_MUL
                            : op == BinaryenDivFloat32() ? WASM_BINARY_F32_DIV
                            : op == BinaryenGtFloat32()  ? WASM_BINARY_F32_GT
                            : op == BinaryenGeFloat32()  ? WASM_BINARY_F32_GE
                                                         : WASM_BINARY_OTHER;
    if (!allocate_children(expression, 2, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenBinaryGetLeft(source), diagnostics, context, path, 0);
    expression->children[1] = convert_child(BinaryenBinaryGetRight(source), diagnostics, context, path, 1);
    if (expression->children[0] == NULL || expression->children[1] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenSelectId()) {
    expression = new_expression(WASM_EXPR_SELECT, "select", path, diagnostics);
    if (expression == NULL)
      return NULL;
    expression->value_type = convert_type(BinaryenExpressionGetType(source));
    if (expression->value_type != WASM_VALUE_I32 && expression->value_type != WASM_VALUE_F32) {
      expression_error(diagnostics, context, expression->opcode, path, "unsupported select result type");
      goto fail;
    }
    /* Preserve Wasm evaluation order: first value, second value, condition. */
    if (!allocate_children(expression, 3, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenSelectGetIfTrue(source), diagnostics, context, path, 0);
    expression->children[1] = convert_child(BinaryenSelectGetIfFalse(source), diagnostics, context, path, 1);
    expression->children[2] = convert_child(BinaryenSelectGetCondition(source), diagnostics, context, path, 2);
    if (expression->children[0] == NULL || expression->children[1] == NULL || expression->children[2] == NULL)
      goto fail;
    return expression;
  }
  if (id == BinaryenReturnId()) {
    expression = new_expression(WASM_EXPR_RETURN, "return", path, diagnostics);
    if (expression == NULL)
      return NULL;
    if (BinaryenReturnGetValue(source) != NULL) {
      if (!allocate_children(expression, 1, diagnostics))
        goto fail;
      expression->children[0] = convert_child(BinaryenReturnGetValue(source), diagnostics, context, path, 0);
      if (expression->children[0] == NULL)
        goto fail;
    }
    return expression;
  }
  if (id == BinaryenDropId()) {
    expression = new_expression(WASM_EXPR_DROP, "drop", path, diagnostics);
    if (expression == NULL)
      return NULL;
    if (!allocate_children(expression, 1, diagnostics))
      goto fail;
    expression->children[0] = convert_child(BinaryenDropGetValue(source), diagnostics, context, path, 0);
    if (expression->children[0] == NULL)
      goto fail;
    return expression;
  }
  expression_error(diagnostics, context, unsupported_expression_opcode(id), path, "unsupported Wasm expression kind %u",
                   (unsigned)id);
  return NULL;
fail:
  free_expression(expression);
  return NULL;
}

/* Reads the original Wasm bytes for metadata Binaryen does not expose directly.
 */
static bool read_file(const char *path, char **contents, size_t *size, Diagnostics *diagnostics) {
  FILE *file = fopen(path, "rb");
  long length;
  char *buffer;
  if (file == NULL) {
    diagnostics_error(diagnostics, "cannot open input module '%s'", path);
    return false;
  }
  if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
    diagnostics_error(diagnostics, "cannot determine size of input module '%s'", path);
    fclose(file);
    return false;
  }
  buffer = malloc(length == 0 ? 1 : (size_t)length);
  if (buffer == NULL || (length != 0 && fread(buffer, 1, (size_t)length, file) != (size_t)length)) {
    diagnostics_error(diagnostics, "cannot read input module '%s'", path);
    free(buffer);
    fclose(file);
    return false;
  }
  fclose(file);
  *contents = buffer;
  *size = (size_t)length;
  return true;
}

/* Returns a readable name for one scalar/reference Wasm value type. */
static const char *binaryen_type_name(BinaryenType type) {
  if (type == BinaryenTypeNone())
    return "none";
  if (type == BinaryenTypeInt32())
    return "i32";
  if (type == BinaryenTypeInt64())
    return "i64";
  if (type == BinaryenTypeFloat32())
    return "f32";
  if (type == BinaryenTypeFloat64())
    return "f64";
  if (type == BinaryenTypeVec128())
    return "v128";
  if (type == BinaryenTypeFuncref())
    return "funcref";
  if (type == BinaryenTypeExternref())
    return "externref";
  if (type == BinaryenTypeUnreachable())
    return "unreachable";
  return "other";
}

/* Prints a Binaryen function parameter or result tuple without restricting it.
 */
static void print_binaryen_type_tuple(FILE *stream, BinaryenType type) {
  BinaryenIndex arity = BinaryenTypeArity(type), index;
  BinaryenType *items;
  if (arity == 0) {
    fputs("()", stream);
    return;
  }
  items = calloc(arity, sizeof(*items));
  if (items == NULL) {
    fputs("(<out of memory>)", stream);
    return;
  }
  BinaryenTypeExpand(type, items);
  fputc('(', stream);
  for (index = 0; index < arity; ++index) {
    if (index != 0)
      fputs(", ", stream);
    fputs(binaryen_type_name(items[index]), stream);
  }
  fputc(')', stream);
  free(items);
}

/* Writes an intentionally non-validating Binaryen module inventory for users.
 */
bool wasm_module_report_profile(const char *path, FILE *stream, Diagnostics *diagnostics) {
  char *contents = NULL, *text = NULL;
  size_t size = 0;
  BinaryenModuleRef source = NULL;
  BinaryenIndex index, import_count = 0;
  bool success = false;

  if (!read_file(path, &contents, &size, diagnostics))
    goto done;
  source = BinaryenModuleReadWithFeatures(contents, size, BinaryenFeatureAll());
  if (source == NULL || !BinaryenModuleValidate(source)) {
    diagnostics_error(diagnostics, "Binaryen could not load '%s' as a valid Wasm module", path);
    goto done;
  }
  text = BinaryenModuleAllocateAndWriteText(source);
  if (text == NULL) {
    diagnostics_error(diagnostics, "Binaryen could not print '%s' as Wasm text", path);
    goto done;
  }
  for (index = 0; index < BinaryenGetNumFunctions(source); ++index) {
    BinaryenFunctionRef function = BinaryenGetFunctionByIndex(source, index);
    const char *import_module = BinaryenFunctionImportGetModule(function);
    if (import_module != NULL && import_module[0] != '\0')
      ++import_count;
  }

  fprintf(stream, "VirconWasm profile report\n");
  fprintf(stream, "input: %s\n", path);
  fprintf(stream, "Binaryen features: 0x%08X\n", (unsigned)BinaryenModuleGetFeatures(source));
  fprintf(stream, "functions: %u (%u imports, %u defined)\n", (unsigned)BinaryenGetNumFunctions(source),
          (unsigned)import_count, (unsigned)(BinaryenGetNumFunctions(source) - import_count));
  fprintf(stream, "exports: %u\n", (unsigned)BinaryenGetNumExports(source));
  fprintf(stream, "globals: %u\n", (unsigned)BinaryenGetNumGlobals(source));
  fprintf(stream, "tables: %u\n", (unsigned)BinaryenGetNumTables(source));
  fprintf(stream, "element segments: %u\n", (unsigned)BinaryenGetNumElementSegments(source));
  fprintf(stream, "data segments: %u\n", (unsigned)BinaryenGetNumDataSegments(source));
  fprintf(stream, "memory: %s\n", BinaryenHasMemory(source) ? "present" : "absent");
  fputs("\nFunctions:\n", stream);
  for (index = 0; index < BinaryenGetNumFunctions(source); ++index) {
    BinaryenFunctionRef function = BinaryenGetFunctionByIndex(source, index);
    const char *name = BinaryenFunctionGetName(function);
    const char *import_module = BinaryenFunctionImportGetModule(function);
    const char *import_name = BinaryenFunctionImportGetBase(function);
    fprintf(stream, "  [%u] %s", (unsigned)index, name != NULL && name[0] != '\0' ? name : "<unnamed>");
    if (import_module != NULL && import_module[0] != '\0')
      fprintf(stream, " import %s.%s", import_module, import_name != NULL ? import_name : "<unnamed>");
    else
      fprintf(stream, " defined locals=%u", (unsigned)BinaryenFunctionGetNumVars(function));
    fputs(" params=", stream);
    print_binaryen_type_tuple(stream, BinaryenFunctionGetParams(function));
    fputs(" results=", stream);
    print_binaryen_type_tuple(stream, BinaryenFunctionGetResults(function));
    fputc('\n', stream);
  }
  fputs("\nBinaryen Wasm text (complete module; inspect this for every "
        "expression):\n",
        stream);
  fputs(text, stream);
  if (text[0] != '\0' && text[strlen(text) - 1] != '\n')
    fputc('\n', stream);
  success = ferror(stream) == 0;
  if (!success)
    diagnostics_error(diagnostics, "cannot write Wasm profile report");
done:
  free(text);
  free(contents);
  if (source != NULL)
    BinaryenModuleDispose(source);
  return success;
}

/* Binaryen 130 has no memory enumerator. Its own text form supplies the sole
 * core-memory name and limits; Binaryen still owns all binary decoding. */
/* Extracts memory declarations from Binaryen's printed module representation.
 */
static bool read_memory_text(const char *text, WasmModule *module, Diagnostics *diagnostics) {
  const char *cursor = text;
  while ((cursor = strstr(cursor, "(memory")) != NULL) {
    const char *p = cursor + 7, *end = strchr(cursor, ')');
    if (end == NULL) {
      diagnostics_error(diagnostics, "Binaryen produced malformed memory text");
      return false;
    }
    while (p < end && isspace((unsigned char)*p))
      ++p;
    if (p < end && *p == '$')
      while (p < end && !isspace((unsigned char)*p))
        ++p;
    while (p < end && isspace((unsigned char)*p))
      ++p;
    /* An export contains `(memory $name)` too; only a declaration has a
     * page count after its optional name. */
    if (p >= end || !isdigit((unsigned char)*p)) {
      cursor = end + 1;
      continue;
    }
    ++module->memory_count;
    if (module->memory_count == 1) {
      char *after;
      unsigned long initial = strtoul(p, &after, 10);
      if (after == p || initial > UINT32_MAX) {
        diagnostics_error(diagnostics, "could not read Wasm memory initial size");
        return false;
      }
      module->memory_initial_pages = (uint32_t)initial;
      p = after;
      while (p < end && isspace((unsigned char)*p))
        ++p;
      if (p < end && isdigit((unsigned char)*p)) {
        unsigned long maximum = strtoul(p, &after, 10);
        if (maximum > UINT32_MAX)
          return false;
        module->memory_has_max = true;
        module->memory_max_pages = (uint32_t)maximum;
      }
      module->memory_is_shared = strstr(cursor, "shared") != NULL && strstr(cursor, "shared") < end;
      module->memory_is_64 = strstr(cursor, "i64") != NULL && strstr(cursor, "i64") < end;
    }
    cursor = end + 1;
  }
  module->has_memory = module->memory_count != 0;
  return true;
}

/* Detects an imported memory in Binaryen's printed module representation. */
static bool text_has_memory_import(const char *text) {
  const char *cursor = text;
  while ((cursor = strstr(cursor, "(import")) != NULL) {
    const char *end = strchr(cursor, ')'), *memory = strstr(cursor, "(memory");
    if (end != NULL && memory != NULL && memory < end)
      return true;
    cursor += 7;
  }
  return false;
}

/* Checks that printed active-data offsets use the supported constant form. */
static bool text_data_offsets_are_const(const char *text, size_t count) {
  const char *cursor = text;
  size_t seen = 0;
  while ((cursor = strstr(cursor, "(data")) != NULL) {
    const char *next = strstr(cursor + 5, "(data"), *constant = strstr(cursor, "(i32.const");
    if (constant == NULL || (next != NULL && constant > next))
      return false;
    ++seen;
    cursor += 5;
  }
  return seen == count;
}

/* Finds the public export name when Binaryen has no retained name-section name.
 */
/* Finds the public export name associated with one internal function name. */
static const char *exported_function_name(const WasmModule *module, const char *internal_name) {
  size_t index;
  for (index = 0; index < module->export_count; ++index) {
    const WasmExport *export_ref = &module->exports[index];
    if (export_ref->is_function && strcmp(export_ref->value, internal_name) == 0)
      return export_ref->name;
  }
  return NULL;
}

/* Records the narrow, linker-defined stack-pointer global if its declaration is
 * valid. */
/* Recognizes the single documented linker stack-pointer global exception. */
static bool read_stack_pointer_global(BinaryenModuleRef source, WasmModule *module, Diagnostics *diagnostics) {
  BinaryenGlobalRef global;
  BinaryenExpressionRef initializer;
  const char *name, *import_module;

  if (module->global_count != 1)
    return true;
  global = BinaryenGetGlobalByIndex(source, 0);
  name = BinaryenGlobalGetName(global);
  if (name == NULL || strcmp(name, "__stack_pointer") != 0)
    return true;
  module->has_stack_pointer_global = true;
  import_module = BinaryenGlobalImportGetModule(global);
  initializer = BinaryenGlobalGetInitExpr(global);
  if (!BinaryenGlobalIsMutable(global) || BinaryenGlobalGetType(global) != BinaryenTypeInt32() ||
      (import_module != NULL && import_module[0] != '\0') || initializer == NULL ||
      BinaryenExpressionGetId(initializer) != BinaryenConstId() ||
      BinaryenExpressionGetType(initializer) != BinaryenTypeInt32())
    return true;
  module->stack_pointer_name = copy_string(name);
  if (module->stack_pointer_name == NULL) {
    diagnostics_error(diagnostics, "out of memory recording Wasm stack-pointer global");
    return false;
  }
  module->stack_pointer_initial = (uint32_t)BinaryenConstGetValueI32(initializer);
  module->stack_pointer_global_is_valid = true;
  return true;
}

/* Copies Binaryen data segments into the compiler-owned module representation.
 */
static bool read_data_segments(BinaryenModuleRef source, WasmModule *module, Diagnostics *diagnostics) {
  BinaryenIndex index;
  module->data_segment_count = BinaryenGetNumDataSegments(source);
  module->data_segments = calloc(module->data_segment_count, sizeof(*module->data_segments));
  if (module->data_segment_count != 0 && module->data_segments == NULL) {
    diagnostics_error(diagnostics, "out of memory reading Wasm data segments");
    return false;
  }
  for (index = 0; index < module->data_segment_count; ++index) {
    BinaryenDataSegmentRef segment = BinaryenGetDataSegmentByIndex(source, index);
    WasmDataSegment *out = &module->data_segments[index];
    out->offset = BinaryenGetDataSegmentByteOffset(source, segment);
    out->size = BinaryenGetDataSegmentByteLength(segment);
    out->is_passive = BinaryenGetDataSegmentPassive(segment);
    out->offset_is_i32_const = true;
    out->bytes = malloc(out->size == 0 ? 1 : out->size);
    if (out->bytes == NULL) {
      diagnostics_error(diagnostics, "out of memory copying Wasm data segment");
      return false;
    }
    BinaryenCopyDataSegmentData(segment, (char *)out->bytes);
  }
  return true;
}

#ifdef USE_EMBEDDED_BINARYEN
/* Returns whether a module has linker scaffolding targeted by the cleanup
 * profile. Hand-authored Wasm without it retains the existing direct-input
 * validation behavior; linked frontend modules normally carry a table, global,
 * or element segment that triggers the profile. */
static bool needs_embedded_normalization(BinaryenModuleRef source) {
  return BinaryenGetNumGlobals(source) != 0 || BinaryenGetNumTables(source) != 0 ||
         BinaryenGetNumElementSegments(source) != 0;
}

/* Restores the input memory image after a cleanup pass removed part of it.
 *
 * Active data is meaningful to this frontend even when Binaryen determines
 * that the Wasm body does not read it: wasm2vircon uses it to initialize the
 * cartridge's writable linear-memory image. */
static bool restore_memory_image(BinaryenModuleRef source, const WasmModule *original, Diagnostics *diagnostics) {
  const char **data = NULL;
  BinaryenExpressionRef *offsets = NULL;
  BinaryenIndex *sizes = NULL;
  bool *passives = NULL;
  size_t index;

  if (original->data_segment_count != 0) {
    data = calloc(original->data_segment_count, sizeof(*data));
    offsets = calloc(original->data_segment_count, sizeof(*offsets));
    sizes = calloc(original->data_segment_count, sizeof(*sizes));
    passives = calloc(original->data_segment_count, sizeof(*passives));
    if (data == NULL || offsets == NULL || sizes == NULL || passives == NULL) {
      diagnostics_error(diagnostics, "out of memory restoring embedded Wasm data segments");
      free(data);
      free(offsets);
      free(sizes);
      free(passives);
      return false;
    }
    for (index = 0; index < original->data_segment_count; ++index) {
      const WasmDataSegment *segment = &original->data_segments[index];
      data[index] = (const char *)segment->bytes;
      offsets[index] = BinaryenConst(source, BinaryenLiteralInt32((int32_t)segment->offset));
      sizes[index] = (BinaryenIndex)segment->size;
      passives[index] = segment->is_passive;
    }
  }
  BinaryenSetMemory(source, original->memory_initial_pages,
                    original->memory_has_max ? original->memory_max_pages : UINT32_MAX, NULL, NULL, data, passives,
                    offsets, sizes, (BinaryenIndex)original->data_segment_count, original->memory_is_shared,
                    original->memory_is_64, "0");
  free(data);
  free(offsets);
  free(sizes);
  free(passives);
  return true;
}

/* Runs the supported narrow Binaryen cleanup profile without creating a file.
 *
 * The compiler-owned decoder still receives a fresh Binaryen module from the
 * optimized bytes. This keeps Binaryen objects at the Wasm frontend boundary
 * rather than leaking them into validation or V32 lowering. */
static BinaryenModuleRef normalize_embedded_binaryen(BinaryenModuleRef source, Diagnostics *diagnostics) {
  static const char *passes[] = {"remove-unused-module-elements", "vacuum"};
  BinaryenModuleAllocateAndWriteResult encoded;
  BinaryenModuleRef normalized;
  WasmModule original_image = {0};
  bool previous_debug_info;
  char *text = BinaryenModuleAllocateAndWriteText(source);

  /* The cleanup pass may remove an unused memory. VirconWasm requires the
   * frontend-visible memory declaration even for a control-only module, so
   * retain its original empty declaration without retaining removed code/data.
   */
  if (text == NULL || !read_memory_text(text, &original_image, diagnostics)) {
    free(text);
    return NULL;
  }
  free(text);
  if (!read_data_segments(source, &original_image, diagnostics)) {
    wasm_module_dispose(&original_image);
    return NULL;
  }

  /* Match the supported external profile's -g setting. Besides diagnostics,
   * this keeps the canonical __stack_pointer global name intact while passes
   * rebuild the module. The setting is process-global in Binaryen, so restore
   * the caller's state as soon as the normalized bytes are produced. */
  previous_debug_info = BinaryenGetDebugInfo();
  BinaryenSetDebugInfo(true);
  BinaryenModuleRunPasses(source, passes, sizeof(passes) / sizeof(*passes));
  if (original_image.has_memory &&
      (!BinaryenHasMemory(source) || BinaryenGetNumDataSegments(source) != original_image.data_segment_count) &&
      !restore_memory_image(source, &original_image, diagnostics)) {
    BinaryenSetDebugInfo(previous_debug_info);
    wasm_module_dispose(&original_image);
    return NULL;
  }
  encoded = BinaryenModuleAllocateAndWrite(source, NULL);
  BinaryenSetDebugInfo(previous_debug_info);
  wasm_module_dispose(&original_image);
  if (encoded.binary == NULL || encoded.binaryBytes == 0) {
    diagnostics_error(diagnostics, "embedded Binaryen could not serialize normalized Wasm");
    free(encoded.binary);
    free(encoded.sourceMap);
    return NULL;
  }
  normalized = BinaryenModuleReadWithFeatures((char *)encoded.binary, encoded.binaryBytes, BinaryenFeatureAll());
  free(encoded.binary);
  free(encoded.sourceMap);
  if (normalized == NULL || !BinaryenModuleValidate(normalized)) {
    diagnostics_error(diagnostics, "embedded Binaryen produced invalid normalized Wasm");
    if (normalized != NULL)
      BinaryenModuleDispose(normalized);
    return NULL;
  }
  return normalized;
}
#endif

/* Loads, validates, and converts one Wasm module through the Binaryen C API. */
bool wasm_module_load(const char *path, bool optimize_input, WasmModule *module, Diagnostics *diagnostics) {
  char *contents = NULL, *text = NULL;
  size_t size = 0;
  BinaryenModuleRef source = NULL;
  BinaryenIndex index;
  memset(module, 0, sizeof(*module));
  if (!read_file(path, &contents, &size, diagnostics))
    return false;
  source = BinaryenModuleReadWithFeatures(contents, size, BinaryenFeatureAll());
  free(contents);
  if (source == NULL || !BinaryenModuleValidate(source)) {
    diagnostics_error(diagnostics, "Binaryen could not load '%s' as a valid Wasm module", path);
    if (source)
      BinaryenModuleDispose(source);
    return false;
  }
#ifdef USE_EMBEDDED_BINARYEN
  if (optimize_input && needs_embedded_normalization(source)) {
    BinaryenModuleRef normalized = normalize_embedded_binaryen(source, diagnostics);
    BinaryenModuleDispose(source);
    if (normalized == NULL)
      return false;
    source = normalized;
  }
#else
  (void)optimize_input;
#endif
  text = BinaryenModuleAllocateAndWriteText(source);
  if (text == NULL || !read_memory_text(text, module, diagnostics))
    goto fail;
  module->has_imported_memory = text_has_memory_import(text);
  module->table_count = BinaryenGetNumTables(source);
  module->global_count = BinaryenGetNumGlobals(source);
  module->element_segment_count = BinaryenGetNumElementSegments(source);
  module->data_segment_count = BinaryenGetNumDataSegments(source);
  if (!read_stack_pointer_global(source, module, diagnostics))
    goto fail;
  if (module->data_segment_count != 0 && !text_data_offsets_are_const(text, module->data_segment_count)) {
    diagnostics_error(diagnostics, "active data segments must use constant i32 offsets");
    goto fail;
  }
  free(text);
  text = NULL;
  module->export_count = BinaryenGetNumExports(source);
  module->exports = calloc(module->export_count, sizeof(*module->exports));
  if (module->export_count != 0 && module->exports == NULL)
    goto fail;
  for (index = 0; index < module->export_count; ++index) {
    BinaryenExportRef export_ref = BinaryenGetExportByIndex(source, index);
    BinaryenExternalKind export_kind = BinaryenExportGetKind(export_ref);
    module->exports[index].name = copy_string(BinaryenExportGetName(export_ref));
    module->exports[index].value = copy_string(BinaryenExportGetValue(export_ref));
    module->exports[index].is_function = export_kind == BinaryenExternalFunction();
    module->exports[index].is_global = export_kind == BinaryenExternalGlobal();
    module->exports[index].is_table = export_kind == BinaryenExternalTable();
    if (!module->exports[index].name || !module->exports[index].value)
      goto fail;
  }
  module->function_count = BinaryenGetNumFunctions(source);
  module->functions = calloc(module->function_count, sizeof(*module->functions));
  if (module->function_count != 0 && module->functions == NULL)
    goto fail;
  for (index = 0; index < module->function_count; ++index) {
    BinaryenFunctionRef function = BinaryenGetFunctionByIndex(source, index);
    WasmFunction *out = &module->functions[index];
    const char *import_module;
    DecodeContext context;
    out->index = index;
    out->name = copy_string(BinaryenFunctionGetName(function));
    if (out->name == NULL)
      goto fail;
    out->diagnostic_name =
        copy_string(has_descriptive_function_name(out->name) ? out->name : exported_function_name(module, out->name));
    context.function_index = out->index;
    context.function_name = out->diagnostic_name;
    context.module = module;
    if (!convert_tuple_type(BinaryenFunctionGetParams(function), &out->params, &out->param_count, diagnostics,
                            &context))
      goto fail;
    out->result = convert_type(BinaryenFunctionGetResults(function));
    if (out->result == WASM_VALUE_OTHER) {
      function_error(diagnostics, &context, "has an unsupported result type");
      goto fail;
    }
    import_module = BinaryenFunctionImportGetModule(function);
    out->is_import = import_module != NULL && import_module[0] != '\0';
    if (out->is_import) {
      out->import_module = copy_string(import_module);
      out->import_name = copy_string(BinaryenFunctionImportGetBase(function));
      if (!out->import_module || !out->import_name)
        goto fail;
      continue;
    }
    out->local_count = BinaryenFunctionGetNumVars(function);
    out->locals = calloc(out->local_count, sizeof(*out->locals));
    if (out->local_count != 0 && out->locals == NULL)
      goto fail;
    for (BinaryenIndex local = 0; local < out->local_count; ++local) {
      out->locals[local] = convert_type(BinaryenFunctionGetVar(function, local));
      if (out->locals[local] == WASM_VALUE_OTHER) {
        function_error(diagnostics, &context, "has an unsupported local type");
        goto fail;
      }
    }
    out->body = convert_expression(BinaryenFunctionGetBody(function), diagnostics, &context, "body");
    if (out->body == NULL)
      goto fail;
  }
  if (!read_data_segments(source, module, diagnostics))
    goto fail;
  BinaryenModuleDispose(source);
  return true;
fail:
  free(text);
  if (source)
    BinaryenModuleDispose(source);
  wasm_module_dispose(module);
  return false;
}

/* Releases all function, expression, export, and data-segment storage. */
void wasm_module_dispose(WasmModule *module) {
  size_t index;
  if (module->functions != NULL)
    for (index = 0; index < module->function_count; ++index) {
      WasmFunction *f = &module->functions[index];
      free(f->name);
      free(f->diagnostic_name);
      free(f->import_module);
      free(f->import_name);
      free(f->params);
      free(f->locals);
      free_expression(f->body);
    }
  if (module->exports != NULL)
    for (index = 0; index < module->export_count; ++index) {
      free(module->exports[index].name);
      free(module->exports[index].value);
    }
  if (module->data_segments != NULL)
    for (index = 0; index < module->data_segment_count; ++index)
      free(module->data_segments[index].bytes);
  free(module->functions);
  free(module->exports);
  free(module->stack_pointer_name);
  free(module->data_segments);
  memset(module, 0, sizeof(*module));
}

/* Searches compiler-owned functions by their internal Wasm name. */
const WasmFunction *wasm_module_find_function(const WasmModule *module, const char *name) {
  size_t index;
  for (index = 0; index < module->function_count; ++index)
    if (strcmp(module->functions[index].name, name) == 0)
      return &module->functions[index];
  return NULL;
}
