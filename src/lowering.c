#include "lowering.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target_layout.h"

#define LINEAR_BASE VIRCON_LINEAR_MEMORY_BASE
#define TEMP_SLOTS 48u
#define OUTGOING_SLOTS 4u

typedef struct Value { int slot; bool present; } Value;
typedef struct Target { const char *wasm_name; char *label; } Target;
typedef struct Context {
    const ValidatedModule *validated; const WasmFunction *function;
    VirconIrProgram *program; Diagnostics *diagnostics;
    Target targets[32]; size_t target_count; unsigned next_label, temp_depth;
    char return_label[64]; uint32_t memory_bytes;
} Context;

static char *format_text(const char *format, ...)
{
    va_list args, copy; int length; char *text;
    va_start(args, format); va_copy(copy, args); length = vsnprintf(NULL, 0, format, copy); va_end(copy);
    if (length < 0) { va_end(args); return NULL; }
    text = malloc((size_t)length + 1); if (text != NULL) vsnprintf(text, (size_t)length + 1, format, args); va_end(args); return text;
}
static bool emit(Context *context, const char *format, ...)
{
    va_list args; char buffer[256]; int length; char *text;
    va_start(args, format); length = vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    if (length < 0) return false;
    text = length < (int)sizeof(buffer) ? format_text("%s", buffer) : NULL;
    if (text == NULL) { diagnostics_error(context->diagnostics, "out of memory building V32 IR"); return false; }
    return vircon_ir_append_text(context->program, text, context->diagnostics);
}
static bool fresh_label(Context *context, const char *kind, char *out, size_t size)
{ return snprintf(out, size, "__wasm_%s_%zu_%u", kind, (size_t)(context->function - context->validated->module->functions), context->next_label++) > 0; }
static bool emit_label(Context *context, const char *label) { return emit(context, "%s:", label); }
static int temp_slot(Context *context)
{
    if (context->temp_depth == TEMP_SLOTS) { diagnostics_error(context->diagnostics, "expression nesting exceeds VirconWasm v1 temporary-slot limit"); return 0; }
    ++context->temp_depth; return -(int)(context->function->local_count + context->temp_depth);
}
static void release(Context *context, Value value) { if (value.present && context->temp_depth != 0) --context->temp_depth; }
static bool load_slot(Context *context, int reg, int slot) { return emit(context, "  mov R%d, [BP%+d]", reg, slot); }
static bool store_slot(Context *context, int slot, int reg) { return emit(context, "  mov [BP%+d], R%d", slot, reg); }
static int local_slot(const WasmFunction *function, uint32_t index)
{ return index < function->param_count ? (int)(2 + index) : -(int)(index - function->param_count + 1); }
static size_t function_index(const WasmModule *module, const WasmFunction *function) { return (size_t)(function - module->functions); }
static void function_label(const WasmModule *module, const WasmFunction *function, char *out, size_t size)
{ snprintf(out, size, "__wasm_function_%zu", function_index(module, function)); }

static bool lower_expression(Context *context, const WasmExpr *expression, Value *value);

static bool effective_address(Context *context, Value pointer, uint32_t offset, uint32_t width)
{
    uint32_t maximum;
    if ((uint64_t)offset + width > context->memory_bytes) return emit(context, "  jmp __wasm_trap");
    maximum = context->memory_bytes - offset - width;
    return load_slot(context, 2, pointer.slot) && emit(context, "  mov R1, R2") && emit(context, "  ilt R1, 0") && emit(context, "  jt R1, __wasm_trap") && emit(context, "  mov R1, R2") && emit(context, "  igt R1, 0x%08X", maximum) && emit(context, "  jt R1, __wasm_trap") && (offset == 0 || emit(context, "  iadd R2, 0x%08X", offset));
}
static bool load_byte_at_r2(Context *context, int result)
{
    return emit(context, "  mov R3, R2") && emit(context, "  and R3, 3") && emit(context, "  imul R3, -8") && emit(context, "  mov R4, R2") && emit(context, "  mov R5, -2") && emit(context, "  shl R4, R5") && emit(context, "  iadd R4, %u", LINEAR_BASE) && emit(context, "  mov R%d, [R4]", result) && emit(context, "  shl R%d, R3", result) && emit(context, "  and R%d, 0x000000FF", result);
}
static bool store_byte_at_r2(Context *context, int value_register)
{
    return emit(context, "  mov R3, R2") && emit(context, "  and R3, 3") && emit(context, "  imul R3, 8") && emit(context, "  mov R4, R2") && emit(context, "  mov R5, -2") && emit(context, "  shl R4, R5") && emit(context, "  iadd R4, %u", LINEAR_BASE) && emit(context, "  mov R5, [R4]") && emit(context, "  mov R6, 0x000000FF") && emit(context, "  shl R6, R3") && emit(context, "  bnot R6") && emit(context, "  and R5, R6") && emit(context, "  mov R6, R%d", value_register) && emit(context, "  and R6, 0x000000FF") && emit(context, "  shl R6, R3") && emit(context, "  or R5, R6") && emit(context, "  mov [R4], R5");
}
static bool lower_load(Context *context, const WasmExpr *expression, Value *value)
{
    Value pointer = {0}; int slot; char aligned[64], done[64];
    if (!lower_expression(context, expression->children[0], &pointer) || !pointer.present || !effective_address(context, pointer, expression->offset, expression->bytes)) return false;
    if (expression->bytes == 1) {
        if (!load_byte_at_r2(context, 1) || !store_slot(context, pointer.slot, 1)) return false;
        *value = pointer; return true;
    }
    if (!fresh_label(context, "load_aligned", aligned, sizeof(aligned)) || !fresh_label(context, "load_done", done, sizeof(done))) return false;
    if (!emit(context, "  mov R1, R2") || !emit(context, "  and R1, 3") || !emit(context, "  jf R1, %s", aligned)) return false;
    /* The unaligned path reconstructs the value from four byte lanes. */
    if (!emit(context, "  mov R6, 0")) return false;
    for (unsigned byte = 0; byte < 4; ++byte) {
        if (!load_byte_at_r2(context, 5) || (byte != 0 && !emit(context, "  mov R3, %u", byte * 8)) || (byte != 0 && !emit(context, "  shl R5, R3")) || !emit(context, "  or R6, R5") || (byte != 3 && !emit(context, "  iadd R2, 1"))) return false;
    }
    if (!emit(context, "  mov R1, R6") || !emit(context, "  jmp %s", done) || !emit_label(context, aligned) || !emit(context, "  mov R3, R2") || !emit(context, "  mov R4, -2") || !emit(context, "  shl R3, R4") || !emit(context, "  iadd R3, %u", LINEAR_BASE) || !emit(context, "  mov R1, [R3]") || !emit_label(context, done)) return false;
    slot = pointer.slot; if (!store_slot(context, slot, 1)) return false; *value = pointer; return true;
}
static bool lower_store(Context *context, const WasmExpr *expression, Value *value)
{
    Value pointer = {0}, input = {0}; char aligned[64], done[64];
    if (!lower_expression(context, expression->children[0], &pointer) || !lower_expression(context, expression->children[1], &input) || !pointer.present || !input.present || !effective_address(context, pointer, expression->offset, expression->bytes) || !load_slot(context, 1, input.slot)) return false;
    if (expression->bytes == 1) { if (!store_byte_at_r2(context, 1)) return false; release(context, input); release(context, pointer); value->present = false; return true; }
    if (!fresh_label(context, "store_aligned", aligned, sizeof(aligned)) || !fresh_label(context, "store_done", done, sizeof(done))) return false;
    if (!emit(context, "  mov R7, R2") || !emit(context, "  and R7, 3") || !emit(context, "  jf R7, %s", aligned)) return false;
    for (unsigned byte = 0; byte < 4; ++byte) {
        if (!emit(context, "  mov R6, R1") || (byte != 0 && !emit(context, "  mov R3, -%u", byte * 8)) || (byte != 0 && !emit(context, "  shl R6, R3")) || !store_byte_at_r2(context, 6) || (byte != 3 && !emit(context, "  iadd R2, 1"))) return false;
    }
    if (!emit(context, "  jmp %s", done) || !emit_label(context, aligned) || !emit(context, "  mov R3, R2") || !emit(context, "  mov R4, -2") || !emit(context, "  shl R3, R4") || !emit(context, "  iadd R3, %u", LINEAR_BASE) || !emit(context, "  mov [R3], R1") || !emit_label(context, done)) return false;
    release(context, input); release(context, pointer); value->present = false; return true;
}
static bool lower_call(Context *context, const WasmExpr *expression, Value *value)
{
    const WasmFunction *callee = wasm_module_find_function(context->validated->module, expression->name); Value arguments[4] = {{0}}; char label[64]; size_t index;
    for (index = 0; index < expression->child_count; ++index) if (!lower_expression(context, expression->children[index], &arguments[index]) || !arguments[index].present) return false;
    if (callee->is_import) {
        if (strcmp(callee->import_name, "vircon_set_background_color") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out GPU_ClearColor, R1") || !emit(context, "  out GPU_Command, GPUCommand_ClearScreen")) return false; }
        else if (strcmp(callee->import_name, "vircon_end_frame") == 0) { if (!emit(context, "  wait")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_get_selected_texture") == 0) {
            int slot = temp_slot(context);
            if (slot == 0 || !emit(context, "  in R0, GPU_SelectedTexture") || !store_slot(context, slot, 0)) return false;
            value->slot = slot; value->present = true; return true;
        }
        else if (strcmp(callee->import_name, "vircon_gpu_select_texture") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out GPU_SelectedTexture, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_select_region") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out GPU_SelectedRegion, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_point") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !load_slot(context, 2, arguments[1].slot) || !emit(context, "  out GPU_DrawingPointX, R1") || !emit(context, "  out GPU_DrawingPointY, R2")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_draw_region") == 0) { if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegion")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_region_minimum") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !load_slot(context, 2, arguments[1].slot) || !emit(context, "  out GPU_RegionMinX, R1") || !emit(context, "  out GPU_RegionMinY, R2")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_region_maximum") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !load_slot(context, 2, arguments[1].slot) || !emit(context, "  out GPU_RegionMaxX, R1") || !emit(context, "  out GPU_RegionMaxY, R2")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_region_hotspot") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !load_slot(context, 2, arguments[1].slot) || !emit(context, "  out GPU_RegionHotSpotX, R1") || !emit(context, "  out GPU_RegionHotSpotY, R2")) return false; }
        else if (strcmp(callee->import_name, "vircon_spu_select_channel") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out SPU_SelectedChannel, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_spu_set_channel_assigned_sound") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out SPU_ChannelAssignedSound, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_spu_play_selected_channel") == 0) { if (!emit(context, "  out SPU_Command, SPUCommand_PlaySelectedChannel")) return false; }
        else if (strcmp(callee->import_name, "vircon_spu_set_channel_volume") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out SPU_ChannelVolume, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_rng_set_current_value") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out RNG_CurrentValue, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_multiply_color") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out GPU_MultiplyColor, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_scale_bits") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !load_slot(context, 2, arguments[1].slot) || !emit(context, "  out GPU_DrawingScaleX, R1") || !emit(context, "  out GPU_DrawingScaleY, R2")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_scale") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !load_slot(context, 2, arguments[1].slot) || !emit(context, "  out GPU_DrawingScaleX, R1") || !emit(context, "  out GPU_DrawingScaleY, R2")) return false; }
        else if (strcmp(callee->import_name, "vircon_gpu_draw_region_zoomed") == 0) { if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionZoomed")) return false; }
        else if (strcmp(callee->import_name, "vircon_input_select_gamepad") == 0) { if (!load_slot(context, 1, arguments[0].slot) || !emit(context, "  out INP_SelectedGamepad, R1")) return false; }
        else if (strcmp(callee->import_name, "vircon_input_gamepad_left") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_right") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_up") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_down") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_connected") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_a") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_b") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_x") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_y") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_l") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_r") == 0 || strcmp(callee->import_name, "vircon_input_gamepad_button_start") == 0 || strcmp(callee->import_name, "vircon_timer_get_frame_counter") == 0 || strcmp(callee->import_name, "vircon_timer_get_current_time") == 0 || strcmp(callee->import_name, "vircon_rng_get_current_value") == 0 || strcmp(callee->import_name, "vircon_spu_get_channel_state") == 0) {
            const char *port = strcmp(callee->import_name, "vircon_input_gamepad_left") == 0 ? "INP_GamepadLeft" :
                strcmp(callee->import_name, "vircon_input_gamepad_right") == 0 ? "INP_GamepadRight" :
                strcmp(callee->import_name, "vircon_input_gamepad_up") == 0 ? "INP_GamepadUp" :
                strcmp(callee->import_name, "vircon_input_gamepad_down") == 0 ? "INP_GamepadDown" :
                strcmp(callee->import_name, "vircon_input_gamepad_connected") == 0 ? "INP_GamepadConnected" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_a") == 0 ? "INP_GamepadButtonA" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_b") == 0 ? "INP_GamepadButtonB" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_x") == 0 ? "INP_GamepadButtonX" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_y") == 0 ? "INP_GamepadButtonY" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_l") == 0 ? "INP_GamepadButtonL" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_r") == 0 ? "INP_GamepadButtonR" :
                strcmp(callee->import_name, "vircon_input_gamepad_button_start") == 0 ? "INP_GamepadButtonStart" :
                strcmp(callee->import_name, "vircon_timer_get_current_time") == 0 ? "TIM_CurrentTime" :
                strcmp(callee->import_name, "vircon_rng_get_current_value") == 0 ? "RNG_CurrentValue" :
                strcmp(callee->import_name, "vircon_spu_get_channel_state") == 0 ? "SPU_ChannelState" : "TIM_FrameCounter";
            int slot = temp_slot(context);
            if (slot == 0 || !emit(context, "  in R0, %s", port) || !store_slot(context, slot, 0)) return false;
            value->slot = slot; value->present = true; return true;
        }
        else { diagnostics_error(context->diagnostics, "internal error: unsupported import passed validation"); return false; }
        for (index = expression->child_count; index != 0; --index)
            release(context, arguments[index - 1]);
        value->present = false;
        return true;
    }
    for (index = 0; index < expression->child_count; ++index) if (!load_slot(context, 1, arguments[index].slot) || !emit(context, "  mov [SP+%zu], R1", index)) return false;
    for (index = expression->child_count; index != 0; --index) release(context, arguments[index - 1]);
    function_label(context->validated->module, callee, label, sizeof(label)); if (!emit(context, "  call %s", label)) return false;
    if (callee->result == WASM_VALUE_I32) { int slot = temp_slot(context); if (slot == 0 || !store_slot(context, slot, 0)) return false; value->slot = slot; value->present = true; }
    else value->present = false;
    return true;
}

static bool lower_binary(Context *context, const WasmExpr *expression, Value *value)
{
    Value left = {0}, right = {0}; char normal[64], done[64];
    if (!lower_expression(context, expression->children[0], &left) ||
        !lower_expression(context, expression->children[1], &right) ||
        !left.present || !right.present || !load_slot(context, 1, left.slot) ||
        !load_slot(context, 2, right.slot)) return false;

    switch (expression->binary_op) {
    case WASM_BINARY_ADD: if (!emit(context, "  iadd R1, R2")) return false; break;
    case WASM_BINARY_SUB: if (!emit(context, "  isub R1, R2")) return false; break;
    case WASM_BINARY_MUL: if (!emit(context, "  imul R1, R2")) return false; break;
    case WASM_BINARY_AND: if (!emit(context, "  and R1, R2")) return false; break;
    case WASM_BINARY_EQ: if (!emit(context, "  ieq R1, R2")) return false; break;
    case WASM_BINARY_NE: if (!emit(context, "  ine R1, R2")) return false; break;
    case WASM_BINARY_LT_S: if (!emit(context, "  ilt R1, R2")) return false; break;
    case WASM_BINARY_GT_S: if (!emit(context, "  igt R1, R2")) return false; break;
    case WASM_BINARY_GE_S: if (!emit(context, "  ige R1, R2")) return false; break;
    case WASM_BINARY_F32_MUL: if (!emit(context, "  fmul R1, R2")) return false; break;
    case WASM_BINARY_LT_U:
        if (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") || !emit(context, "  ilt R1, R2")) return false;
        break;
    case WASM_BINARY_GT_U:
        if (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") || !emit(context, "  igt R1, R2")) return false;
        break;
    case WASM_BINARY_SHL:
        if (!emit(context, "  and R2, 31") || !emit(context, "  shl R1, R2")) return false;
        break;
    case WASM_BINARY_DIV_U:
        if (!emit(context, "  call __wasm_i32_div_u") || !store_slot(context, left.slot, 0)) return false;
        release(context, right); *value = left; return true;
    case WASM_BINARY_REM_S:
        if (!fresh_label(context, "rem_s_normal", normal, sizeof(normal)) ||
            !fresh_label(context, "rem_s_done", done, sizeof(done)) ||
            !emit(context, "  mov R3, R2") || !emit(context, "  ieq R3, 0") ||
            !emit(context, "  jt R3, __wasm_trap") || !emit(context, "  ieq R3, -1") ||
            !emit(context, "  jf R3, %s", normal) || !emit(context, "  mov R1, 0") ||
            !emit(context, "  jmp %s", done) || !emit_label(context, normal) ||
            !emit(context, "  imod R1, R2") || !emit_label(context, done)) return false;
        break;
    case WASM_BINARY_OTHER:
        diagnostics_error(context->diagnostics, "internal error: unsupported binary operation passed validation"); return false;
    }
    if (!store_slot(context, left.slot, 1)) return false;
    release(context, right); *value = left; return true;
}

static bool expression_uses_binary(const WasmExpr *expression, WasmBinaryOp operation)
{
    size_t index;
    if (expression->kind == WASM_EXPR_BINARY && expression->binary_op == operation) return true;
    for (index = 0; index < expression->child_count; ++index)
        if (expression_uses_binary(expression->children[index], operation)) return true;
    return false;
}

static bool emit_unsigned_division_helper(Context *context)
{
    return emit_label(context, "__wasm_i32_div_u") &&
        emit(context, "  mov R3, R2") && emit(context, "  ieq R3, 0") &&
        emit(context, "  jt R3, __wasm_trap") && emit(context, "  mov R3, 0") &&
        emit(context, "  mov R4, 0") && emit(context, "  mov R5, 32") &&
        emit_label(context, "__wasm_i32_div_u_loop") &&
        emit(context, "  mov R6, R1") && emit(context, "  mov R7, -31") &&
        emit(context, "  shl R6, R7") && emit(context, "  shl R1, 1") &&
        emit(context, "  shl R4, 1") && emit(context, "  or R4, R6") &&
        emit(context, "  mov R6, R4") && emit(context, "  xor R6, 0x80000000") &&
        emit(context, "  mov R7, R2") && emit(context, "  xor R7, 0x80000000") &&
        emit(context, "  ige R6, R7") && emit(context, "  jf R6, __wasm_i32_div_u_skip") &&
        emit(context, "  isub R4, R2") && emit(context, "  shl R3, 1") &&
        emit(context, "  or R3, 1") && emit(context, "  jmp __wasm_i32_div_u_decrement") &&
        emit_label(context, "__wasm_i32_div_u_skip") && emit(context, "  shl R3, 1") &&
        emit_label(context, "__wasm_i32_div_u_decrement") && emit(context, "  isub R5, 1") &&
        emit(context, "  jt R5, __wasm_i32_div_u_loop") && emit(context, "  mov R0, R3") &&
        emit(context, "  ret");
}

static bool lower_expression(Context *context, const WasmExpr *expression, Value *value)
{
    Value left = {0}, right = {0}, condition = {0}; char label[64], end[64], false_label[64]; size_t index;
    value->present = false;
    switch (expression->kind) {
    case WASM_EXPR_I32_CONST: { int slot = temp_slot(context); if (slot == 0 || !emit(context, "  mov R1, 0x%08X", (uint32_t)expression->i32_value) || !store_slot(context, slot, 1)) return false; value->slot = slot; value->present = true; return true; }
    case WASM_EXPR_F32_CONST: { union { float value; uint32_t bits; } constant; int slot = temp_slot(context); constant.value = expression->f32_value; if (slot == 0 || !emit(context, "  mov R1, 0x%08X", constant.bits) || !store_slot(context, slot, 1)) return false; value->slot = slot; value->present = true; return true; }
    case WASM_EXPR_LOCAL_GET: { int slot = temp_slot(context); if (slot == 0 || !load_slot(context, 1, local_slot(context->function, expression->index)) || !store_slot(context, slot, 1)) return false; value->slot = slot; value->present = true; return true; }
    case WASM_EXPR_LOCAL_SET:
        if (!lower_expression(context, expression->children[0], &left) || !left.present || !load_slot(context, 1, left.slot) || !store_slot(context, local_slot(context->function, expression->index), 1)) return false;
        if (expression->is_tee) { *value = left; return true; } release(context, left); return true;
    case WASM_EXPR_UNARY:
        if (!lower_expression(context, expression->children[0], &left) || !left.present ||
            !load_slot(context, 1, left.slot)) return false;
        if (expression->unary_op == WASM_UNARY_EQZ) { if (!emit(context, "  ieq R1, 0")) return false; }
        else if (expression->unary_op == WASM_UNARY_CONVERT_I32_S_TO_F32) { if (!emit(context, "  cif R1")) return false; }
        else return false;
        if (!store_slot(context, left.slot, 1)) return false;
        *value = left; return true;
    case WASM_EXPR_BINARY: return lower_binary(context, expression, value);
    case WASM_EXPR_SELECT:
        /* Children retain Wasm's evaluation order: first, second, condition. */
        if (!lower_expression(context, expression->children[0], &left) ||
            !lower_expression(context, expression->children[1], &right) ||
            !lower_expression(context, expression->children[2], &condition) ||
            !left.present || !right.present || !condition.present ||
            !fresh_label(context, "select_false", false_label, sizeof(false_label)) ||
            !fresh_label(context, "select_done", end, sizeof(end)) ||
            !load_slot(context, 1, condition.slot) || !emit(context, "  jf R1, %s", false_label) ||
            !load_slot(context, 1, left.slot) || !emit(context, "  jmp %s", end) ||
            !emit_label(context, false_label) || !load_slot(context, 1, right.slot) ||
            !emit_label(context, end) || !store_slot(context, left.slot, 1)) return false;
        release(context, condition); release(context, right); *value = left; return true;
    case WASM_EXPR_LOAD: return lower_load(context, expression, value);
    case WASM_EXPR_STORE: return lower_store(context, expression, value);
    case WASM_EXPR_CALL: return lower_call(context, expression, value);
    case WASM_EXPR_BLOCK:
        if (expression->name != NULL) { if (!fresh_label(context, "block_end", label, sizeof(label)) || context->target_count == 32) return false; context->targets[context->target_count++] = (Target){expression->name, format_text("%s", label)}; }
        for (index = 0; index < expression->child_count; ++index) { if (value->present) release(context, *value); value->present = false; if (!lower_expression(context, expression->children[index], value)) return false; }
        if (expression->name != NULL) { Target target = context->targets[--context->target_count]; if (!emit_label(context, target.label)) { free(target.label); return false; } free(target.label); } return true;
    case WASM_EXPR_LOOP:
        if (!fresh_label(context, "loop", label, sizeof(label)) || context->target_count == 32) return false;
        context->targets[context->target_count++] = (Target){expression->name, format_text("%s", label)};
        if (!emit_label(context, label) || !lower_expression(context, expression->children[0], value)) return false;
        free(context->targets[--context->target_count].label); value->present = false; return true;
    case WASM_EXPR_BR:
        for (index = context->target_count; index != 0; --index) if (strcmp(context->targets[index - 1].wasm_name, expression->name) == 0) return emit(context, "  jmp %s", context->targets[index - 1].label);
        diagnostics_error(context->diagnostics, "branch targets '%s' outside active structured control", expression->name); return false;
    case WASM_EXPR_BR_IF:
        if (!lower_expression(context, expression->children[0], &condition) || !condition.present || !load_slot(context, 1, condition.slot)) return false;
        release(context, condition);
        for (index = context->target_count; index != 0; --index)
            if (strcmp(context->targets[index - 1].wasm_name, expression->name) == 0)
                return emit(context, "  jt R1, %s", context->targets[index - 1].label);
        diagnostics_error(context->diagnostics, "conditional branch targets '%s' outside active structured control", expression->name); return false;
    case WASM_EXPR_IF:
        if (!lower_expression(context, expression->children[0], &left) || !left.present || !fresh_label(context, "if_end", end, sizeof(end)) || !load_slot(context, 1, left.slot) || !emit(context, "  jf R1, %s", end)) return false;
        release(context, left); if (!lower_expression(context, expression->children[1], &right)) return false; release(context, right); return emit_label(context, end);
    case WASM_EXPR_RETURN:
        if (expression->child_count != 0) { if (!lower_expression(context, expression->children[0], &left) || !left.present || !load_slot(context, 0, left.slot)) return false; release(context, left); }
        return emit(context, "  jmp %s", context->return_label);
    case WASM_EXPR_DROP:
        if (!lower_expression(context, expression->children[0], &left)) return false;
        release(context, left); value->present = false; return true;
    case WASM_EXPR_UNREACHABLE: return emit(context, "  jmp __wasm_trap");
    }
    diagnostics_error(context->diagnostics, "internal error: unhandled Wasm expression"); return false;
}

static bool initialize_data(const ValidatedModule *validated, Context *context)
{
    size_t bytes = (size_t)context->memory_bytes, words = (bytes + 3) / 4, index, segment_index; unsigned char *memory = calloc(bytes, 1); bool *touched = calloc(words, sizeof(*touched));
    if (memory == NULL || touched == NULL) { free(memory); free(touched); diagnostics_error(context->diagnostics, "out of memory constructing initial linear memory"); return false; }
    for (segment_index = 0; segment_index < validated->module->data_segment_count; ++segment_index) { const WasmDataSegment *segment = &validated->module->data_segments[segment_index]; memcpy(memory + segment->offset, segment->bytes, segment->size); for (index = segment->offset / 4; index <= (segment->offset + segment->size - 1) / 4 && segment->size != 0; ++index) touched[index] = true; }
    for (index = 0; index < words; ++index) if (touched[index]) { uint32_t word = (uint32_t)memory[index * 4] | ((uint32_t)memory[index * 4 + 1] << 8) | ((uint32_t)memory[index * 4 + 2] << 16) | ((uint32_t)memory[index * 4 + 3] << 24); if (!emit(context, "  mov R1, 0x%08X", word) || !emit(context, "  mov [%u], R1", LINEAR_BASE + (unsigned)index)) { free(memory); free(touched); return false; } }
    free(memory); free(touched); return true;
}

static bool lower_function(const ValidatedModule *validated, const WasmFunction *function, VirconIrProgram *program, Diagnostics *diagnostics, uint32_t memory_bytes)
{
    Context context = {0}; Value result = {0}; char label[64];
    context.validated = validated; context.function = function; context.program = program; context.diagnostics = diagnostics; context.memory_bytes = memory_bytes;
    function_label(validated->module, function, label, sizeof(label));
    snprintf(context.return_label, sizeof(context.return_label), "__wasm_return_%zu",
             function_index(validated->module, function));
    if (!emit_label(&context, label) || !emit(&context, "  push BP") || !emit(&context, "  mov BP, SP") || !emit(&context, "  isub SP, %u", (unsigned)(function->local_count + TEMP_SLOTS + OUTGOING_SLOTS)) || !lower_expression(&context, function->body, &result)) return false;
    if (function->result == WASM_VALUE_I32) { if (result.present) { if (!load_slot(&context, 0, result.slot)) return false; } else if (!emit(&context, "  mov R0, 0")) return false; }
    release(&context, result);
    return emit_label(&context, context.return_label) && emit(&context, "  mov SP, BP") && emit(&context, "  pop BP") && emit(&context, "  ret");
}

bool lower_module_to_vircon_ir(const ValidatedModule *validated, VirconIrProgram *program, Diagnostics *diagnostics)
{
    Context startup = {0}; uint32_t memory_bytes = validated->module->memory_initial_pages * 65536u; size_t index; char entry_label[64]; bool needs_unsigned_division = false;
    startup.program = program; startup.diagnostics = diagnostics; startup.memory_bytes = memory_bytes;
    function_label(validated->module, validated->entry, entry_label, sizeof(entry_label));
    if (!emit_label(&startup, "__wasm_entry") || !initialize_data(validated, &startup) || !emit(&startup, "  call %s", entry_label) || !emit(&startup, "  hlt") || !emit_label(&startup, "__wasm_trap") || !emit(&startup, "  hlt  ; Wasm memory/unreachable trap")) return false;
    for (index = 0; index < validated->module->function_count; ++index) {
        const WasmFunction *function = &validated->module->functions[index];
        if (validated->reachable[index] && !function->is_import && expression_uses_binary(function->body, WASM_BINARY_DIV_U)) needs_unsigned_division = true;
        if (validated->reachable[index] && !function->is_import && !lower_function(validated, function, program, diagnostics, memory_bytes)) return false;
    }
    if (needs_unsigned_division && !emit_unsigned_division_helper(&startup)) return false;
    return true;
}
