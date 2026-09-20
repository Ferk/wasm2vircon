#include "validator.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "target_layout.h"

typedef struct ImportSpec {
    const char *module, *name;
    WasmValueType params[4];
    size_t param_count;
    WasmValueType result;
} ImportSpec;

static const ImportSpec IMPORTS[] = {
    {"env", "vircon_set_background_color", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_end_frame", {WASM_VALUE_NONE}, 0, WASM_VALUE_NONE},
    {"env", "vircon_gpu_get_selected_texture", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_gpu_select_texture", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_gpu_select_region", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_drawing_point", {WASM_VALUE_I32, WASM_VALUE_I32}, 2, WASM_VALUE_NONE},
    {"env", "vircon_gpu_draw_region", {WASM_VALUE_NONE}, 0, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_region_minimum", {WASM_VALUE_I32, WASM_VALUE_I32}, 2, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_region_maximum", {WASM_VALUE_I32, WASM_VALUE_I32}, 2, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_region_hotspot", {WASM_VALUE_I32, WASM_VALUE_I32}, 2, WASM_VALUE_NONE},
    {"env", "vircon_spu_select_channel", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_spu_set_channel_assigned_sound", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_spu_play_selected_channel", {WASM_VALUE_NONE}, 0, WASM_VALUE_NONE},
    {"env", "vircon_spu_set_channel_volume", {WASM_VALUE_F32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_multiply_color", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_drawing_scale_bits", {WASM_VALUE_I32, WASM_VALUE_I32}, 2, WASM_VALUE_NONE},
    {"env", "vircon_gpu_set_drawing_scale", {WASM_VALUE_F32, WASM_VALUE_F32}, 2, WASM_VALUE_NONE},
    {"env", "vircon_gpu_draw_region_zoomed", {WASM_VALUE_NONE}, 0, WASM_VALUE_NONE},
    {"env", "vircon_cpu_sin", {WASM_VALUE_F32}, 1, WASM_VALUE_F32},
    {"env", "vircon_cpu_acos", {WASM_VALUE_F32}, 1, WASM_VALUE_F32},
    {"env", "vircon_cpu_log", {WASM_VALUE_F32}, 1, WASM_VALUE_F32},
    {"env", "vircon_cpu_pow", {WASM_VALUE_F32, WASM_VALUE_F32}, 2, WASM_VALUE_F32},
    {"env", "vircon_input_select_gamepad", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_input_gamepad_left", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_right", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_up", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_down", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_connected", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_a", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_b", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_x", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_y", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_l", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_r", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_input_gamepad_button_start", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_timer_get_frame_counter", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_timer_get_current_time", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_timer_get_current_date", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_rng_get_current_value", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_rng_set_current_value", {WASM_VALUE_I32}, 1, WASM_VALUE_NONE},
    {"env", "vircon_spu_get_channel_state", {WASM_VALUE_NONE}, 0, WASM_VALUE_I32},
    {"env", "vircon_spu_set_channel_speed", {WASM_VALUE_F32}, 1, WASM_VALUE_NONE},
};

static const ImportSpec *find_import_spec(const char *module, const char *name)
{
    size_t index;
    for (index = 0; index < sizeof(IMPORTS) / sizeof(IMPORTS[0]); ++index)
        if (strcmp(module, IMPORTS[index].module) == 0 && strcmp(name, IMPORTS[index].name) == 0)
            return &IMPORTS[index];
    return NULL;
}

static bool matches_signature(const WasmFunction *function, const ImportSpec *spec)
{
    size_t index;
    if (function->param_count != spec->param_count || function->result != spec->result) return false;
    for (index = 0; index < spec->param_count; ++index)
        if (function->params[index] != spec->params[index]) return false;
    return true;
}

static bool function_has_i32_signature(const WasmFunction *function)
{
    size_t index;
    if (function->result != WASM_VALUE_NONE && function->result != WASM_VALUE_I32 && function->result != WASM_VALUE_F32) return false;
    for (index = 0; index < function->param_count; ++index)
        if (function->params[index] != WASM_VALUE_I32 && function->params[index] != WASM_VALUE_F32) return false;
    for (index = 0; index < function->local_count; ++index)
        if (function->locals[index] != WASM_VALUE_I32 && function->locals[index] != WASM_VALUE_F32) return false;
    return true;
}

static size_t function_index(const WasmModule *module, const WasmFunction *function)
{
    return (size_t)(function - module->functions);
}

static bool validate_function(const WasmModule *module, const WasmFunction *function,
                              bool *reachable, Diagnostics *diagnostics);

static bool validate_expression(const WasmModule *module, const WasmFunction *function,
                                const WasmExpr *expression, bool *reachable,
                                Diagnostics *diagnostics)
{
    const WasmFunction *callee; const ImportSpec *spec; size_t index;
    switch (expression->kind) {
    case WASM_EXPR_BLOCK:
    case WASM_EXPR_LOOP:
        for (index = 0; index < expression->child_count; ++index)
            if (!validate_expression(module, function, expression->children[index], reachable, diagnostics)) return false;
        return true;
    case WASM_EXPR_BR:
    case WASM_EXPR_UNREACHABLE:
    case WASM_EXPR_I32_CONST:
    case WASM_EXPR_F32_CONST:
        return true;
    case WASM_EXPR_BR_IF:
        return expression->child_count == 1 &&
               validate_expression(module, function, expression->children[0], reachable, diagnostics);
    case WASM_EXPR_IF:
        if (expression->child_count != 2) { diagnostics_error(diagnostics, "function '%s' uses an if with else, which VirconWasm v1 does not support", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics) &&
               validate_expression(module, function, expression->children[1], reachable, diagnostics);
    case WASM_EXPR_LOCAL_GET:
        if (expression->index >= function->param_count + function->local_count) { diagnostics_error(diagnostics, "function '%s' gets an invalid local index", function->name); return false; }
        return true;
    case WASM_EXPR_LOCAL_SET:
        if (expression->index >= function->param_count + function->local_count) { diagnostics_error(diagnostics, "function '%s' sets an invalid local index", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics);
    case WASM_EXPR_DROP:
        return expression->child_count == 1 && validate_expression(module, function, expression->children[0], reachable, diagnostics);
    case WASM_EXPR_LOAD:
        if (!((expression->bytes == 1 && !expression->is_signed) || expression->bytes == 4)) { diagnostics_error(diagnostics, "function '%s' uses unsupported load width/sign", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics);
    case WASM_EXPR_STORE:
        if (expression->bytes != 1 && expression->bytes != 4) { diagnostics_error(diagnostics, "function '%s' uses unsupported store width", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics) &&
               validate_expression(module, function, expression->children[1], reachable, diagnostics);
    case WASM_EXPR_BINARY:
        if (expression->binary_op == WASM_BINARY_OTHER) { diagnostics_error(diagnostics, "function '%s' uses an unsupported i32 binary operation", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics) &&
               validate_expression(module, function, expression->children[1], reachable, diagnostics);
    case WASM_EXPR_UNARY:
        if (expression->unary_op == WASM_UNARY_OTHER) { diagnostics_error(diagnostics, "function '%s' uses an unsupported unary operation", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics);
    case WASM_EXPR_SELECT:
        if (expression->child_count != 3) { diagnostics_error(diagnostics, "function '%s' has a malformed select", function->name); return false; }
        return validate_expression(module, function, expression->children[0], reachable, diagnostics) &&
               validate_expression(module, function, expression->children[1], reachable, diagnostics) &&
               validate_expression(module, function, expression->children[2], reachable, diagnostics);
    case WASM_EXPR_RETURN:
        if ((function->result == WASM_VALUE_NONE && expression->child_count != 0) ||
            ((function->result == WASM_VALUE_I32 || function->result == WASM_VALUE_F32) && expression->child_count != 1)) { diagnostics_error(diagnostics, "function '%s' has an incompatible return", function->name); return false; }
        return expression->child_count == 0 || validate_expression(module, function, expression->children[0], reachable, diagnostics);
    case WASM_EXPR_CALL:
        callee = wasm_module_find_function(module, expression->name);
        if (callee == NULL) { diagnostics_error(diagnostics, "call target '%s' does not name a module function", expression->name); return false; }
        if (expression->child_count != callee->param_count) { diagnostics_error(diagnostics, "call to '%s' has %zu operands; expected %zu", expression->name, expression->child_count, callee->param_count); return false; }
        for (index = 0; index < expression->child_count; ++index)
            if (!validate_expression(module, function, expression->children[index], reachable, diagnostics)) return false;
        if (callee->is_import) {
            spec = find_import_spec(callee->import_module, callee->import_name);
            if (spec == NULL) { diagnostics_error(diagnostics, "call uses unsupported import '%s.%s'", callee->import_module, callee->import_name); return false; }
            if (!matches_signature(callee, spec)) { diagnostics_error(diagnostics, "import '%s.%s' has an unsupported signature", callee->import_module, callee->import_name); return false; }
            return true;
        }
        return validate_function(module, callee, reachable, diagnostics);
    }
    diagnostics_error(diagnostics, "internal error: unknown compiler-owned Wasm expression"); return false;
}

static bool validate_function(const WasmModule *module, const WasmFunction *function,
                              bool *reachable, Diagnostics *diagnostics)
{
    size_t index = function_index(module, function);
    if (reachable[index]) return true;
    reachable[index] = true;
    if (!function_has_i32_signature(function)) { diagnostics_error(diagnostics, "reachable function '%s' has unsupported VirconWasm v1 value types", function->name); return false; }
    return validate_expression(module, function, function->body, reachable, diagnostics);
}

bool validate_virconwasm_v1(const WasmModule *module, const char *entry_name,
                            ValidatedModule *validated, Diagnostics *diagnostics)
{
    const WasmExport *entry_export = NULL; const WasmFunction *entry; size_t index; uint64_t memory_bytes, available_bytes;
    memset(validated, 0, sizeof(*validated));
    if (module->memory_count != 1 || !module->has_memory) { diagnostics_error(diagnostics, "VirconWasm v1 requires exactly one defined linear memory"); return false; }
    if (module->has_imported_memory || module->memory_is_shared || module->memory_is_64) { diagnostics_error(diagnostics, "memory imports, shared memory, and memory64 are unsupported"); return false; }
    if (module->table_count != 0 || module->global_count != 0 || module->element_segment_count != 0) { diagnostics_error(diagnostics, "tables, globals, and element segments are unsupported in VirconWasm v1"); return false; }
    memory_bytes = (uint64_t)module->memory_initial_pages * 65536u;
    available_bytes = VIRCON_LINEAR_MEMORY_BYTES;
    if (memory_bytes == 0 || memory_bytes > available_bytes || memory_bytes > UINT32_MAX) { diagnostics_error(diagnostics, "declared Wasm memory does not fit the reserved Vircon32 linear-memory region"); return false; }
    for (index = 0; index < module->data_segment_count; ++index) { const WasmDataSegment *segment = &module->data_segments[index]; if (segment->is_passive || !segment->offset_is_i32_const || (uint64_t)segment->offset + segment->size > memory_bytes) { diagnostics_error(diagnostics, "data segment %zu is not an in-bounds active constant-offset segment", index); return false; } }
    for (index = 0; index < module->function_count; ++index) { const WasmFunction *function = &module->functions[index]; const ImportSpec *spec; if (!function->is_import) continue; spec = find_import_spec(function->import_module, function->import_name); if (spec == NULL) { diagnostics_error(diagnostics, "unsupported function import '%s.%s'", function->import_module, function->import_name); return false; } if (!matches_signature(function, spec)) { diagnostics_error(diagnostics, "import '%s.%s' has an unsupported signature", function->import_module, function->import_name); return false; } }
    for (index = 0; index < module->export_count; ++index) if (strcmp(module->exports[index].name, entry_name) == 0) { entry_export = &module->exports[index]; break; }
    if (entry_export == NULL || !entry_export->is_function) { diagnostics_error(diagnostics, "entry export '%s' does not name a function", entry_name); return false; }
    entry = wasm_module_find_function(module, entry_export->value);
    if (entry == NULL || entry->is_import || entry->param_count != 0 || entry->result != WASM_VALUE_I32) { diagnostics_error(diagnostics, "entry export '%s' must refer to a defined () -> i32 function", entry_name); return false; }
    validated->reachable = calloc(module->function_count, sizeof(*validated->reachable));
    if (validated->reachable == NULL) { diagnostics_error(diagnostics, "out of memory tracking reachable functions"); return false; }
    if (!validate_function(module, entry, validated->reachable, diagnostics)) { validated_module_dispose(validated); return false; }
    validated->module = module; validated->entry = entry; return true;
}

void validated_module_dispose(ValidatedModule *validated)
{
    free(validated->reachable); memset(validated, 0, sizeof(*validated));
}
