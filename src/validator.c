#include "validator.h"

#include <string.h>

typedef struct ImportSpec {
    const char *module;
    const char *name;
    WasmValueType params[1];
    size_t param_count;
    WasmValueType result;
} ImportSpec;

static const ImportSpec IMPORTS[] = {
    {"env", "vircon_set_background_color", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_end_frame", {WASM_VALUE_NONE}, 0, WASM_VALUE_NONE},
};

static const ImportSpec *find_import_spec(const char *module, const char *name)
{
    size_t index;

    for (index = 0; index < sizeof(IMPORTS) / sizeof(IMPORTS[0]); index++) {
        if (strcmp(module, IMPORTS[index].module) == 0 &&
            strcmp(name, IMPORTS[index].name) == 0) {
            return &IMPORTS[index];
        }
    }
    return NULL;
}

static bool matches_signature(const WasmFunction *function, const ImportSpec *spec)
{
    size_t index;

    if (function->param_count != spec->param_count || function->result != spec->result) {
        return false;
    }
    for (index = 0; index < spec->param_count; index++) {
        if (function->params[index] != spec->params[index]) {
            return false;
        }
    }
    return true;
}

static bool validate_expression(const WasmExpr *expression, const WasmModule *module,
                                Diagnostics *diagnostics)
{
    const WasmFunction *callee;
    const ImportSpec *spec;
    size_t index;

    switch (expression->kind) {
    case WASM_EXPR_BLOCK:
    case WASM_EXPR_LOOP:
        for (index = 0; index < expression->child_count; index++) {
            if (!validate_expression(expression->children[index], module, diagnostics)) {
                return false;
            }
        }
        return true;
    case WASM_EXPR_BR:
    case WASM_EXPR_UNREACHABLE:
    case WASM_EXPR_I32_CONST:
        return true;
    case WASM_EXPR_CALL:
        callee = wasm_module_find_function(module, expression->name);
        if (callee == NULL) {
            diagnostics_error(diagnostics, "call target '%s' does not name a module function",
                              expression->name);
            return false;
        }
        if (!callee->is_import) {
            diagnostics_error(diagnostics,
                              "call to defined function '%s' is not supported in VirconWasm v0",
                              expression->name);
            return false;
        }
        spec = find_import_spec(callee->import_module, callee->import_name);
        if (spec == NULL) {
            diagnostics_error(diagnostics, "call uses unsupported import '%s.%s'",
                              callee->import_module, callee->import_name);
            return false;
        }
        if (expression->child_count != spec->param_count) {
            diagnostics_error(diagnostics, "call to '%s.%s' has %zu operands; expected %zu",
                              spec->module, spec->name, expression->child_count,
                              spec->param_count);
            return false;
        }
        for (index = 0; index < expression->child_count; index++) {
            if (!validate_expression(expression->children[index], module, diagnostics)) {
                return false;
            }
            if (spec->params[index] == WASM_VALUE_I32 &&
                expression->children[index]->kind != WASM_EXPR_I32_CONST) {
                diagnostics_error(diagnostics,
                                  "VirconWasm v0 currently accepts only i32.const as the argument to '%s.%s'",
                                  spec->module, spec->name);
                return false;
            }
        }
        return true;
    }
    diagnostics_error(diagnostics, "internal error: unknown compiler-owned Wasm expression");
    return false;
}

bool validate_virconwasm_v0(const WasmModule *module, const char *entry_name,
                            ValidatedModule *validated, Diagnostics *diagnostics)
{
    const WasmExport *entry_export = NULL;
    const WasmFunction *entry;
    size_t index;

    memset(validated, 0, sizeof(*validated));
    if (!module->has_memory) {
        diagnostics_error(diagnostics, "VirconWasm v0 requires one declared Wasm memory");
        return false;
    }
    if (module->has_imported_memory) {
        diagnostics_error(diagnostics,
                          "memory imports are unsupported: VirconWasm v0 imports must be platform functions");
        return false;
    }
    if (module->table_count != 0 || module->global_count != 0 ||
        module->element_segment_count != 0 || module->data_segment_count != 0) {
        diagnostics_error(diagnostics,
                          "tables, globals, element segments, and data segments are unsupported in VirconWasm v0");
        return false;
    }

    for (index = 0; index < module->function_count; index++) {
        const WasmFunction *function = &module->functions[index];
        const ImportSpec *spec;

        if (!function->is_import) {
            continue;
        }
        spec = find_import_spec(function->import_module, function->import_name);
        if (spec == NULL) {
            diagnostics_error(diagnostics, "unsupported function import '%s.%s'",
                              function->import_module, function->import_name);
            return false;
        }
        if (!matches_signature(function, spec)) {
            diagnostics_error(diagnostics, "import '%s.%s' has a signature unsupported by VirconWasm v0",
                              function->import_module, function->import_name);
            return false;
        }
    }

    for (index = 0; index < module->export_count; index++) {
        if (strcmp(module->exports[index].name, entry_name) == 0) {
            entry_export = &module->exports[index];
            break;
        }
    }
    if (entry_export == NULL) {
        diagnostics_error(diagnostics, "entry export '%s' does not exist", entry_name);
        return false;
    }
    if (!entry_export->is_function) {
        diagnostics_error(diagnostics, "entry export '%s' is not a function", entry_name);
        return false;
    }
    entry = wasm_module_find_function(module, entry_export->value);
    if (entry == NULL || entry->is_import) {
        diagnostics_error(diagnostics, "entry export '%s' does not refer to a defined function",
                          entry_name);
        return false;
    }
    if (entry->param_count != 0 || entry->result != WASM_VALUE_I32) {
        diagnostics_error(diagnostics,
                          "entry export '%s' must have signature () -> i32 for this frontend profile",
                          entry_name);
        return false;
    }
    if (!validate_expression(entry->body, module, diagnostics)) {
        return false;
    }

    validated->module = module;
    validated->entry = entry;
    return true;
}
