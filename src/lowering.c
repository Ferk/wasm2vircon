#include "lowering.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ControlTarget {
    const char *wasm_name;
    char *assembly_label;
} ControlTarget;

typedef struct LoweringContext {
    VirconIrProgram *program;
    Diagnostics *diagnostics;
    const WasmModule *module;
    ControlTarget targets[32];
    size_t target_count;
    unsigned next_label;
} LoweringContext;

typedef struct VirconValue {
    int register_number;
} VirconValue;

static char *copy_string(const char *source)
{
    size_t length = strlen(source) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, source, length);
    }
    return copy;
}

static bool emit_label(LoweringContext *context, const char *label)
{
    VirconIrInstruction instruction = {0};

    instruction.op = VIRCON_IR_LABEL;
    instruction.label = copy_string(label);
    if (instruction.label == NULL) {
        diagnostics_error(context->diagnostics, "out of memory while naming Vircon label");
        return false;
    }
    if (!vircon_ir_append(context->program, instruction, context->diagnostics)) {
        free(instruction.label);
        return false;
    }
    return true;
}

static bool emit_jump(LoweringContext *context, const char *label)
{
    VirconIrInstruction instruction = {0};

    instruction.op = VIRCON_IR_JUMP;
    instruction.label = copy_string(label);
    if (instruction.label == NULL) {
        diagnostics_error(context->diagnostics, "out of memory while naming branch target");
        return false;
    }
    if (!vircon_ir_append(context->program, instruction, context->diagnostics)) {
        free(instruction.label);
        return false;
    }
    return true;
}

static bool emit_simple(LoweringContext *context, VirconIrOp op)
{
    VirconIrInstruction instruction = {0};

    instruction.op = op;
    return vircon_ir_append(context->program, instruction, context->diagnostics);
}

static char *fresh_label(LoweringContext *context, const char *kind)
{
    char text[64];
    int written = snprintf(text, sizeof(text), "__wasm_%s_%u", kind,
                           context->next_label++);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        diagnostics_error(context->diagnostics, "could not construct a Vircon label");
        return NULL;
    }
    return copy_string(text);
}

static bool push_target(LoweringContext *context, const char *wasm_name,
                        char *assembly_label)
{
    if (context->target_count == sizeof(context->targets) / sizeof(context->targets[0])) {
        diagnostics_error(context->diagnostics, "structured control nesting exceeds VirconWasm v0 limit");
        return false;
    }
    context->targets[context->target_count].wasm_name = wasm_name;
    context->targets[context->target_count].assembly_label = assembly_label;
    context->target_count++;
    return true;
}

static const char *find_target(const LoweringContext *context, const char *wasm_name)
{
    size_t index;

    for (index = context->target_count; index != 0; index--) {
        if (context->targets[index - 1].wasm_name != NULL &&
            strcmp(context->targets[index - 1].wasm_name, wasm_name) == 0) {
            return context->targets[index - 1].assembly_label;
        }
    }
    return NULL;
}

static bool lower_expression(LoweringContext *context, const WasmExpr *expression,
                             VirconValue *value, bool *has_value);

static bool lower_call(LoweringContext *context, const WasmExpr *expression,
                       VirconValue *value, bool *has_value)
{
    VirconValue argument;
    bool argument_has_value;
    VirconIrInstruction instruction = {0};

    const WasmFunction *callee = wasm_module_find_function(context->module, expression->name);

    if (callee == NULL || !callee->is_import) {
        diagnostics_error(context->diagnostics,
                          "internal error: call '%s' passed validation without an import",
                          expression->name);
        return false;
    }
    if (strcmp(callee->import_module, "env") == 0 &&
        strcmp(callee->import_name, "vircon_set_background_color") == 0) {
        if (!lower_expression(context, expression->children[0], &argument,
                              &argument_has_value) || !argument_has_value) {
            diagnostics_error(context->diagnostics,
                              "background-colour import did not receive an i32 value");
            return false;
        }
        instruction.op = VIRCON_IR_SET_BACKGROUND_COLOR;
        instruction.register_number = argument.register_number;
    } else if (strcmp(callee->import_module, "env") == 0 &&
               strcmp(callee->import_name, "vircon_end_frame") == 0) {
        instruction.op = VIRCON_IR_END_FRAME;
    } else {
        diagnostics_error(context->diagnostics,
                          "internal error: unsupported call '%s.%s' passed validation",
                          callee->import_module, callee->import_name);
        return false;
    }
    if (!vircon_ir_append(context->program, instruction, context->diagnostics)) {
        return false;
    }
    *has_value = false;
    (void)value;
    return true;
}

static bool lower_expression(LoweringContext *context, const WasmExpr *expression,
                             VirconValue *value, bool *has_value)
{
    size_t index;
    char *label;
    const char *target;
    VirconIrInstruction instruction = {0};

    switch (expression->kind) {
    case WASM_EXPR_BLOCK:
        label = NULL;
        if (expression->name != NULL) {
            label = fresh_label(context, "block_end");
            if (label == NULL || !push_target(context, expression->name, label)) {
                free(label);
                return false;
            }
        }
        for (index = 0; index < expression->child_count; index++) {
            if (!lower_expression(context, expression->children[index], value, has_value)) {
                if (label != NULL) {
                    context->target_count--;
                    free(label);
                }
                return false;
            }
        }
        if (label != NULL) {
            context->target_count--;
            if (!emit_label(context, label)) {
                free(label);
                return false;
            }
            free(label);
        }
        *has_value = false;
        return true;
    case WASM_EXPR_LOOP:
        label = fresh_label(context, "loop");
        if (label == NULL || !push_target(context, expression->name, label)) {
            free(label);
            return false;
        }
        if (!emit_label(context, label) ||
            !lower_expression(context, expression->children[0], value, has_value)) {
            context->target_count--;
            free(label);
            return false;
        }
        context->target_count--;
        free(label);
        *has_value = false;
        return true;
    case WASM_EXPR_BR:
        target = find_target(context, expression->name);
        if (target == NULL) {
            diagnostics_error(context->diagnostics,
                              "branch targets '%s' outside active structured control", expression->name);
            return false;
        }
        *has_value = false;
        return emit_jump(context, target);
    case WASM_EXPR_CALL:
        return lower_call(context, expression, value, has_value);
    case WASM_EXPR_I32_CONST:
        instruction.op = VIRCON_IR_MOV_I32;
        instruction.register_number = 1;
        instruction.i32_value = expression->i32_value;
        if (!vircon_ir_append(context->program, instruction, context->diagnostics)) {
            return false;
        }
        value->register_number = instruction.register_number;
        *has_value = true;
        return true;
    case WASM_EXPR_UNREACHABLE:
        *has_value = false;
        return emit_simple(context, VIRCON_IR_UNREACHABLE);
    }
    diagnostics_error(context->diagnostics, "internal error: unknown Wasm expression in lowering");
    return false;
}

bool lower_entry_to_vircon_ir(const ValidatedModule *module,
                              VirconIrProgram *program,
                              Diagnostics *diagnostics)
{
    LoweringContext context = {0};
    VirconValue value = {0};
    bool has_value = false;

    context.program = program;
    context.diagnostics = diagnostics;
    context.module = module->module;
    if (!emit_label(&context, "__wasm_entry") ||
        !lower_expression(&context, module->entry->body, &value, &has_value) ||
        !emit_simple(&context, VIRCON_IR_HALT)) {
        return false;
    }
    return true;
}
