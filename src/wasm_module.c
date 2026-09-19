#include "wasm_module.h"

#include <binaryen-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *source)
{
    size_t length;
    char *copy;

    if (source == NULL) {
        return NULL;
    }
    length = strlen(source) + 1;
    copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, source, length);
    }
    return copy;
}

static void free_expression(WasmExpr *expression)
{
    size_t index;

    if (expression == NULL) {
        return;
    }
    for (index = 0; index < expression->child_count; index++) {
        free_expression(expression->children[index]);
    }
    free(expression->children);
    free(expression->name);
    free(expression);
}

static WasmValueType convert_type(BinaryenType type)
{
    if (type == BinaryenTypeNone()) {
        return WASM_VALUE_NONE;
    }
    if (type == BinaryenTypeInt32()) {
        return WASM_VALUE_I32;
    }
    return WASM_VALUE_OTHER;
}

static bool convert_tuple_type(BinaryenType type,
                               WasmValueType *values,
                               size_t *count,
                               Diagnostics *diagnostics,
                               const char *function_name)
{
    BinaryenIndex arity = BinaryenTypeArity(type);
    BinaryenType expanded[4];
    BinaryenIndex index;

    if (arity > 4) {
        diagnostics_error(diagnostics,
                          "function '%s' has too many parameters for VirconWasm v0",
                          function_name);
        return false;
    }
    if (arity == 0) {
        *count = 0;
        return true;
    }
    BinaryenTypeExpand(type, expanded);
    for (index = 0; index < arity; index++) {
        values[index] = convert_type(expanded[index]);
        if (values[index] == WASM_VALUE_OTHER) {
            diagnostics_error(diagnostics,
                              "function '%s' has an unsupported parameter type",
                              function_name);
            return false;
        }
    }
    *count = arity;
    return true;
}

static WasmExpr *new_expression(WasmExprKind kind, Diagnostics *diagnostics)
{
    WasmExpr *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        diagnostics_error(diagnostics, "out of memory while reading Wasm expression");
        return NULL;
    }
    expression->kind = kind;
    return expression;
}

static WasmExpr *convert_expression(BinaryenExpressionRef binaryen_expression,
                                    Diagnostics *diagnostics,
                                    const char *function_name)
{
    BinaryenExpressionId id;
    WasmExpr *expression;
    BinaryenIndex index;

    if (binaryen_expression == NULL) {
        diagnostics_error(diagnostics,
                          "function '%s' contains a missing expression",
                          function_name);
        return NULL;
    }

    id = BinaryenExpressionGetId(binaryen_expression);
    if (id == BinaryenBlockId()) {
        expression = new_expression(WASM_EXPR_BLOCK, diagnostics);
        if (expression == NULL) {
            return NULL;
        }
        expression->name = copy_string(BinaryenBlockGetName(binaryen_expression));
        expression->child_count = BinaryenBlockGetNumChildren(binaryen_expression);
        if (expression->child_count != 0) {
            expression->children = calloc(expression->child_count,
                                          sizeof(*expression->children));
            if (expression->children == NULL) {
                diagnostics_error(diagnostics, "out of memory while reading Wasm block");
                free_expression(expression);
                return NULL;
            }
        }
        for (index = 0; index < expression->child_count; index++) {
            expression->children[index] = convert_expression(
                BinaryenBlockGetChildAt(binaryen_expression, index), diagnostics,
                function_name);
            if (expression->children[index] == NULL) {
                free_expression(expression);
                return NULL;
            }
        }
        return expression;
    }

    if (id == BinaryenLoopId()) {
        expression = new_expression(WASM_EXPR_LOOP, diagnostics);
        if (expression == NULL) {
            return NULL;
        }
        expression->name = copy_string(BinaryenLoopGetName(binaryen_expression));
        if (expression->name == NULL) {
            diagnostics_error(diagnostics,
                              "function '%s' has an unnamed loop, which cannot be lowered",
                              function_name);
            free_expression(expression);
            return NULL;
        }
        expression->child_count = 1;
        expression->children = calloc(1, sizeof(*expression->children));
        if (expression->children == NULL) {
            diagnostics_error(diagnostics, "out of memory while reading Wasm loop");
            free_expression(expression);
            return NULL;
        }
        expression->children[0] = convert_expression(BinaryenLoopGetBody(binaryen_expression),
                                                      diagnostics, function_name);
        if (expression->children[0] == NULL) {
            free_expression(expression);
            return NULL;
        }
        return expression;
    }

    if (id == BinaryenBreakId()) {
        if (BinaryenBreakGetCondition(binaryen_expression) != NULL ||
            BinaryenBreakGetValue(binaryen_expression) != NULL) {
            diagnostics_error(diagnostics,
                              "function '%s' uses conditional or value-carrying br, which VirconWasm v0 does not support",
                              function_name);
            return NULL;
        }
        expression = new_expression(WASM_EXPR_BR, diagnostics);
        if (expression == NULL) {
            return NULL;
        }
        expression->name = copy_string(BinaryenBreakGetName(binaryen_expression));
        if (expression->name == NULL) {
            diagnostics_error(diagnostics,
                              "function '%s' has a branch without a target label",
                              function_name);
            free_expression(expression);
            return NULL;
        }
        return expression;
    }

    if (id == BinaryenCallId()) {
        expression = new_expression(WASM_EXPR_CALL, diagnostics);
        if (expression == NULL) {
            return NULL;
        }
        expression->name = copy_string(BinaryenCallGetTarget(binaryen_expression));
        expression->child_count = BinaryenCallGetNumOperands(binaryen_expression);
        if (expression->name == NULL) {
            diagnostics_error(diagnostics,
                              "function '%s' contains a call without a target",
                              function_name);
            free_expression(expression);
            return NULL;
        }
        if (expression->child_count != 0) {
            expression->children = calloc(expression->child_count,
                                          sizeof(*expression->children));
            if (expression->children == NULL) {
                diagnostics_error(diagnostics, "out of memory while reading Wasm call");
                free_expression(expression);
                return NULL;
            }
        }
        for (index = 0; index < expression->child_count; index++) {
            expression->children[index] = convert_expression(
                BinaryenCallGetOperandAt(binaryen_expression, index), diagnostics,
                function_name);
            if (expression->children[index] == NULL) {
                free_expression(expression);
                return NULL;
            }
        }
        return expression;
    }

    if (id == BinaryenConstId()) {
        if (BinaryenExpressionGetType(binaryen_expression) != BinaryenTypeInt32()) {
            diagnostics_error(diagnostics,
                              "function '%s' contains a non-i32 constant, which VirconWasm v0 does not support",
                              function_name);
            return NULL;
        }
        expression = new_expression(WASM_EXPR_I32_CONST, diagnostics);
        if (expression != NULL) {
            expression->i32_value = BinaryenConstGetValueI32(binaryen_expression);
        }
        return expression;
    }

    if (id == BinaryenUnreachableId()) {
        return new_expression(WASM_EXPR_UNREACHABLE, diagnostics);
    }

    diagnostics_error(diagnostics,
                      "function '%s' contains unsupported Wasm expression kind %u",
                      function_name, (unsigned)id);
    return NULL;
}

static bool read_file(const char *path, char **contents, size_t *size,
                      Diagnostics *diagnostics)
{
    FILE *file;
    long file_size;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        diagnostics_error(diagnostics, "cannot open input module '%s'", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        diagnostics_error(diagnostics, "cannot determine size of input module '%s'", path);
        fclose(file);
        return false;
    }
    buffer = malloc((size_t)file_size == 0 ? 1 : (size_t)file_size);
    if (buffer == NULL) {
        diagnostics_error(diagnostics, "out of memory reading input module '%s'", path);
        fclose(file);
        return false;
    }
    if (file_size != 0 && fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size) {
        diagnostics_error(diagnostics, "cannot read input module '%s'", path);
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);
    *contents = buffer;
    *size = (size_t)file_size;
    return true;
}

/*
 * Binaryen 130 exposes MemoryImportGetModule only by internal memory name,
 * but its C API has no memory enumeration/name accessor. A guessed name makes
 * Binaryen terminate. Use Binaryen's own text serialization only to reject
 * this unsupported module-level form; Wasm binary decoding remains Binaryen's
 * responsibility.
 */
static bool text_has_memory_import(const char *text)
{
    const char *line = text;

    while (*line != '\0') {
        const char *line_end = strchr(line, '\n');
        const char *import_marker = strstr(line, "(import ");
        const char *memory_marker = strstr(line, "(memory");

        if (line_end == NULL) {
            line_end = line + strlen(line);
        }
        if (import_marker != NULL && memory_marker != NULL &&
            import_marker < line_end && memory_marker < line_end) {
            return true;
        }
        line = *line_end == '\0' ? line_end : line_end + 1;
    }
    return false;
}

bool wasm_module_load(const char *path, WasmModule *module, Diagnostics *diagnostics)
{
    char *contents = NULL;
    size_t size = 0;
    BinaryenModuleRef binaryen_module = NULL;
    BinaryenIndex index;

    memset(module, 0, sizeof(*module));
    if (!read_file(path, &contents, &size, diagnostics)) {
        return false;
    }

    binaryen_module = BinaryenModuleReadWithFeatures(contents, size,
                                                      BinaryenFeatureAll());
    free(contents);
    if (binaryen_module == NULL) {
        diagnostics_error(diagnostics, "Binaryen could not load '%s' as a Wasm module", path);
        return false;
    }
    if (!BinaryenModuleValidate(binaryen_module)) {
        diagnostics_error(diagnostics, "Binaryen rejected '%s' as an invalid Wasm module", path);
        BinaryenModuleDispose(binaryen_module);
        return false;
    }

    module->has_memory = BinaryenHasMemory(binaryen_module);
    {
        char *module_text = BinaryenModuleAllocateAndWriteText(binaryen_module);
        if (module_text == NULL) {
            diagnostics_error(diagnostics, "Binaryen could not inspect Wasm module imports");
            BinaryenModuleDispose(binaryen_module);
            wasm_module_dispose(module);
            return false;
        }
        module->has_imported_memory = text_has_memory_import(module_text);
        free(module_text);
    }
    module->table_count = BinaryenGetNumTables(binaryen_module);
    module->global_count = BinaryenGetNumGlobals(binaryen_module);
    module->element_segment_count = BinaryenGetNumElementSegments(binaryen_module);
    module->data_segment_count = BinaryenGetNumDataSegments(binaryen_module);

    module->function_count = BinaryenGetNumFunctions(binaryen_module);
    module->functions = calloc(module->function_count, sizeof(*module->functions));
    if (module->function_count != 0 && module->functions == NULL) {
        diagnostics_error(diagnostics, "out of memory while reading Wasm functions");
        BinaryenModuleDispose(binaryen_module);
        wasm_module_dispose(module);
        return false;
    }
    for (index = 0; index < module->function_count; index++) {
        BinaryenFunctionRef source = BinaryenGetFunctionByIndex(binaryen_module, index);
        WasmFunction *destination = &module->functions[index];
        const char *import_module;

        destination->name = copy_string(BinaryenFunctionGetName(source));
        if (destination->name == NULL ||
            !convert_tuple_type(BinaryenFunctionGetParams(source), destination->params,
                                &destination->param_count, diagnostics,
                                destination->name == NULL ? "<unnamed>" : destination->name)) {
            diagnostics_error(diagnostics, "out of memory or invalid function metadata");
            BinaryenModuleDispose(binaryen_module);
            wasm_module_dispose(module);
            return false;
        }
        destination->result = convert_type(BinaryenFunctionGetResults(source));
        if (destination->result == WASM_VALUE_OTHER) {
            diagnostics_error(diagnostics, "function '%s' has an unsupported result type",
                              destination->name);
            BinaryenModuleDispose(binaryen_module);
            wasm_module_dispose(module);
            return false;
        }

        import_module = BinaryenFunctionImportGetModule(source);
        /* Binaryen uses an empty string, rather than NULL, for non-imports. */
        destination->is_import = import_module != NULL && import_module[0] != '\0';
        if (destination->is_import) {
            destination->import_module = copy_string(import_module);
            destination->import_name = copy_string(BinaryenFunctionImportGetBase(source));
            if (destination->import_module == NULL || destination->import_name == NULL) {
                diagnostics_error(diagnostics, "out of memory while reading Wasm import");
                BinaryenModuleDispose(binaryen_module);
                wasm_module_dispose(module);
                return false;
            }
        } else {
            destination->body = convert_expression(BinaryenFunctionGetBody(source), diagnostics,
                                                   destination->name);
            if (destination->body == NULL) {
                BinaryenModuleDispose(binaryen_module);
                wasm_module_dispose(module);
                return false;
            }
        }
    }

    module->export_count = BinaryenGetNumExports(binaryen_module);
    module->exports = calloc(module->export_count, sizeof(*module->exports));
    if (module->export_count != 0 && module->exports == NULL) {
        diagnostics_error(diagnostics, "out of memory while reading Wasm exports");
        BinaryenModuleDispose(binaryen_module);
        wasm_module_dispose(module);
        return false;
    }
    for (index = 0; index < module->export_count; index++) {
        BinaryenExportRef source = BinaryenGetExportByIndex(binaryen_module, index);
        WasmExport *destination = &module->exports[index];

        destination->name = copy_string(BinaryenExportGetName(source));
        destination->value = copy_string(BinaryenExportGetValue(source));
        destination->is_function = BinaryenExportGetKind(source) == BinaryenExternalFunction();
        if (destination->name == NULL || destination->value == NULL) {
            diagnostics_error(diagnostics, "out of memory while reading Wasm export");
            BinaryenModuleDispose(binaryen_module);
            wasm_module_dispose(module);
            return false;
        }
    }

    BinaryenModuleDispose(binaryen_module);
    return true;
}

void wasm_module_dispose(WasmModule *module)
{
    size_t index;

    for (index = 0; index < module->function_count; index++) {
        WasmFunction *function = &module->functions[index];
        free(function->name);
        free(function->import_module);
        free(function->import_name);
        free_expression(function->body);
    }
    for (index = 0; index < module->export_count; index++) {
        free(module->exports[index].name);
        free(module->exports[index].value);
    }
    free(module->functions);
    free(module->exports);
    memset(module, 0, sizeof(*module));
}

const WasmFunction *wasm_module_find_function(const WasmModule *module,
                                              const char *name)
{
    size_t index;

    for (index = 0; index < module->function_count; index++) {
        if (strcmp(module->functions[index].name, name) == 0) {
            return &module->functions[index];
        }
    }
    return NULL;
}
