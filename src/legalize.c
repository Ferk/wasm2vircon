/* Compiler-owned legalization for raw Wasm linker artifacts.
 *
 * This deliberately small pass runs after Binaryen decoding and before
 * VirconWasm validation. It never exposes Binaryen objects and does not try to
 * reproduce a general Wasm optimizer. */

#include "legalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reports a legalization rejection with the same stable identity as decoding. */
static void legalization_error(Diagnostics *diagnostics, const WasmFunction *function, const WasmExpr *expression,
                               const char *reason) {
  if (function->diagnostic_name != NULL && function->diagnostic_name[0] != '\0')
    diagnostics_error(diagnostics, "Wasm %s in function %zu '%s' at expression path %s: %s", expression->opcode,
                      function->index, function->diagnostic_name, expression->path, reason);
  else
    diagnostics_error(diagnostics, "Wasm %s in function %zu at expression path %s: %s", expression->opcode,
                      function->index, expression->path, reason);
}

/* Finds the final expression executed on the direct fallthrough path. */
static WasmExpr *tail_expression(WasmExpr *expression) {
  while (expression != NULL && expression->kind == WASM_EXPR_BLOCK && expression->child_count != 0)
    expression = expression->children[expression->child_count - 1];
  return expression;
}

/* Proves that a typed loop cannot complete normally with a value.
 *
 * A direct, unconditional branch to the loop's own label closes every ordinary
 * fallthrough path. Earlier branches may return or leave another control
 * target, but they also cannot make this loop expression produce a value. */
static bool loop_has_direct_back_edge(const WasmExpr *loop) {
  WasmExpr *tail;
  if (loop->child_count != 1 || loop->name == NULL)
    return false;
  tail = tail_expression(loop->children[0]);
  return tail != NULL && tail->kind == WASM_EXPR_BR && tail->name != NULL && strcmp(tail->name, loop->name) == 0;
}

/* Walks an expression tree, recording global use and legalizing typed loops. */
static bool legalize_expression(WasmExpr *expression, const WasmFunction *function, bool *uses_global,
                                Diagnostics *diagnostics) {
  size_t index;
  if (expression->kind == WASM_EXPR_GLOBAL_GET || expression->kind == WASM_EXPR_GLOBAL_SET ||
      expression->kind == WASM_EXPR_STACK_POINTER_GET || expression->kind == WASM_EXPR_STACK_POINTER_SET)
    *uses_global = true;
  if (expression->kind == WASM_EXPR_LOOP && expression->value_type != WASM_VALUE_NONE &&
      (expression->value_type != WASM_VALUE_I32 || !loop_has_direct_back_edge(expression))) {
    legalization_error(diagnostics, function, expression,
                       "value-producing loops are unsupported unless they "
                       "have a direct unconditional back-edge");
    return false;
  }
  for (index = 0; index < expression->child_count; ++index)
    if (!legalize_expression(expression->children[index], function, uses_global, diagnostics))
      return false;
  if (expression->kind == WASM_EXPR_LOOP && expression->value_type == WASM_VALUE_I32 &&
      loop_has_direct_back_edge(expression))
    expression->value_type = WASM_VALUE_NONE;
  return true;
}

/* Returns whether a module export keeps a global or table externally visible. */
static bool has_exported_kind(const WasmModule *module, bool global) {
  size_t index;
  for (index = 0; index < module->export_count; ++index)
    if ((global && module->exports[index].is_global) || (!global && module->exports[index].is_table))
      return true;
  return false;
}

/* Legalizes raw linker artifacts without expanding the VirconWasm feature set. */
bool wasm_module_legalize_linker_artifacts(WasmModule *module, Diagnostics *diagnostics) {
  bool uses_global = false;
  size_t index;

  for (index = 0; index < module->function_count; ++index) {
    WasmFunction *function = &module->functions[index];
    if (!function->is_import && !legalize_expression(function->body, function, &uses_global, diagnostics))
      return false;
  }

  if (!uses_global && !has_exported_kind(module, true)) {
    module->global_count = 0;
    module->has_stack_pointer_global = false;
    module->stack_pointer_global_is_valid = false;
    module->stack_pointer_initial = 0;
    free(module->stack_pointer_name);
    module->stack_pointer_name = NULL;
  }

  /* Decode rejects every table operation and indirect call before this point.
   * An unexported table/its elements are therefore inert linker scaffolding. */
  if (!has_exported_kind(module, false)) {
    module->table_count = 0;
    module->element_segment_count = 0;
  }
  return true;
}
