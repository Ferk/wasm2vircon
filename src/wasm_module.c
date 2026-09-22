#include "wasm_module.h"

#include <binaryen-c.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *source)
{
    size_t length; char *copy;
    if (source == NULL) return NULL;
    length = strlen(source) + 1; copy = malloc(length);
    if (copy != NULL) memcpy(copy, source, length);
    return copy;
}

static void free_expression(WasmExpr *expression)
{
    size_t index;
    if (expression == NULL) return;
    for (index = 0; index < expression->child_count; ++index) free_expression(expression->children[index]);
    free(expression->children); free(expression->name); free(expression);
}

static WasmValueType convert_type(BinaryenType type)
{
    if (type == BinaryenTypeNone()) return WASM_VALUE_NONE;
    if (type == BinaryenTypeInt32()) return WASM_VALUE_I32;
    if (type == BinaryenTypeFloat32()) return WASM_VALUE_F32;
    return WASM_VALUE_OTHER;
}

static bool convert_tuple_type(BinaryenType type, WasmValueType *values, size_t *count,
                               Diagnostics *diagnostics, const char *function_name)
{
    BinaryenIndex arity = BinaryenTypeArity(type), index; BinaryenType expanded[4];
    if (arity > 4) { diagnostics_error(diagnostics, "function '%s' has more than four parameters", function_name); return false; }
    if (arity == 0) { *count = 0; return true; }
    BinaryenTypeExpand(type, expanded);
    for (index = 0; index < arity; ++index) {
        values[index] = convert_type(expanded[index]);
        if (values[index] == WASM_VALUE_OTHER) { diagnostics_error(diagnostics, "function '%s' has an unsupported parameter type", function_name); return false; }
    }
    *count = arity; return true;
}

static WasmExpr *new_expression(WasmExprKind kind, Diagnostics *diagnostics)
{
    WasmExpr *expression = calloc(1, sizeof(*expression));
    if (expression == NULL) diagnostics_error(diagnostics, "out of memory while reading Wasm expression");
    else expression->kind = kind;
    return expression;
}

static bool allocate_children(WasmExpr *expression, size_t count, Diagnostics *diagnostics)
{
    expression->child_count = count;
    if (count == 0) return true;
    expression->children = calloc(count, sizeof(*expression->children));
    if (expression->children == NULL) { diagnostics_error(diagnostics, "out of memory while reading Wasm expression children"); return false; }
    return true;
}

static WasmExpr *convert_expression(BinaryenExpressionRef source, Diagnostics *diagnostics,
                                    const char *function_name)
{
    BinaryenExpressionId id; WasmExpr *expression; BinaryenIndex index;
    if (source == NULL) { diagnostics_error(diagnostics, "function '%s' contains a missing expression", function_name); return NULL; }
    id = BinaryenExpressionGetId(source);
    if (id == BinaryenBlockId()) {
        expression = new_expression(WASM_EXPR_BLOCK, diagnostics); if (expression == NULL) return NULL;
        expression->name = copy_string(BinaryenBlockGetName(source));
        if (!allocate_children(expression, BinaryenBlockGetNumChildren(source), diagnostics)) goto fail;
        for (index = 0; index < expression->child_count; ++index) { expression->children[index] = convert_expression(BinaryenBlockGetChildAt(source, index), diagnostics, function_name); if (expression->children[index] == NULL) goto fail; }
        return expression;
    }
    if (id == BinaryenLoopId()) {
        expression = new_expression(WASM_EXPR_LOOP, diagnostics); if (expression == NULL) return NULL;
        if (BinaryenExpressionGetType(source) != BinaryenTypeNone() &&
            BinaryenExpressionGetType(source) != BinaryenTypeUnreachable()) {
            diagnostics_error(diagnostics, "function '%s' contains a value-producing loop", function_name);
            goto fail;
        }
        expression->name = copy_string(BinaryenLoopGetName(source));
        if (expression->name == NULL) { diagnostics_error(diagnostics, "function '%s' has an unnamed loop", function_name); goto fail; }
        if (!allocate_children(expression, 1, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenLoopGetBody(source), diagnostics, function_name);
        if (expression->children[0] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenBreakId()) {
        expression = new_expression(WASM_EXPR_BR, diagnostics); if (expression == NULL) return NULL;
        expression->name = copy_string(BinaryenBreakGetName(source));
        if (expression->name == NULL) { diagnostics_error(diagnostics, "function '%s' has a branch without a target", function_name); goto fail; }
        if (BinaryenBreakGetValue(source) != NULL) { diagnostics_error(diagnostics, "function '%s' uses a value-carrying br, which is unsupported", function_name); goto fail; }
        if (BinaryenBreakGetCondition(source) != NULL) {
            expression->kind = WASM_EXPR_BR_IF;
            if (!allocate_children(expression, 1, diagnostics)) goto fail;
            expression->children[0] = convert_expression(BinaryenBreakGetCondition(source), diagnostics, function_name);
            if (expression->children[0] == NULL) goto fail;
        }
        return expression;
    }
    if (id == BinaryenCallId()) {
        expression = new_expression(WASM_EXPR_CALL, diagnostics); if (expression == NULL) return NULL;
        expression->name = copy_string(BinaryenCallGetTarget(source));
        if (expression->name == NULL || !allocate_children(expression, BinaryenCallGetNumOperands(source), diagnostics)) goto fail;
        for (index = 0; index < expression->child_count; ++index) { expression->children[index] = convert_expression(BinaryenCallGetOperandAt(source, index), diagnostics, function_name); if (expression->children[index] == NULL) goto fail; }
        return expression;
    }
    if (id == BinaryenConstId()) {
        if (BinaryenExpressionGetType(source) == BinaryenTypeInt32()) {
            expression = new_expression(WASM_EXPR_I32_CONST, diagnostics); if (expression != NULL) expression->i32_value = BinaryenConstGetValueI32(source); return expression;
        }
        if (BinaryenExpressionGetType(source) == BinaryenTypeFloat32()) {
            expression = new_expression(WASM_EXPR_F32_CONST, diagnostics); if (expression != NULL) expression->f32_value = BinaryenConstGetValueF32(source); return expression;
        }
        diagnostics_error(diagnostics, "function '%s' contains an unsupported constant type", function_name); return NULL;
    }
    if (id == BinaryenUnreachableId()) return new_expression(WASM_EXPR_UNREACHABLE, diagnostics);
    if (id == BinaryenIfId()) {
        expression = new_expression(WASM_EXPR_IF, diagnostics); if (expression == NULL) return NULL;
        if (BinaryenExpressionGetType(source) != BinaryenTypeNone()) { diagnostics_error(diagnostics, "function '%s' contains a value-producing if", function_name); goto fail; }
        if (!allocate_children(expression, BinaryenIfGetIfFalse(source) == NULL ? 2 : 3, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenIfGetCondition(source), diagnostics, function_name);
        expression->children[1] = convert_expression(BinaryenIfGetIfTrue(source), diagnostics, function_name);
        if (expression->children[0] == NULL || expression->children[1] == NULL) goto fail;
        if (expression->child_count == 3) { expression->children[2] = convert_expression(BinaryenIfGetIfFalse(source), diagnostics, function_name); if (expression->children[2] == NULL) goto fail; }
        return expression;
    }
    if (id == BinaryenLocalGetId()) { expression = new_expression(WASM_EXPR_LOCAL_GET, diagnostics); if (expression != NULL) expression->index = BinaryenLocalGetGetIndex(source); return expression; }
    if (id == BinaryenLocalSetId()) {
        expression = new_expression(WASM_EXPR_LOCAL_SET, diagnostics); if (expression == NULL) return NULL;
        expression->index = BinaryenLocalSetGetIndex(source); expression->is_tee = BinaryenLocalSetIsTee(source);
        if (!allocate_children(expression, 1, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenLocalSetGetValue(source), diagnostics, function_name); if (expression->children[0] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenLoadId()) {
        expression = new_expression(WASM_EXPR_LOAD, diagnostics); if (expression == NULL) return NULL;
        expression->bytes = BinaryenLoadGetBytes(source); expression->offset = BinaryenLoadGetOffset(source); expression->align = BinaryenLoadGetAlign(source); expression->is_signed = BinaryenLoadIsSigned(source);
        if (!allocate_children(expression, 1, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenLoadGetPtr(source), diagnostics, function_name); if (expression->children[0] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenStoreId()) {
        expression = new_expression(WASM_EXPR_STORE, diagnostics); if (expression == NULL) return NULL;
        expression->bytes = BinaryenStoreGetBytes(source); expression->offset = BinaryenStoreGetOffset(source); expression->align = BinaryenStoreGetAlign(source);
        if (expression->bytes == 8) {
            BinaryenExpressionRef stored_value = BinaryenStoreGetValue(source);
            /* This is intentionally not general i64 support. Clang can fold
             * neighbouring i32 initializers into this exact store shape. */
            if (BinaryenStoreGetValueType(source) != BinaryenTypeInt64() ||
                BinaryenExpressionGetId(stored_value) != BinaryenConstId() ||
                BinaryenExpressionGetType(stored_value) != BinaryenTypeInt64()) {
                diagnostics_error(diagnostics, "function '%s' uses an unsupported i64.store; only an i64.const initializer is accepted", function_name);
                goto fail;
            }
            expression->kind = WASM_EXPR_I64_CONST_STORE;
            expression->i64_value = (uint64_t)BinaryenConstGetValueI64(stored_value);
            if (!allocate_children(expression, 1, diagnostics)) goto fail;
            expression->children[0] = convert_expression(BinaryenStoreGetPtr(source), diagnostics, function_name);
            if (expression->children[0] == NULL) goto fail;
            return expression;
        }
        if (!allocate_children(expression, 2, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenStoreGetPtr(source), diagnostics, function_name); expression->children[1] = convert_expression(BinaryenStoreGetValue(source), diagnostics, function_name);
        if (expression->children[0] == NULL || expression->children[1] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenUnaryId()) {
        BinaryenOp op = BinaryenUnaryGetOp(source);
        expression = new_expression(WASM_EXPR_UNARY, diagnostics); if (expression == NULL) return NULL;
        expression->unary_op = op == BinaryenEqZInt32() ? WASM_UNARY_EQZ :
            op == BinaryenConvertSInt32ToFloat32() ? WASM_UNARY_CONVERT_I32_S_TO_F32 :
            op == BinaryenConvertUInt32ToFloat32() ? WASM_UNARY_CONVERT_I32_U_TO_F32 :
            op == BinaryenTruncSatSFloat32ToInt32() ? WASM_UNARY_TRUNC_SAT_F32_TO_I32 : WASM_UNARY_OTHER;
        if (!allocate_children(expression, 1, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenUnaryGetValue(source), diagnostics, function_name);
        if (expression->children[0] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenBinaryId()) {
        BinaryenOp op = BinaryenBinaryGetOp(source); expression = new_expression(WASM_EXPR_BINARY, diagnostics); if (expression == NULL) return NULL;
        expression->binary_op = op == BinaryenAddInt32() ? WASM_BINARY_ADD :
            op == BinaryenSubInt32() ? WASM_BINARY_SUB :
            op == BinaryenMulInt32() ? WASM_BINARY_MUL :
            op == BinaryenDivSInt32() ? WASM_BINARY_DIV_S :
            op == BinaryenDivUInt32() ? WASM_BINARY_DIV_U :
            op == BinaryenRemSInt32() ? WASM_BINARY_REM_S :
            op == BinaryenShlInt32() ? WASM_BINARY_SHL :
            op == BinaryenShrSInt32() ? WASM_BINARY_SHR_S :
            op == BinaryenAndInt32() ? WASM_BINARY_AND :
            op == BinaryenXorInt32() ? WASM_BINARY_XOR :
            op == BinaryenEqInt32() ? WASM_BINARY_EQ :
            op == BinaryenNeInt32() ? WASM_BINARY_NE :
            op == BinaryenLtSInt32() ? WASM_BINARY_LT_S :
            op == BinaryenLtUInt32() ? WASM_BINARY_LT_U :
            op == BinaryenGtSInt32() ? WASM_BINARY_GT_S :
            op == BinaryenGtUInt32() ? WASM_BINARY_GT_U :
            op == BinaryenGeSInt32() ? WASM_BINARY_GE_S :
            op == BinaryenGeUInt32() ? WASM_BINARY_GE_U :
            op == BinaryenLeSInt32() ? WASM_BINARY_LE_S :
            op == BinaryenOrInt32() ? WASM_BINARY_OR :
            op == BinaryenRemUInt32() ? WASM_BINARY_REM_U :
            op == BinaryenShrUInt32() ? WASM_BINARY_SHR_U :
            op == BinaryenAddFloat32() ? WASM_BINARY_F32_ADD :
            op == BinaryenSubFloat32() ? WASM_BINARY_F32_SUB :
            op == BinaryenLeFloat32() ? WASM_BINARY_F32_LE :
            op == BinaryenLtFloat32() ? WASM_BINARY_F32_LT :
            op == BinaryenMulFloat32() ? WASM_BINARY_F32_MUL :
            op == BinaryenDivFloat32() ? WASM_BINARY_F32_DIV :
            op == BinaryenGtFloat32() ? WASM_BINARY_F32_GT : WASM_BINARY_OTHER;
        if (!allocate_children(expression, 2, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenBinaryGetLeft(source), diagnostics, function_name); expression->children[1] = convert_expression(BinaryenBinaryGetRight(source), diagnostics, function_name);
        if (expression->children[0] == NULL || expression->children[1] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenSelectId()) {
        expression = new_expression(WASM_EXPR_SELECT, diagnostics); if (expression == NULL) return NULL;
        expression->value_type = convert_type(BinaryenExpressionGetType(source));
        if (expression->value_type != WASM_VALUE_I32 && expression->value_type != WASM_VALUE_F32) { diagnostics_error(diagnostics, "function '%s' contains an unsupported select result type", function_name); goto fail; }
        /* Preserve Wasm evaluation order: first value, second value, condition. */
        if (!allocate_children(expression, 3, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenSelectGetIfTrue(source), diagnostics, function_name);
        expression->children[1] = convert_expression(BinaryenSelectGetIfFalse(source), diagnostics, function_name);
        expression->children[2] = convert_expression(BinaryenSelectGetCondition(source), diagnostics, function_name);
        if (expression->children[0] == NULL || expression->children[1] == NULL || expression->children[2] == NULL) goto fail;
        return expression;
    }
    if (id == BinaryenReturnId()) {
        expression = new_expression(WASM_EXPR_RETURN, diagnostics); if (expression == NULL) return NULL;
        if (BinaryenReturnGetValue(source) != NULL) { if (!allocate_children(expression, 1, diagnostics)) goto fail; expression->children[0] = convert_expression(BinaryenReturnGetValue(source), diagnostics, function_name); if (expression->children[0] == NULL) goto fail; }
        return expression;
    }
    if (id == BinaryenDropId()) {
        expression = new_expression(WASM_EXPR_DROP, diagnostics); if (expression == NULL) return NULL;
        if (!allocate_children(expression, 1, diagnostics)) goto fail;
        expression->children[0] = convert_expression(BinaryenDropGetValue(source), diagnostics, function_name);
        if (expression->children[0] == NULL) goto fail;
        return expression;
    }
    diagnostics_error(diagnostics, "function '%s' contains unsupported Wasm expression kind %u", function_name, (unsigned)id); return NULL;
fail:
    free_expression(expression); return NULL;
}

static bool read_file(const char *path, char **contents, size_t *size, Diagnostics *diagnostics)
{
    FILE *file = fopen(path, "rb"); long length; char *buffer;
    if (file == NULL) { diagnostics_error(diagnostics, "cannot open input module '%s'", path); return false; }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) { diagnostics_error(diagnostics, "cannot determine size of input module '%s'", path); fclose(file); return false; }
    buffer = malloc(length == 0 ? 1 : (size_t)length);
    if (buffer == NULL || (length != 0 && fread(buffer, 1, (size_t)length, file) != (size_t)length)) { diagnostics_error(diagnostics, "cannot read input module '%s'", path); free(buffer); fclose(file); return false; }
    fclose(file); *contents = buffer; *size = (size_t)length; return true;
}

/* Binaryen 130 has no memory enumerator. Its own text form supplies the sole
 * core-memory name and limits; Binaryen still owns all binary decoding. */
static bool read_memory_text(const char *text, WasmModule *module, Diagnostics *diagnostics)
{
    const char *cursor = text;
    while ((cursor = strstr(cursor, "(memory")) != NULL) {
        const char *p = cursor + 7, *end = strchr(cursor, ')');
        if (end == NULL) { diagnostics_error(diagnostics, "Binaryen produced malformed memory text"); return false; }
        while (p < end && isspace((unsigned char)*p)) ++p;
        if (p < end && *p == '$') while (p < end && !isspace((unsigned char)*p)) ++p;
        while (p < end && isspace((unsigned char)*p)) ++p;
        /* An export contains `(memory $name)` too; only a declaration has a
         * page count after its optional name. */
        if (p >= end || !isdigit((unsigned char)*p)) { cursor = end + 1; continue; }
        ++module->memory_count;
        if (module->memory_count == 1) {
            char *after; unsigned long initial = strtoul(p, &after, 10);
            if (after == p || initial > UINT32_MAX) { diagnostics_error(diagnostics, "could not read Wasm memory initial size"); return false; }
            module->memory_initial_pages = (uint32_t)initial; p = after; while (p < end && isspace((unsigned char)*p)) ++p;
            if (p < end && isdigit((unsigned char)*p)) { unsigned long maximum = strtoul(p, &after, 10); if (maximum > UINT32_MAX) return false; module->memory_has_max = true; module->memory_max_pages = (uint32_t)maximum; }
            module->memory_is_shared = strstr(cursor, "shared") != NULL && strstr(cursor, "shared") < end;
            module->memory_is_64 = strstr(cursor, "i64") != NULL && strstr(cursor, "i64") < end;
        }
        cursor = end + 1;
    }
    module->has_memory = module->memory_count != 0; return true;
}

static bool text_has_memory_import(const char *text)
{
    const char *cursor = text;
    while ((cursor = strstr(cursor, "(import")) != NULL) { const char *end = strchr(cursor, ')'), *memory = strstr(cursor, "(memory"); if (end != NULL && memory != NULL && memory < end) return true; cursor += 7; }
    return false;
}

static bool text_data_offsets_are_const(const char *text, size_t count)
{
    const char *cursor = text; size_t seen = 0;
    while ((cursor = strstr(cursor, "(data")) != NULL) { const char *next = strstr(cursor + 5, "(data"), *constant = strstr(cursor, "(i32.const"); if (constant == NULL || (next != NULL && constant > next)) return false; ++seen; cursor += 5; }
    return seen == count;
}

bool wasm_module_load(const char *path, WasmModule *module, Diagnostics *diagnostics)
{
    char *contents = NULL, *text = NULL; size_t size = 0; BinaryenModuleRef source = NULL; BinaryenIndex index;
    memset(module, 0, sizeof(*module));
    if (!read_file(path, &contents, &size, diagnostics)) return false;
    source = BinaryenModuleReadWithFeatures(contents, size, BinaryenFeatureAll()); free(contents);
    if (source == NULL || !BinaryenModuleValidate(source)) { diagnostics_error(diagnostics, "Binaryen could not load '%s' as a valid Wasm module", path); if (source) BinaryenModuleDispose(source); return false; }
    text = BinaryenModuleAllocateAndWriteText(source);
    if (text == NULL || !read_memory_text(text, module, diagnostics)) goto fail;
    module->has_imported_memory = text_has_memory_import(text); module->table_count = BinaryenGetNumTables(source); module->global_count = BinaryenGetNumGlobals(source); module->element_segment_count = BinaryenGetNumElementSegments(source); module->data_segment_count = BinaryenGetNumDataSegments(source);
    if (module->data_segment_count != 0 && !text_data_offsets_are_const(text, module->data_segment_count)) { diagnostics_error(diagnostics, "active data segments must use constant i32 offsets"); goto fail; }
    free(text); text = NULL;
    module->function_count = BinaryenGetNumFunctions(source); module->functions = calloc(module->function_count, sizeof(*module->functions)); if (module->function_count != 0 && module->functions == NULL) goto fail;
    for (index = 0; index < module->function_count; ++index) {
        BinaryenFunctionRef function = BinaryenGetFunctionByIndex(source, index); WasmFunction *out = &module->functions[index]; const char *import_module;
        out->name = copy_string(BinaryenFunctionGetName(function)); if (out->name == NULL || !convert_tuple_type(BinaryenFunctionGetParams(function), out->params, &out->param_count, diagnostics, out->name ? out->name : "<unnamed>")) goto fail;
        out->result = convert_type(BinaryenFunctionGetResults(function)); if (out->result == WASM_VALUE_OTHER) { diagnostics_error(diagnostics, "function '%s' has an unsupported result type", out->name); goto fail; }
        import_module = BinaryenFunctionImportGetModule(function); out->is_import = import_module != NULL && import_module[0] != '\0';
        if (out->is_import) { out->import_module = copy_string(import_module); out->import_name = copy_string(BinaryenFunctionImportGetBase(function)); if (!out->import_module || !out->import_name) goto fail; continue; }
        out->local_count = BinaryenFunctionGetNumVars(function); out->locals = calloc(out->local_count, sizeof(*out->locals)); if (out->local_count != 0 && out->locals == NULL) goto fail;
        for (BinaryenIndex local = 0; local < out->local_count; ++local) { out->locals[local] = convert_type(BinaryenFunctionGetVar(function, local)); if (out->locals[local] == WASM_VALUE_OTHER) { diagnostics_error(diagnostics, "function '%s' has an unsupported local type", out->name); goto fail; } }
        out->body = convert_expression(BinaryenFunctionGetBody(function), diagnostics, out->name); if (out->body == NULL) goto fail;
    }
    module->export_count = BinaryenGetNumExports(source); module->exports = calloc(module->export_count, sizeof(*module->exports)); if (module->export_count != 0 && module->exports == NULL) goto fail;
    for (index = 0; index < module->export_count; ++index) { BinaryenExportRef export_ref = BinaryenGetExportByIndex(source, index); module->exports[index].name = copy_string(BinaryenExportGetName(export_ref)); module->exports[index].value = copy_string(BinaryenExportGetValue(export_ref)); module->exports[index].is_function = BinaryenExportGetKind(export_ref) == BinaryenExternalFunction(); if (!module->exports[index].name || !module->exports[index].value) goto fail; }
    module->data_segments = calloc(module->data_segment_count, sizeof(*module->data_segments)); if (module->data_segment_count != 0 && module->data_segments == NULL) goto fail;
    for (index = 0; index < module->data_segment_count; ++index) { BinaryenDataSegmentRef segment = BinaryenGetDataSegmentByIndex(source, index); WasmDataSegment *out = &module->data_segments[index]; out->offset = BinaryenGetDataSegmentByteOffset(source, segment); out->size = BinaryenGetDataSegmentByteLength(segment); out->is_passive = BinaryenGetDataSegmentPassive(segment); out->offset_is_i32_const = true; out->bytes = malloc(out->size == 0 ? 1 : out->size); if (out->bytes == NULL) goto fail; BinaryenCopyDataSegmentData(segment, (char *)out->bytes); }
    BinaryenModuleDispose(source); return true;
fail:
    free(text); if (source) BinaryenModuleDispose(source); wasm_module_dispose(module); return false;
}

void wasm_module_dispose(WasmModule *module)
{
    size_t index;
    if (module->functions != NULL)
        for (index = 0; index < module->function_count; ++index) { WasmFunction *f = &module->functions[index]; free(f->name); free(f->import_module); free(f->import_name); free(f->locals); free_expression(f->body); }
    if (module->exports != NULL)
        for (index = 0; index < module->export_count; ++index) { free(module->exports[index].name); free(module->exports[index].value); }
    if (module->data_segments != NULL)
        for (index = 0; index < module->data_segment_count; ++index) free(module->data_segments[index].bytes);
    free(module->functions); free(module->exports); free(module->data_segments); memset(module, 0, sizeof(*module));
}

const WasmFunction *wasm_module_find_function(const WasmModule *module, const char *name)
{
    size_t index; for (index = 0; index < module->function_count; ++index) if (strcmp(module->functions[index].name, name) == 0) return &module->functions[index]; return NULL;
}
