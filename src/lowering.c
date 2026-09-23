/*
 * VirconWasm-to-V32-IR lowering.
 *
 * This file is the frontend boundary: it converts validated compiler-owned
 * Wasm expressions into assembler-oriented V32 IR lines. Binaryen objects do
 * not reach this stage, and Wasm byte-memory semantics are legalized here.
 */

#include "lowering.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target_layout.h"

#define LINEAR_BASE VIRCON_LINEAR_MEMORY_BASE
#define TEMP_SLOTS 48u
#define OUTGOING_SLOTS 4u

/* A temporary stack-frame slot containing one Wasm i32 or f32 value. */
typedef struct Value {
  int slot;
  bool present;
} Value;
/* A structured-control target mapped from a Wasm label to an assembly label. */
typedef struct Target {
  const char *wasm_name;
  char *label;
} Target;
/* Per-function lowering state, including structured targets and temp slots. */
typedef struct Context {
  const ValidatedModule *validated;
  const WasmFunction *function;
  VirconIrProgram *program;
  Diagnostics *diagnostics;
  Target targets[32];
  size_t target_count;
  unsigned next_label, temp_depth;
  char return_label[64];
  uint32_t memory_bytes;
} Context;

/* Allocates an exact-size formatted V32 IR line. */
static char *format_text(const char *format, ...) {
  va_list args, copy;
  int length;
  char *text;
  va_start(args, format);
  va_copy(copy, args);
  length = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (length < 0) {
    va_end(args);
    return NULL;
  }
  text = malloc((size_t)length + 1);
  if (text != NULL)
    vsnprintf(text, (size_t)length + 1, format, args);
  va_end(args);
  return text;
}
/* Formats and appends one Vircon32 assembly line to the current V32 program. */
static bool emit(Context *context, const char *format, ...) {
  va_list args;
  char buffer[256];
  int length;
  char *text;
  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (length < 0)
    return false;
  text = length < (int)sizeof(buffer) ? format_text("%s", buffer) : NULL;
  if (text == NULL) {
    diagnostics_error(context->diagnostics, "out of memory building V32 IR");
    return false;
  }
  return vircon_ir_append_text(context->program, text, context->diagnostics);
}
/* Allocates a function-unique internal label for generated control flow. */
static bool fresh_label(Context *context, const char *kind, char *out,
                        size_t size) {
  return snprintf(out, size, "__wasm_%s_%zu_%u", kind,
                  (size_t)(context->function -
                           context->validated->module->functions),
                  context->next_label++) > 0;
}
/* Appends one assembly label definition. */
static bool emit_label(Context *context, const char *label) {
  return emit(context, "%s:", label);
}

/* Counts target words reserved for Wasm locals; restricted i64 locals use two.
 */
static size_t local_storage_words(const WasmFunction *function) {
  size_t index, words = 0;
  for (index = 0; index < function->local_count; ++index)
    words += function->locals[index] == WASM_VALUE_I64 ? 2u : 1u;
  return words;
}

/* Reserves one compiler-managed word below the current function's locals. */
static int temp_slot(Context *context) {
  if (context->temp_depth == TEMP_SLOTS) {
    diagnostics_error(
        context->diagnostics,
        "expression nesting exceeds VirconWasm v1 temporary-slot limit");
    return 0;
  }
  ++context->temp_depth;
  return -(int)(local_storage_words(context->function) + context->temp_depth);
}
/* Releases the most recently reserved temporary when value owns one. */
static void release(Context *context, Value value) {
  if (value.present && context->temp_depth != 0)
    --context->temp_depth;
}
/* Loads a frame slot into a target register. */
static bool load_slot(Context *context, int reg, int slot) {
  return emit(context, "  mov R%d, [BP%+d]", reg, slot);
}
/* Stores a target register into a frame slot. */
static bool store_slot(Context *context, int slot, int reg) {
  return emit(context, "  mov [BP%+d], R%d", slot, reg);
}
/* Maps a Wasm parameter/local index to its compiler-defined frame slot. */
static int local_slot(const WasmFunction *function, uint32_t index) {
  uint32_t local;
  int slot = -1;
  if (index < function->param_count)
    return (int)(2 + index);
  local = index - (uint32_t)function->param_count;
  for (uint32_t cursor = 0; cursor < local; ++cursor)
    slot -= function->locals[cursor] == WASM_VALUE_I64 ? 2 : 1;
  return slot;
}

/* Returns the high target word of a validated restricted i64 local. */
static int i64_local_high_slot(const WasmFunction *function, uint32_t index) {
  return local_slot(function, index) - 1;
}
/* Converts a function pointer within module storage to its Wasm index. */
static size_t function_index(const WasmModule *module,
                             const WasmFunction *function) {
  return (size_t)(function - module->functions);
}
/* Produces the generated assembly label for one defined Wasm function. */
static void function_label(const WasmModule *module,
                           const WasmFunction *function, char *out,
                           size_t size) {
  snprintf(out, size, "__wasm_function_%zu", function_index(module, function));
}

static bool lower_expression(Context *context, const WasmExpr *expression,
                             Value *value);

/* Checks a Wasm byte address and leaves its effective byte address in R2. */
static bool effective_address(Context *context, Value pointer, uint32_t offset,
                              uint32_t width) {
  uint32_t maximum;
  if ((uint64_t)offset + width > context->memory_bytes)
    return emit(context, "  jmp __wasm_trap");
  maximum = context->memory_bytes - offset - width;
  return load_slot(context, 2, pointer.slot) && emit(context, "  mov R1, R2") &&
         emit(context, "  ilt R1, 0") &&
         emit(context, "  jt R1, __wasm_trap") &&
         emit(context, "  mov R1, R2") &&
         emit(context, "  igt R1, 0x%08X", maximum) &&
         emit(context, "  jt R1, __wasm_trap") &&
         (offset == 0 || emit(context, "  iadd R2, 0x%08X", offset));
}
/* Extracts one little-endian Wasm byte at the checked byte address in R2. */
static bool load_byte_at_r2(Context *context, int result) {
  return emit(context, "  mov R3, R2") && emit(context, "  and R3, 3") &&
         emit(context, "  imul R3, -8") && emit(context, "  mov R4, R2") &&
         emit(context, "  mov R5, -2") && emit(context, "  shl R4, R5") &&
         emit(context, "  iadd R4, %u", LINEAR_BASE) &&
         emit(context, "  mov R%d, [R4]", result) &&
         emit(context, "  shl R%d, R3", result) &&
         emit(context, "  and R%d, 0x000000FF", result);
}
/* Replaces one little-endian Wasm byte at the checked byte address in R2. */
static bool store_byte_at_r2(Context *context, int value_register) {
  /* Vircon32 BNOT is logical-not, not a bitwise complement. XOR builds the
   * inverted lane mask required for Wasm's preserving read-modify-write. */
  return emit(context, "  mov R3, R2") && emit(context, "  and R3, 3") &&
         emit(context, "  imul R3, 8") && emit(context, "  mov R4, R2") &&
         emit(context, "  mov R5, -2") && emit(context, "  shl R4, R5") &&
         emit(context, "  iadd R4, %u", LINEAR_BASE) &&
         emit(context, "  mov R5, [R4]") &&
         emit(context, "  mov R6, 0x000000FF") &&
         emit(context, "  shl R6, R3") &&
         emit(context, "  xor R6, 0xFFFFFFFF") &&
         emit(context, "  and R5, R6") &&
         emit(context, "  mov R6, R%d", value_register) &&
         emit(context, "  and R6, 0x000000FF") &&
         emit(context, "  shl R6, R3") && emit(context, "  or R5, R6") &&
         emit(context, "  mov [R4], R5");
}

/* Loads an i32 at the already checked Wasm byte address in R2. */
static bool load_i32_at_r2(Context *context, int result_register) {
  char aligned[64], done[64];
  if (!fresh_label(context, "load_aligned", aligned, sizeof(aligned)) ||
      !fresh_label(context, "load_done", done, sizeof(done)) ||
      !emit(context, "  mov R1, R2") || !emit(context, "  and R1, 3") ||
      !emit(context, "  jf R1, %s", aligned) || !emit(context, "  mov R6, 0"))
    return false;
  /* Unaligned accesses reconstruct the word from packed byte lanes. */
  for (unsigned byte = 0; byte < 4; ++byte) {
    if (!load_byte_at_r2(context, 5) ||
        (byte != 0 && !emit(context, "  mov R3, %u", byte * 8)) ||
        (byte != 0 && !emit(context, "  shl R5, R3")) ||
        !emit(context, "  or R6, R5") ||
        (byte != 3 && !emit(context, "  iadd R2, 1")))
      return false;
  }
  return emit(context, "  mov R%d, R6", result_register) &&
         emit(context, "  jmp %s", done) && emit_label(context, aligned) &&
         emit(context, "  mov R3, R2") && emit(context, "  mov R4, -2") &&
         emit(context, "  shl R3, R4") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R%d, [R3]", result_register) &&
         emit_label(context, done);
}

/* Stores an i32 at the already checked Wasm byte address in R2. */
static bool store_i32_at_r2(Context *context, int value_register) {
  char aligned[64], done[64];
  if (!fresh_label(context, "store_aligned", aligned, sizeof(aligned)) ||
      !fresh_label(context, "store_done", done, sizeof(done)) ||
      !emit(context, "  mov R7, R2") || !emit(context, "  and R7, 3") ||
      !emit(context, "  jf R7, %s", aligned))
    return false;
  /* Unaligned accesses split the word into preserving byte stores. */
  for (unsigned byte = 0; byte < 4; ++byte) {
    /* store_byte_at_r2 uses R3-R6 internally, so R7 preserves this byte. */
    if (!emit(context, "  mov R7, R%d", value_register) ||
        (byte != 0 && !emit(context, "  mov R3, -%u", byte * 8)) ||
        (byte != 0 && !emit(context, "  shl R7, R3")) ||
        !store_byte_at_r2(context, 7) ||
        (byte != 3 && !emit(context, "  iadd R2, 1")))
      return false;
  }
  return emit(context, "  jmp %s", done) && emit_label(context, aligned) &&
         emit(context, "  mov R3, R2") && emit(context, "  mov R4, -2") &&
         emit(context, "  shl R3, R4") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov [R3], R%d", value_register) &&
         emit_label(context, done);
}

/* Lowers a validated byte or i32 Wasm load into packed-memory operations. */
static bool lower_load(Context *context, const WasmExpr *expression,
                       Value *value) {
  Value pointer = {0};
  int slot;
  if (!lower_expression(context, expression->children[0], &pointer) ||
      !pointer.present ||
      !effective_address(context, pointer, expression->offset,
                         expression->bytes))
    return false;
  if (expression->bytes == 1) {
    if (!load_byte_at_r2(context, 1) || !store_slot(context, pointer.slot, 1))
      return false;
    *value = pointer;
    return true;
  }
  if (!load_i32_at_r2(context, 1))
    return false;
  slot = pointer.slot;
  if (!store_slot(context, slot, 1))
    return false;
  *value = pointer;
  return true;
}
/* Lowers a validated byte or i32 Wasm store into packed-memory operations. */
static bool lower_store(Context *context, const WasmExpr *expression,
                        Value *value) {
  Value pointer = {0}, input = {0};
  if (!lower_expression(context, expression->children[0], &pointer) ||
      !lower_expression(context, expression->children[1], &input) ||
      !pointer.present || !input.present ||
      !effective_address(context, pointer, expression->offset,
                         expression->bytes) ||
      !load_slot(context, 1, input.slot))
    return false;
  if (expression->bytes == 1) {
    if (!store_byte_at_r2(context, 1))
      return false;
    release(context, input);
    release(context, pointer);
    value->present = false;
    return true;
  }
  if (!store_i32_at_r2(context, 1))
    return false;
  release(context, input);
  release(context, pointer);
  value->present = false;
  return true;
}
/* Emits the validated constant, aligned i64-store form as two i32 stores. */
static bool lower_i64_const_store(Context *context, const WasmExpr *expression,
                                  Value *value) {
  uint64_t address = (uint64_t)(uint32_t)expression->children[0]->i32_value +
                     expression->offset;
  uint32_t low = (uint32_t)expression->i64_value;
  uint32_t high = (uint32_t)(expression->i64_value >> 32);
  unsigned word = LINEAR_BASE + (unsigned)(address >> 2);

  /* Validation has already proved the full eight-byte range and the
   * four-byte alignment. Store low then high to preserve Wasm little-endian
   * layout without introducing an i64 target value or arithmetic path. */
  if (!emit(context, "  mov R1, 0x%08X", low) ||
      !emit(context, "  mov [%u], R1", word) ||
      !emit(context, "  mov R1, 0x%08X", high) ||
      !emit(context, "  mov [%u], R1", word + 1))
    return false;
  value->present = false;
  return true;
}

/* Computes the low i32 word after a validated constant logical i64 shift. */
static bool extract_i64_word(Context *context, int low_slot, int high_slot,
                             uint64_t shift) {
  if (shift == 0)
    return load_slot(context, 1, low_slot);
  if (shift < 32) {
    return load_slot(context, 1, low_slot) &&
           emit(context, "  mov R2, -%u", (unsigned)shift) &&
           emit(context, "  shl R1, R2") && load_slot(context, 3, high_slot) &&
           emit(context, "  mov R2, %u", 32u - (unsigned)shift) &&
           emit(context, "  shl R3, R2") && emit(context, "  or R1, R3");
  }
  if (shift == 32)
    return load_slot(context, 1, high_slot);
  return load_slot(context, 1, high_slot) &&
         emit(context, "  mov R2, -%u", (unsigned)(shift - 32)) &&
         emit(context, "  shl R1, R2");
}

/*
 * Lowers the only dynamic i64 form accepted by this profile: an i64.load used
 * directly as an i64.store value. The two temporary slots hold the loaded low
 * and high i32 words before either destination write, preserving Wasm's
 * value-then-store behavior even when the eight-byte ranges overlap.
 */
static bool lower_i64_load_store(Context *context, const WasmExpr *expression,
                                 Value *value) {
  Value destination = {0}, source = {0}, high_word = {0},
        destination_address = {0};

  /* A Wasm store evaluates its destination address before its value. */
  if (!lower_expression(context, expression->children[0], &destination) ||
      !lower_expression(context, expression->children[1], &source) ||
      !destination.present || !source.present ||
      !effective_address(context, source, expression->source_offset, 8))
    return false;

  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1) || !store_slot(context, source.slot, 1) ||
      !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1) ||
      !store_slot(context, high_word.slot, 1))
    return false;
  high_word.present = true;

  if (!effective_address(context, destination, expression->offset, 8))
    return false;
  destination_address.slot = temp_slot(context);
  if (destination_address.slot == 0 ||
      !store_slot(context, destination_address.slot, 2) ||
      !load_slot(context, 2, destination_address.slot) ||
      !load_slot(context, 1, source.slot) || !store_i32_at_r2(context, 1) ||
      !load_slot(context, 2, destination_address.slot) ||
      !emit(context, "  iadd R2, 4") ||
      !load_slot(context, 1, high_word.slot) || !store_i32_at_r2(context, 1))
    return false;

  destination_address.present = true;
  release(context, destination_address);
  release(context, high_word);
  release(context, source);
  release(context, destination);
  value->present = false;
  return true;
}

/* Lowers Zig's local.tee(i64.load) aggregate copy into a two-word local. */
static bool lower_i64_load_store_local_tee(Context *context,
                                           const WasmExpr *expression,
                                           Value *value) {
  Value destination = {0}, source = {0}, high_word = {0},
        destination_address = {0};
  int local_low = local_slot(context->function, expression->index);
  int local_high = i64_local_high_slot(context->function, expression->index);

  if (!lower_expression(context, expression->children[0], &destination) ||
      !lower_expression(context, expression->children[1], &source) ||
      !destination.present || !source.present ||
      !effective_address(context, source, expression->source_offset, 8))
    return false;
  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1) || !store_slot(context, source.slot, 1) ||
      !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1) ||
      !store_slot(context, high_word.slot, 1))
    return false;
  high_word.present = true;

  /* local.tee writes the pair before the enclosing i64.store observes it. */
  if (!load_slot(context, 1, source.slot) ||
      !store_slot(context, local_low, 1) ||
      !load_slot(context, 1, high_word.slot) ||
      !store_slot(context, local_high, 1) ||
      !effective_address(context, destination, expression->offset, 8))
    return false;
  destination_address.slot = temp_slot(context);
  if (destination_address.slot == 0 ||
      !store_slot(context, destination_address.slot, 2) ||
      !load_slot(context, 2, destination_address.slot) ||
      !load_slot(context, 1, source.slot) || !store_i32_at_r2(context, 1) ||
      !load_slot(context, 2, destination_address.slot) ||
      !emit(context, "  iadd R2, 4") ||
      !load_slot(context, 1, high_word.slot) || !store_i32_at_r2(context, 1))
    return false;
  destination_address.present = true;
  release(context, destination_address);
  release(context, high_word);
  release(context, source);
  release(context, destination);
  value->present = false;
  return true;
}

/* Lowers Zig's exact computed two-word packing form directly into i32 stores.
 */
static bool lower_i64_packed_i32_store(Context *context,
                                       const WasmExpr *expression,
                                       Value *value) {
  Value destination = {0}, high_word = {0}, low_word = {0};

  /* Wasm evaluates the store address, high expression, then low expression. */
  if (!lower_expression(context, expression->children[0], &destination) ||
      !lower_expression(context, expression->children[1], &high_word) ||
      !lower_expression(context, expression->children[2], &low_word) ||
      !destination.present || !high_word.present || !low_word.present ||
      !effective_address(context, destination, expression->offset, 8) ||
      !load_slot(context, 1, low_word.slot) || !store_i32_at_r2(context, 1) ||
      !load_slot(context, 2, destination.slot) ||
      !emit(context, "  iadd R2, 4") ||
      !load_slot(context, 1, high_word.slot) || !store_i32_at_r2(context, 1))
    return false;

  release(context, low_word);
  release(context, high_word);
  release(context, destination);
  value->present = false;
  return true;
}

/*
 * Lowers i32.wrap_i64 of either an i64.load or an i64.load shifted right by a
 * constant. The two loaded words stay frontend-local; the result is one
 * ordinary i32 value and no general i64 register or ABI value exists.
 */
static bool lower_i64_word_extract(Context *context, const WasmExpr *expression,
                                   Value *value) {
  Value low_word = {0}, high_word = {0};
  uint64_t shift = expression->i64_value;

  if (!lower_expression(context, expression->children[0], &low_word) ||
      !low_word.present ||
      !effective_address(context, low_word, expression->source_offset, 8))
    return false;
  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1) || !store_slot(context, low_word.slot, 1) ||
      !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1) ||
      !store_slot(context, high_word.slot, 1))
    return false;
  high_word.present = true;

  if (!extract_i64_word(context, low_word.slot, high_word.slot, shift))
    return false;
  if (!store_slot(context, low_word.slot, 1))
    return false;
  release(context, high_word);
  *value = low_word;
  return true;
}

/* Reads one i32 word from the validated compiler-owned two-word i64 local. */
static bool lower_i64_local_word_extract(Context *context,
                                         const WasmExpr *expression,
                                         Value *value) {
  int result_slot = temp_slot(context);
  int local_low = local_slot(context->function, expression->index);
  int local_high = i64_local_high_slot(context->function, expression->index);
  if (result_slot == 0 ||
      !extract_i64_word(context, local_low, local_high,
                        expression->i64_value) ||
      !store_slot(context, result_slot, 1))
    return false;
  value->slot = result_slot;
  value->present = true;
  return true;
}

/* Loads a pair into a restricted i64 local and returns one extracted i32 word.
 */
static bool lower_i64_local_tee_word_extract(Context *context,
                                             const WasmExpr *expression,
                                             Value *value) {
  Value pointer = {0}, high_word = {0};
  int result_slot = temp_slot(context);
  int local_low = local_slot(context->function, expression->index);
  int local_high = i64_local_high_slot(context->function, expression->index);

  if (result_slot == 0 ||
      !lower_expression(context, expression->children[0], &pointer) ||
      !pointer.present ||
      !effective_address(context, pointer, expression->source_offset, 8))
    return false;
  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1) || !store_slot(context, local_low, 1) ||
      !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1) ||
      !store_slot(context, local_high, 1) ||
      !extract_i64_word(context, local_low, local_high,
                        expression->i64_value) ||
      !store_slot(context, result_slot, 1))
    return false;

  high_word.present = true;
  release(context, high_word);
  release(context, pointer);
  value->slot = result_slot;
  value->present = true;
  return true;
}

/* Lowers default-memory bulk operations through compiler-generated V32 helpers.
 */
static bool lower_bulk_memory(Context *context, const WasmExpr *expression,
                              Value *value) {
  Value arguments[3] = {{0}};
  size_t index;
  const char *helper = expression->kind == WASM_EXPR_MEMORY_COPY
                           ? "__wasm_memory_copy"
                           : "__wasm_memory_fill";

  /* Wasm evaluates destination, source/value, then length exactly once. */
  for (index = 0; index < 3; ++index)
    if (!lower_expression(context, expression->children[index],
                          &arguments[index]) ||
        !arguments[index].present)
      return false;
  for (index = 0; index < 3; ++index)
    if (!load_slot(context, 1, arguments[index].slot) ||
        !emit(context, "  mov [SP+%zu], R1", index))
      return false;
  for (index = 3; index != 0; --index)
    release(context, arguments[index - 1]);
  if (!emit(context, "  call %s", helper))
    return false;
  value->present = false;
  return true;
}

/* Emit Wasm's arithmetic right shift. Vircon32 only has a logical right shift
 * through SHL with a negative count, so negative inputs need an explicit mask.
 */
static bool emit_i32_shr_s(Context *context) {
  char logical[64], done[64];
  if (!fresh_label(context, "shr_s_logical", logical, sizeof(logical)) ||
      !fresh_label(context, "shr_s_done", done, sizeof(done)))
    return false;

  return emit(context, "  and R2, 31") && emit(context, "  jf R2, %s", done) &&
         emit(context, "  mov R3, R1") && emit(context, "  ilt R3, 0") &&
         emit(context, "  jf R3, %s", logical) &&
         emit(context, "  mov R3, 0") && emit(context, "  isub R3, R2") &&
         emit(context, "  shl R1, R3") && emit(context, "  mov R4, 32") &&
         emit(context, "  isub R4, R2") &&
         emit(context, "  mov R5, 0xFFFFFFFF") &&
         emit(context, "  shl R5, R4") && emit(context, "  or R1, R5") &&
         emit(context, "  jmp %s", done) && emit_label(context, logical) &&
         emit(context, "  mov R3, 0") && emit(context, "  isub R3, R2") &&
         emit(context, "  shl R1, R3") && emit_label(context, done);
}

/* Emit a correctly rounded conversion from an unsigned Wasm i32 to f32.
 * CIF accepts only signed values. For the upper unsigned half, shifting once
 * and retaining bit zero as a sticky bit preserves round-to-nearest-even when
 * the converted half is doubled. */
static bool emit_f32_convert_i32_u(Context *context) {
  char signed_input[64], done[64];
  if (!fresh_label(context, "u32_to_f32_signed", signed_input,
                   sizeof(signed_input)) ||
      !fresh_label(context, "u32_to_f32_done", done, sizeof(done)))
    return false;

  return emit(context, "  mov R2, R1") && emit(context, "  ilt R2, 0") &&
         emit(context, "  jf R2, %s", signed_input) &&
         emit(context, "  mov R2, R1") && emit(context, "  and R2, 1") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R4, -1") &&
         emit(context, "  shl R3, R4") && emit(context, "  or R3, R2") &&
         emit(context, "  cif R3") && emit(context, "  fadd R3, R3") &&
         emit(context, "  mov R1, R3") && emit(context, "  jmp %s", done) &&
         emit_label(context, signed_input) && emit(context, "  cif R1") &&
         emit_label(context, done);
}

/* Reads one hardware port into the normal single-word compiler value model. */
static bool lower_port_read(Context *context, Value *value, const char *port) {
  int slot = temp_slot(context);
  if (slot == 0 || !emit(context, "  in R0, %s", port) ||
      !store_slot(context, slot, 0))
    return false;
  value->slot = slot;
  value->present = true;
  return true;
}

/* Lowers a unary CPU math instruction through the generic value-slot model. */
static bool lower_cpu_unary(Context *context, const Value *argument,
                            Value *value, const char *instruction) {
  int slot = temp_slot(context);
  if (slot == 0 || !load_slot(context, 0, argument->slot) ||
      !emit(context, "  %s R0", instruction) || !store_slot(context, slot, 0))
    return false;
  value->slot = slot;
  value->present = true;
  return true;
}

/* Lowers a binary CPU math instruction through the generic value-slot model. */
static bool lower_cpu_binary(Context *context, const Value *left,
                             const Value *right, Value *value,
                             const char *instruction) {
  int slot = temp_slot(context);
  if (slot == 0 || !load_slot(context, 0, left->slot) ||
      !load_slot(context, 1, right->slot) ||
      !emit(context, "  %s R0, R1", instruction) ||
      !store_slot(context, slot, 0))
    return false;
  value->slot = slot;
  value->present = true;
  return true;
}

/* Evaluates arguments, then lowers either a platform import or direct call. */
static bool lower_call(Context *context, const WasmExpr *expression,
                       Value *value) {
  const WasmFunction *callee =
      wasm_module_find_function(context->validated->module, expression->name);
  Value arguments[4] = {{0}};
  char label[64];
  size_t index;
  for (index = 0; index < expression->child_count; ++index)
    if (!lower_expression(context, expression->children[index],
                          &arguments[index]) ||
        !arguments[index].present)
      return false;
  if (callee->is_import) {
    if (strcmp(callee->import_name, "vircon_set_background_color") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out GPU_ClearColor, R1") ||
          !emit(context, "  out GPU_Command, GPUCommand_ClearScreen"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_end_frame") == 0) {
      if (!emit(context, "  wait"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_selected_texture") ==
               0) {
      int slot = temp_slot(context);
      if (slot == 0 || !emit(context, "  in R0, GPU_SelectedTexture") ||
          !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_gpu_select_texture") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out GPU_SelectedTexture, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_selected_region") ==
               0) {
      int slot = temp_slot(context);
      if (slot == 0 || !emit(context, "  in R0, GPU_SelectedRegion") ||
          !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_gpu_select_region") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out GPU_SelectedRegion, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_point") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  out GPU_DrawingPointX, R1") ||
          !emit(context, "  out GPU_DrawingPointY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region") == 0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegion"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_region_minimum") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  out GPU_RegionMinX, R1") ||
          !emit(context, "  out GPU_RegionMinY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_region_maximum") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  out GPU_RegionMaxX, R1") ||
          !emit(context, "  out GPU_RegionMaxY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_region_hotspot") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  out GPU_RegionHotSpotX, R1") ||
          !emit(context, "  out GPU_RegionHotSpotY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_select_channel") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_SelectedChannel, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_select_sound") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_SelectedSound, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_get_selected_sound") ==
               0) {
      return lower_port_read(context, value, "SPU_SelectedSound");
    } else if (strcmp(callee->import_name, "vircon_spu_get_selected_channel") ==
               0) {
      return lower_port_read(context, value, "SPU_SelectedChannel");
    } else if (strcmp(callee->import_name,
                      "vircon_spu_set_sound_play_with_loop") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_SoundPlayWithLoop, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_sound_loop_start") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_SoundLoopStart, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_sound_loop_end") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_SoundLoopEnd, R1"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_spu_set_channel_assigned_sound") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_ChannelAssignedSound, R1"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_spu_play_selected_channel") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_PlaySelectedChannel"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_spu_pause_selected_channel") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_PauseSelectedChannel"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_spu_stop_selected_channel") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_StopSelectedChannel"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_volume") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_ChannelVolume, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_speed") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_ChannelSpeed, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_position") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_ChannelPosition, R1"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_spu_set_channel_loop_enabled") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_ChannelLoopEnabled, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_global_volume") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out SPU_GlobalVolume, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_get_channel_speed") ==
               0) {
      return lower_port_read(context, value, "SPU_ChannelSpeed");
    } else if (strcmp(callee->import_name, "vircon_spu_get_channel_position") ==
               0) {
      return lower_port_read(context, value, "SPU_ChannelPosition");
    } else if (strcmp(callee->import_name, "vircon_spu_get_global_volume") ==
               0) {
      return lower_port_read(context, value, "SPU_GlobalVolume");
    } else if (strcmp(callee->import_name, "vircon_spu_pause_all_channels") ==
               0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_PauseAllChannels"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_stop_all_channels") ==
               0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_StopAllChannels"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_resume_all_channels") ==
               0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_ResumeAllChannels"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_rng_set_current_value") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out RNG_CurrentValue, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_memcard_read_word") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  iadd R1, 0x30000000") ||
          !emit(context, "  mov R0, [R1]") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_memcard_write_word") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  iadd R1, 0x30000000") ||
          !emit(context, "  mov [R1], R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_multiply_color") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out GPU_MultiplyColor, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_multiply_color") ==
               0) {
      return lower_port_read(context, value, "GPU_MultiplyColor");
    } else if (strcmp(callee->import_name, "vircon_gpu_set_active_blending") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out GPU_ActiveBlending, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_active_blending") ==
               0) {
      return lower_port_read(context, value, "GPU_ActiveBlending");
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_point_x") ==
               0) {
      return lower_port_read(context, value, "GPU_DrawingPointX");
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_point_y") ==
               0) {
      return lower_port_read(context, value, "GPU_DrawingPointY");
    } else if (strcmp(callee->import_name,
                      "vircon_gpu_set_drawing_scale_bits") == 0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  out GPU_DrawingScaleX, R1") ||
          !emit(context, "  out GPU_DrawingScaleY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_scale") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !load_slot(context, 2, arguments[1].slot) ||
          !emit(context, "  out GPU_DrawingScaleX, R1") ||
          !emit(context, "  out GPU_DrawingScaleY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_scale_x") ==
               0) {
      return lower_port_read(context, value, "GPU_DrawingScaleX");
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_scale_y") ==
               0) {
      return lower_port_read(context, value, "GPU_DrawingScaleY");
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region_zoomed") ==
               0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionZoomed"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_angle") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out GPU_DrawingAngle, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_angle") ==
               0) {
      return lower_port_read(context, value, "GPU_DrawingAngle");
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region_rotated") ==
               0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionRotated"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_gpu_draw_region_rotozoomed") == 0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionRotozoomed"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_sin") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !load_slot(context, 0, arguments[0].slot) ||
          !emit(context, "  sin R0") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_cpu_acos") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !load_slot(context, 0, arguments[0].slot) ||
          !emit(context, "  acos R0") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_cpu_log") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !load_slot(context, 0, arguments[0].slot) ||
          !emit(context, "  log R0") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_cpu_pow") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !load_slot(context, 0, arguments[0].slot) ||
          !load_slot(context, 1, arguments[1].slot) ||
          !emit(context, "  pow R0, R1") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_cpu_fmod") == 0) {
      if (!lower_cpu_binary(context, &arguments[0], &arguments[1], value,
                            "fmod"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_imin") == 0) {
      if (!lower_cpu_binary(context, &arguments[0], &arguments[1], value,
                            "imin"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_imax") == 0) {
      if (!lower_cpu_binary(context, &arguments[0], &arguments[1], value,
                            "imax"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_iabs") == 0) {
      if (!lower_cpu_unary(context, &arguments[0], value, "iabs"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_fmin") == 0) {
      if (!lower_cpu_binary(context, &arguments[0], &arguments[1], value,
                            "fmin"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_fmax") == 0) {
      if (!lower_cpu_binary(context, &arguments[0], &arguments[1], value,
                            "fmax"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_fabs") == 0) {
      if (!lower_cpu_unary(context, &arguments[0], value, "fabs"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_floor") == 0) {
      if (!lower_cpu_unary(context, &arguments[0], value, "flr"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_ceil") == 0) {
      if (!lower_cpu_unary(context, &arguments[0], value, "ceil"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_round") == 0) {
      if (!lower_cpu_unary(context, &arguments[0], value, "round"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_atan2") == 0) {
      if (!lower_cpu_binary(context, &arguments[0], &arguments[1], value,
                            "atan2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_halt") == 0) {
      if (!emit(context, "  hlt"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_input_select_gamepad") ==
               0) {
      if (!load_slot(context, 1, arguments[0].slot) ||
          !emit(context, "  out INP_SelectedGamepad, R1"))
        return false;
    } else if (strcmp(callee->import_name,
                      "vircon_input_get_selected_gamepad") == 0) {
      return lower_port_read(context, value, "INP_SelectedGamepad");
    } else if (strcmp(callee->import_name, "vircon_timer_get_cycle_counter") ==
               0) {
      return lower_port_read(context, value, "TIM_CycleCounter");
    } else if (strcmp(callee->import_name, "vircon_input_gamepad_left") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_right") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_up") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_down") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_connected") ==
                   0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_a") ==
                   0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_b") ==
                   0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_x") ==
                   0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_y") ==
                   0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_l") ==
                   0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_r") ==
                   0 ||
               strcmp(callee->import_name,
                      "vircon_input_gamepad_button_start") == 0 ||
               strcmp(callee->import_name, "vircon_timer_get_frame_counter") ==
                   0 ||
               strcmp(callee->import_name, "vircon_timer_get_current_time") ==
                   0 ||
               strcmp(callee->import_name, "vircon_timer_get_current_date") ==
                   0 ||
               strcmp(callee->import_name, "vircon_rng_get_current_value") ==
                   0 ||
               strcmp(callee->import_name, "vircon_spu_get_channel_state") ==
                   0 ||
               strcmp(callee->import_name, "vircon_memcard_is_connected") ==
                   0) {
      const char *port =
          strcmp(callee->import_name, "vircon_input_gamepad_left") == 0
              ? "INP_GamepadLeft"
          : strcmp(callee->import_name, "vircon_input_gamepad_right") == 0
              ? "INP_GamepadRight"
          : strcmp(callee->import_name, "vircon_input_gamepad_up") == 0
              ? "INP_GamepadUp"
          : strcmp(callee->import_name, "vircon_input_gamepad_down") == 0
              ? "INP_GamepadDown"
          : strcmp(callee->import_name, "vircon_input_gamepad_connected") == 0
              ? "INP_GamepadConnected"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_a") == 0
              ? "INP_GamepadButtonA"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_b") == 0
              ? "INP_GamepadButtonB"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_x") == 0
              ? "INP_GamepadButtonX"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_y") == 0
              ? "INP_GamepadButtonY"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_l") == 0
              ? "INP_GamepadButtonL"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_r") == 0
              ? "INP_GamepadButtonR"
          : strcmp(callee->import_name, "vircon_input_gamepad_button_start") ==
                  0
              ? "INP_GamepadButtonStart"
          : strcmp(callee->import_name, "vircon_timer_get_current_time") == 0
              ? "TIM_CurrentTime"
          : strcmp(callee->import_name, "vircon_timer_get_current_date") == 0
              ? "TIM_CurrentDate"
          : strcmp(callee->import_name, "vircon_rng_get_current_value") == 0
              ? "RNG_CurrentValue"
          : strcmp(callee->import_name, "vircon_memcard_is_connected") == 0
              ? "MEM_Connected"
          : strcmp(callee->import_name, "vircon_spu_get_channel_state") == 0
              ? "SPU_ChannelState"
              : "TIM_FrameCounter";
      int slot = temp_slot(context);
      if (slot == 0 || !emit(context, "  in R0, %s", port) ||
          !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else {
      diagnostics_error(context->diagnostics,
                        "internal error: unsupported import passed validation");
      return false;
    }
    for (index = expression->child_count; index != 0; --index)
      release(context, arguments[index - 1]);
    value->present = false;
    return true;
  }
  for (index = 0; index < expression->child_count; ++index)
    if (!load_slot(context, 1, arguments[index].slot) ||
        !emit(context, "  mov [SP+%zu], R1", index))
      return false;
  for (index = expression->child_count; index != 0; --index)
    release(context, arguments[index - 1]);
  function_label(context->validated->module, callee, label, sizeof(label));
  if (!emit(context, "  call %s", label))
    return false;
  if (callee->result == WASM_VALUE_I32 || callee->result == WASM_VALUE_F32) {
    int slot = temp_slot(context);
    if (slot == 0 || !store_slot(context, slot, 0))
      return false;
    value->slot = slot;
    value->present = true;
  } else
    value->present = false;
  return true;
}

/* Lowers one supported typed binary operation through the value-slot model. */
static bool lower_binary(Context *context, const WasmExpr *expression,
                         Value *value) {
  Value left = {0}, right = {0};
  char normal[64], done[64];
  if (!lower_expression(context, expression->children[0], &left) ||
      !lower_expression(context, expression->children[1], &right) ||
      !left.present || !right.present || !load_slot(context, 1, left.slot) ||
      !load_slot(context, 2, right.slot))
    return false;

  switch (expression->binary_op) {
  case WASM_BINARY_ADD:
    if (!emit(context, "  iadd R1, R2"))
      return false;
    break;
  case WASM_BINARY_SUB:
    if (!emit(context, "  isub R1, R2"))
      return false;
    break;
  case WASM_BINARY_MUL:
    if (!emit(context, "  imul R1, R2"))
      return false;
    break;
  case WASM_BINARY_XOR:
    if (!emit(context, "  xor R1, R2"))
      return false;
    break;
  case WASM_BINARY_DIV_S:
    /* Wasm traps for a zero divisor and for INT32_MIN / -1. Vircon IDIV
     * checks only zero, so guard the second case before issuing it. */
    if (!fresh_label(context, "div_s_normal", normal, sizeof(normal)) ||
        !emit(context, "  mov R3, R2") || !emit(context, "  ieq R3, 0") ||
        !emit(context, "  jt R3, __wasm_trap") ||
        !emit(context, "  mov R3, R1") ||
        !emit(context, "  ieq R3, 0x80000000") ||
        !emit(context, "  jf R3, %s", normal) ||
        !emit(context, "  mov R3, R2") || !emit(context, "  ieq R3, -1") ||
        !emit(context, "  jt R3, __wasm_trap") ||
        !emit_label(context, normal) || !emit(context, "  idiv R1, R2"))
      return false;
    break;
  case WASM_BINARY_AND:
    if (!emit(context, "  and R1, R2"))
      return false;
    break;
  case WASM_BINARY_OR:
    if (!emit(context, "  or R1, R2"))
      return false;
    break;
  case WASM_BINARY_EQ:
    if (!emit(context, "  ieq R1, R2"))
      return false;
    break;
  case WASM_BINARY_NE:
    if (!emit(context, "  ine R1, R2"))
      return false;
    break;
  case WASM_BINARY_LT_S:
    if (!emit(context, "  ilt R1, R2"))
      return false;
    break;
  case WASM_BINARY_GT_S:
    if (!emit(context, "  igt R1, R2"))
      return false;
    break;
  case WASM_BINARY_GE_S:
    if (!emit(context, "  ige R1, R2"))
      return false;
    break;
  case WASM_BINARY_LE_S:
    if (!emit(context, "  ile R1, R2"))
      return false;
    break;
  case WASM_BINARY_LE_U:
    /* Bias both operands so unsigned ordering becomes signed ordering. */
    if (!emit(context, "  xor R1, 0x80000000") ||
        !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  ile R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_ADD:
    if (!emit(context, "  fadd R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_SUB:
    if (!emit(context, "  fsub R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_LE:
    if (!emit(context, "  fle R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_LT:
    if (!emit(context, "  flt R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_MUL:
    if (!emit(context, "  fmul R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_DIV:
    if (!emit(context, "  fdiv R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_GT:
    if (!emit(context, "  fgt R1, R2"))
      return false;
    break;
  case WASM_BINARY_LT_U:
    if (!emit(context, "  xor R1, 0x80000000") ||
        !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  ilt R1, R2"))
      return false;
    break;
  case WASM_BINARY_GT_U:
    if (!emit(context, "  xor R1, 0x80000000") ||
        !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  igt R1, R2"))
      return false;
    break;
  case WASM_BINARY_GE_U:
    if (!emit(context, "  xor R1, 0x80000000") ||
        !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  ige R1, R2"))
      return false;
    break;
  case WASM_BINARY_SHR_U:
    if (!emit(context, "  and R2, 31") || !emit(context, "  mov R3, 0") ||
        !emit(context, "  isub R3, R2") || !emit(context, "  shl R1, R3"))
      return false;
    break;
  case WASM_BINARY_SHR_S:
    if (!emit_i32_shr_s(context))
      return false;
    break;
  case WASM_BINARY_SHL:
    if (!emit(context, "  and R2, 31") || !emit(context, "  shl R1, R2"))
      return false;
    break;
  case WASM_BINARY_DIV_U:
    if (!emit(context, "  call __wasm_i32_div_u") ||
        !store_slot(context, left.slot, 0))
      return false;
    release(context, right);
    *value = left;
    return true;
  case WASM_BINARY_REM_U:
    if (!emit(context, "  call __wasm_i32_div_u") ||
        !load_slot(context, 1, left.slot) || !emit(context, "  imul R0, R2") ||
        !emit(context, "  isub R1, R0"))
      return false;
    break;
  case WASM_BINARY_REM_S:
    if (!fresh_label(context, "rem_s_normal", normal, sizeof(normal)) ||
        !fresh_label(context, "rem_s_done", done, sizeof(done)) ||
        !emit(context, "  mov R3, R2") || !emit(context, "  ieq R3, 0") ||
        !emit(context, "  jt R3, __wasm_trap") ||
        !emit(context, "  ieq R3, -1") ||
        !emit(context, "  jf R3, %s", normal) ||
        !emit(context, "  mov R1, 0") || !emit(context, "  jmp %s", done) ||
        !emit_label(context, normal) || !emit(context, "  imod R1, R2") ||
        !emit_label(context, done))
      return false;
    break;
  case WASM_BINARY_OTHER:
    diagnostics_error(
        context->diagnostics,
        "internal error: unsupported binary operation passed validation");
    return false;
  }
  if (!store_slot(context, left.slot, 1))
    return false;
  release(context, right);
  *value = left;
  return true;
}

/* Recursively detects one binary operation in an expression tree. */
static bool expression_uses_binary(const WasmExpr *expression,
                                   WasmBinaryOp operation) {
  size_t index;
  if (expression->kind == WASM_EXPR_BINARY &&
      expression->binary_op == operation)
    return true;
  for (index = 0; index < expression->child_count; ++index)
    if (expression_uses_binary(expression->children[index], operation))
      return true;
  return false;
}

/* Returns whether a reachable function needs a compiler-generated bulk helper.
 */
static bool expression_uses_kind(const WasmExpr *expression,
                                 WasmExprKind kind) {
  size_t index;
  if (expression->kind == kind)
    return true;
  for (index = 0; index < expression->child_count; ++index)
    if (expression_uses_kind(expression->children[index], kind))
      return true;
  return false;
}

/* Emits the shared software helper for Wasm unsigned i32 division. */
static bool emit_unsigned_division_helper(Context *context) {
  return emit_label(context, "__wasm_i32_div_u") &&
         emit(context, "  mov R3, R2") && emit(context, "  ieq R3, 0") &&
         emit(context, "  jt R3, __wasm_trap") &&
         emit(context, "  mov R3, 0") && emit(context, "  mov R4, 0") &&
         emit(context, "  mov R5, 32") &&
         emit_label(context, "__wasm_i32_div_u_loop") &&
         emit(context, "  mov R6, R1") && emit(context, "  mov R7, -31") &&
         emit(context, "  shl R6, R7") && emit(context, "  shl R1, 1") &&
         emit(context, "  shl R4, 1") && emit(context, "  or R4, R6") &&
         emit(context, "  mov R6, R4") &&
         emit(context, "  xor R6, 0x80000000") &&
         emit(context, "  mov R7, R2") &&
         emit(context, "  xor R7, 0x80000000") &&
         emit(context, "  ige R6, R7") &&
         emit(context, "  jf R6, __wasm_i32_div_u_skip") &&
         emit(context, "  isub R4, R2") && emit(context, "  shl R3, 1") &&
         emit(context, "  or R3, 1") &&
         emit(context, "  jmp __wasm_i32_div_u_decrement") &&
         emit_label(context, "__wasm_i32_div_u_skip") &&
         emit(context, "  shl R3, 1") &&
         emit_label(context, "__wasm_i32_div_u_decrement") &&
         emit(context, "  isub R5, 1") &&
         emit(context, "  jt R5, __wasm_i32_div_u_loop") &&
         emit(context, "  mov R0, R3") && emit(context, "  ret");
}

/* Emits the shared bounds checks used before either bulk helper mutates RAM. */
static bool emit_bulk_bounds_checks(Context *context, bool has_source) {
  uint32_t memory_bytes = context->memory_bytes;
  if (!emit(context, "  mov R1, [BP-3]") || !emit(context, "  ilt R1, 0") ||
      !emit(context, "  jt R1, __wasm_trap") ||
      !emit(context, "  mov R1, [BP-3]") ||
      !emit(context, "  igt R1, 0x%08X", memory_bytes) ||
      !emit(context, "  jt R1, __wasm_trap") ||
      !emit(context, "  mov R2, 0x%08X", memory_bytes) ||
      !emit(context, "  isub R2, R1") || !emit(context, "  mov R1, [BP-1]") ||
      !emit(context, "  ilt R1, 0") || !emit(context, "  jt R1, __wasm_trap") ||
      !emit(context, "  igt R1, R2") || !emit(context, "  jt R1, __wasm_trap"))
    return false;
  if (!has_source)
    return true;
  return emit(context, "  mov R1, [BP-2]") && emit(context, "  ilt R1, 0") &&
         emit(context, "  jt R1, __wasm_trap") &&
         emit(context, "  igt R1, R2") && emit(context, "  jt R1, __wasm_trap");
}

/* Emits overlap-safe Wasm memory.copy over packed byte-addressed linear memory.
 */
static bool emit_memory_copy_helper(Context *context) {
  return emit_label(context, "__wasm_memory_copy") &&
         emit(context, "  push BP") && emit(context, "  mov BP, SP") &&
         emit(context, "  isub SP, 3") && emit(context, "  mov R1, [BP+2]") &&
         emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP+3]") &&
         emit(context, "  mov [BP-2], R1") &&
         emit(context, "  mov R1, [BP+4]") &&
         emit(context, "  mov [BP-3], R1") &&
         emit_bulk_bounds_checks(context, true) &&
         /* Copy backward only when destination starts inside the source range.
          */
         emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  mov R2, [BP-2]") && emit(context, "  igt R1, R2") &&
         emit(context, "  jf R1, __wasm_memory_copy_forward") &&
         emit(context, "  mov R1, [BP-2]") &&
         emit(context, "  mov R2, [BP-3]") && emit(context, "  iadd R1, R2") &&
         emit(context, "  mov R2, [BP-1]") && emit(context, "  ilt R2, R1") &&
         emit(context, "  jt R2, __wasm_memory_copy_backward") &&
         emit_label(context, "__wasm_memory_copy_forward") &&
         emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  jf R1, __wasm_memory_copy_done") &&
         emit_label(context, "__wasm_memory_copy_forward_loop") &&
         /* Read source byte R4. */
         emit(context, "  mov R1, [BP-2]") && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 3") && emit(context, "  imul R2, -8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R4, [R3]") && emit(context, "  shl R4, R2") &&
         emit(context, "  and R4, 0x000000FF") &&
         /* Insert R4 into the destination byte lane. */
         emit(context, "  mov R1, [BP-1]") && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 3") && emit(context, "  imul R2, 8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R5, [R3]") && emit(context, "  shl R4, R2") &&
         emit(context, "  mov R6, 0x000000FF") &&
         emit(context, "  shl R6, R2") &&
         emit(context, "  xor R6, 0xFFFFFFFF") &&
         emit(context, "  and R5, R6") && emit(context, "  or R5, R4") &&
         emit(context, "  mov [R3], R5") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  iadd R1, 1") && emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP-2]") && emit(context, "  iadd R1, 1") &&
         emit(context, "  mov [BP-2], R1") &&
         emit(context, "  mov R1, [BP-3]") && emit(context, "  isub R1, 1") &&
         emit(context, "  mov [BP-3], R1") &&
         emit(context, "  jt R1, __wasm_memory_copy_forward_loop") &&
         emit(context, "  jmp __wasm_memory_copy_done") &&
         emit_label(context, "__wasm_memory_copy_backward") &&
         emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  mov R2, [BP-3]") && emit(context, "  iadd R1, R2") &&
         emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP-2]") && emit(context, "  iadd R1, R2") &&
         emit(context, "  mov [BP-2], R1") &&
         emit_label(context, "__wasm_memory_copy_backward_loop") &&
         emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  jf R1, __wasm_memory_copy_done") &&
         emit(context, "  mov R1, [BP-1]") && emit(context, "  isub R1, 1") &&
         emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP-2]") && emit(context, "  isub R1, 1") &&
         emit(context, "  mov [BP-2], R1") && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 3") && emit(context, "  imul R2, -8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R4, [R3]") && emit(context, "  shl R4, R2") &&
         emit(context, "  and R4, 0x000000FF") &&
         emit(context, "  mov R1, [BP-1]") && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 3") && emit(context, "  imul R2, 8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R5, [R3]") && emit(context, "  shl R4, R2") &&
         emit(context, "  mov R6, 0x000000FF") &&
         emit(context, "  shl R6, R2") &&
         emit(context, "  xor R6, 0xFFFFFFFF") &&
         emit(context, "  and R5, R6") && emit(context, "  or R5, R4") &&
         emit(context, "  mov [R3], R5") && emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  isub R1, 1") && emit(context, "  mov [BP-3], R1") &&
         emit(context, "  jmp __wasm_memory_copy_backward_loop") &&
         emit_label(context, "__wasm_memory_copy_done") &&
         emit(context, "  mov SP, BP") && emit(context, "  pop BP") &&
         emit(context, "  ret");
}

/* Emits Wasm memory.fill over packed byte-addressed linear memory. */
static bool emit_memory_fill_helper(Context *context) {
  return emit_label(context, "__wasm_memory_fill") &&
         emit(context, "  push BP") && emit(context, "  mov BP, SP") &&
         emit(context, "  isub SP, 3") && emit(context, "  mov R1, [BP+2]") &&
         emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP+3]") &&
         emit(context, "  mov [BP-2], R1") &&
         emit(context, "  mov R1, [BP+4]") &&
         emit(context, "  mov [BP-3], R1") &&
         emit_bulk_bounds_checks(context, false) &&
         emit_label(context, "__wasm_memory_fill_loop") &&
         emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  jf R1, __wasm_memory_fill_done") &&
         emit(context, "  mov R1, [BP-1]") && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 3") && emit(context, "  imul R2, 8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R4, [BP-2]") &&
         emit(context, "  and R4, 0x000000FF") &&
         emit(context, "  shl R4, R2") && emit(context, "  mov R5, [R3]") &&
         emit(context, "  mov R6, 0x000000FF") &&
         emit(context, "  shl R6, R2") &&
         emit(context, "  xor R6, 0xFFFFFFFF") &&
         emit(context, "  and R5, R6") && emit(context, "  or R5, R4") &&
         emit(context, "  mov [R3], R5") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  iadd R1, 1") && emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP-3]") && emit(context, "  isub R1, 1") &&
         emit(context, "  mov [BP-3], R1") &&
         emit(context, "  jmp __wasm_memory_fill_loop") &&
         emit_label(context, "__wasm_memory_fill_done") &&
         emit(context, "  mov SP, BP") && emit(context, "  pop BP") &&
         emit(context, "  ret");
}

/* Dispatches one validated expression to its V32 IR lowering. */
static bool lower_expression(Context *context, const WasmExpr *expression,
                             Value *value) {
  Value left = {0}, right = {0}, condition = {0};
  char label[64], end[64], false_label[64];
  size_t index;
  value->present = false;
  switch (expression->kind) {
  case WASM_EXPR_I32_CONST: {
    int slot = temp_slot(context);
    if (slot == 0 ||
        !emit(context, "  mov R1, 0x%08X", (uint32_t)expression->i32_value) ||
        !store_slot(context, slot, 1))
      return false;
    value->slot = slot;
    value->present = true;
    return true;
  }
  case WASM_EXPR_F32_CONST: {
    union {
      float value;
      uint32_t bits;
    } constant;
    int slot = temp_slot(context);
    constant.value = expression->f32_value;
    if (slot == 0 || !emit(context, "  mov R1, 0x%08X", constant.bits) ||
        !store_slot(context, slot, 1))
      return false;
    value->slot = slot;
    value->present = true;
    return true;
  }
  case WASM_EXPR_LOCAL_GET: {
    int slot = temp_slot(context);
    if (slot == 0 ||
        !load_slot(context, 1,
                   local_slot(context->function, expression->index)) ||
        !store_slot(context, slot, 1))
      return false;
    value->slot = slot;
    value->present = true;
    return true;
  }
  case WASM_EXPR_LOCAL_SET:
    if (!lower_expression(context, expression->children[0], &left) ||
        !left.present || !load_slot(context, 1, left.slot) ||
        !store_slot(context, local_slot(context->function, expression->index),
                    1))
      return false;
    if (expression->is_tee) {
      *value = left;
      return true;
    }
    release(context, left);
    return true;
  case WASM_EXPR_STACK_POINTER_GET: {
    int slot = temp_slot(context);
    if (slot == 0 ||
        !emit(context, "  mov R1, [%u]", VIRCON_WASM_STACK_POINTER_WORD) ||
        !store_slot(context, slot, 1))
      return false;
    value->slot = slot;
    value->present = true;
    return true;
  }
  case WASM_EXPR_STACK_POINTER_SET:
    if (!lower_expression(context, expression->children[0], &left) ||
        !left.present || !load_slot(context, 1, left.slot) ||
        !emit(context, "  mov [%u], R1", VIRCON_WASM_STACK_POINTER_WORD))
      return false;
    release(context, left);
    return true;
  case WASM_EXPR_UNARY:
    if (!lower_expression(context, expression->children[0], &left) ||
        !left.present || !load_slot(context, 1, left.slot))
      return false;
    if (expression->unary_op == WASM_UNARY_EQZ) {
      if (!emit(context, "  ieq R1, 0"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_CONVERT_I32_S_TO_F32) {
      if (!emit(context, "  cif R1"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_CONVERT_I32_U_TO_F32) {
      if (!emit_f32_convert_i32_u(context))
        return false;
    } else if (expression->unary_op == WASM_UNARY_TRUNC_SAT_F32_TO_I32) {
      char nan[64], minimum[64], maximum[64], done[64];
      if (!fresh_label(context, "trunc_nan", nan, sizeof(nan)) ||
          !fresh_label(context, "trunc_min", minimum, sizeof(minimum)) ||
          !fresh_label(context, "trunc_max", maximum, sizeof(maximum)) ||
          !fresh_label(context, "trunc_done", done, sizeof(done)) ||
          !emit(context, "  mov R3, R1") || !emit(context, "  feq R3, R1") ||
          !emit(context, "  jf R3, %s", nan) ||
          !emit(context, "  mov R2, 0xCF000000") ||
          !emit(context, "  mov R3, R1") || !emit(context, "  fle R3, R2") ||
          !emit(context, "  jt R3, %s", minimum) ||
          !emit(context, "  mov R2, 0x4F000000") ||
          !emit(context, "  mov R3, R2") || !emit(context, "  fle R3, R1") ||
          !emit(context, "  jt R3, %s", maximum) ||
          !emit(context, "  cfi R1") || !emit(context, "  jmp %s", done) ||
          !emit_label(context, nan) || !emit(context, "  mov R1, 0") ||
          !emit(context, "  jmp %s", done) || !emit_label(context, minimum) ||
          !emit(context, "  mov R1, 0x80000000") ||
          !emit(context, "  jmp %s", done) || !emit_label(context, maximum) ||
          !emit(context, "  mov R1, 0x7FFFFFFF") || !emit_label(context, done))
        return false;
    } else
      return false;
    if (!store_slot(context, left.slot, 1))
      return false;
    *value = left;
    return true;
  case WASM_EXPR_BINARY:
    return lower_binary(context, expression, value);
  case WASM_EXPR_SELECT:
    /* Children retain Wasm's evaluation order: first, second, condition. */
    if (!lower_expression(context, expression->children[0], &left) ||
        !lower_expression(context, expression->children[1], &right) ||
        !lower_expression(context, expression->children[2], &condition) ||
        !left.present || !right.present || !condition.present ||
        !fresh_label(context, "select_false", false_label,
                     sizeof(false_label)) ||
        !fresh_label(context, "select_done", end, sizeof(end)) ||
        !load_slot(context, 1, condition.slot) ||
        !emit(context, "  jf R1, %s", false_label) ||
        !load_slot(context, 1, left.slot) || !emit(context, "  jmp %s", end) ||
        !emit_label(context, false_label) ||
        !load_slot(context, 1, right.slot) || !emit_label(context, end) ||
        !store_slot(context, left.slot, 1))
      return false;
    release(context, condition);
    release(context, right);
    *value = left;
    return true;
  case WASM_EXPR_LOAD:
    return lower_load(context, expression, value);
  case WASM_EXPR_STORE:
    return lower_store(context, expression, value);
  case WASM_EXPR_I64_CONST_STORE:
    return lower_i64_const_store(context, expression, value);
  case WASM_EXPR_I64_LOAD_STORE:
    return lower_i64_load_store(context, expression, value);
  case WASM_EXPR_I64_WORD_EXTRACT:
    return lower_i64_word_extract(context, expression, value);
  case WASM_EXPR_I64_LOAD_STORE_LOCAL_TEE:
    return lower_i64_load_store_local_tee(context, expression, value);
  case WASM_EXPR_I64_LOCAL_WORD_EXTRACT:
    return lower_i64_local_word_extract(context, expression, value);
  case WASM_EXPR_I64_LOCAL_TEE_WORD_EXTRACT:
    return lower_i64_local_tee_word_extract(context, expression, value);
  case WASM_EXPR_I64_PACKED_I32_STORE:
    return lower_i64_packed_i32_store(context, expression, value);
  case WASM_EXPR_MEMORY_COPY:
  case WASM_EXPR_MEMORY_FILL:
    return lower_bulk_memory(context, expression, value);
  case WASM_EXPR_CALL:
    return lower_call(context, expression, value);
  case WASM_EXPR_BLOCK:
    if (expression->name != NULL) {
      if (!fresh_label(context, "block_end", label, sizeof(label)) ||
          context->target_count == 32)
        return false;
      context->targets[context->target_count++] =
          (Target){expression->name, format_text("%s", label)};
    }
    for (index = 0; index < expression->child_count; ++index) {
      if (value->present)
        release(context, *value);
      value->present = false;
      if (!lower_expression(context, expression->children[index], value))
        return false;
    }
    if (expression->name != NULL) {
      Target target = context->targets[--context->target_count];
      if (!emit_label(context, target.label)) {
        free(target.label);
        return false;
      }
      free(target.label);
    }
    return true;
  case WASM_EXPR_LOOP:
    if (!fresh_label(context, "loop", label, sizeof(label)) ||
        context->target_count == 32)
      return false;
    context->targets[context->target_count++] =
        (Target){expression->name, format_text("%s", label)};
    if (!emit_label(context, label) ||
        !lower_expression(context, expression->children[0], value))
      return false;
    free(context->targets[--context->target_count].label);
    value->present = false;
    return true;
  case WASM_EXPR_BR:
    for (index = context->target_count; index != 0; --index)
      if (strcmp(context->targets[index - 1].wasm_name, expression->name) == 0)
        return emit(context, "  jmp %s", context->targets[index - 1].label);
    diagnostics_error(context->diagnostics,
                      "branch targets '%s' outside active structured control",
                      expression->name);
    return false;
  case WASM_EXPR_BR_IF:
    if (!lower_expression(context, expression->children[0], &condition) ||
        !condition.present || !load_slot(context, 1, condition.slot))
      return false;
    release(context, condition);
    for (index = context->target_count; index != 0; --index)
      if (strcmp(context->targets[index - 1].wasm_name, expression->name) == 0)
        return emit(context, "  jt R1, %s", context->targets[index - 1].label);
    diagnostics_error(
        context->diagnostics,
        "conditional branch targets '%s' outside active structured control",
        expression->name);
    return false;
  case WASM_EXPR_IF:
    if (!lower_expression(context, expression->children[0], &left) ||
        !left.present || !fresh_label(context, "if_end", end, sizeof(end)) ||
        !load_slot(context, 1, left.slot) || !emit(context, "  jf R1, %s", end))
      return false;
    release(context, left);
    if (!lower_expression(context, expression->children[1], &right))
      return false;
    release(context, right);
    return emit_label(context, end);
  case WASM_EXPR_RETURN:
    if (expression->child_count != 0) {
      if (!lower_expression(context, expression->children[0], &left) ||
          !left.present || !load_slot(context, 0, left.slot))
        return false;
      release(context, left);
    }
    return emit(context, "  jmp %s", context->return_label);
  case WASM_EXPR_DROP:
    if (!lower_expression(context, expression->children[0], &left))
      return false;
    release(context, left);
    value->present = false;
    return true;
  case WASM_EXPR_UNREACHABLE:
    return emit(context, "  jmp __wasm_trap");
  case WASM_EXPR_GLOBAL_GET:
  case WASM_EXPR_GLOBAL_SET:
    diagnostics_error(context->diagnostics,
                      "internal error: unsupported global passed validation");
    return false;
  }
  diagnostics_error(context->diagnostics,
                    "internal error: unhandled Wasm expression");
  return false;
}

/* Packs active Wasm data segments into writable target RAM at startup. */
static bool initialize_data(const ValidatedModule *validated,
                            Context *context) {
  size_t bytes = (size_t)context->memory_bytes, words = (bytes + 3) / 4, index,
         segment_index;
  unsigned char *memory = calloc(bytes, 1);
  bool *touched = calloc(words, sizeof(*touched));
  if (memory == NULL || touched == NULL) {
    free(memory);
    free(touched);
    diagnostics_error(context->diagnostics,
                      "out of memory constructing initial linear memory");
    return false;
  }
  for (segment_index = 0; segment_index < validated->module->data_segment_count;
       ++segment_index) {
    const WasmDataSegment *segment =
        &validated->module->data_segments[segment_index];
    memcpy(memory + segment->offset, segment->bytes, segment->size);
    for (index = segment->offset / 4;
         index <= (segment->offset + segment->size - 1) / 4 &&
         segment->size != 0;
         ++index)
      touched[index] = true;
  }
  for (index = 0; index < words; ++index)
    if (touched[index]) {
      uint32_t word = (uint32_t)memory[index * 4] |
                      ((uint32_t)memory[index * 4 + 1] << 8) |
                      ((uint32_t)memory[index * 4 + 2] << 16) |
                      ((uint32_t)memory[index * 4 + 3] << 24);
      if (!emit(context, "  mov R1, 0x%08X", word) ||
          !emit(context, "  mov [%u], R1", LINEAR_BASE + (unsigned)index)) {
        free(memory);
        free(touched);
        return false;
      }
    }
  free(memory);
  free(touched);
  return true;
}

/* Emits one reachable Wasm function and its compiler-defined frame. */
static bool lower_function(const ValidatedModule *validated,
                           const WasmFunction *function,
                           VirconIrProgram *program, Diagnostics *diagnostics,
                           uint32_t memory_bytes) {
  Context context = {0};
  Value result = {0};
  char label[64];
  context.validated = validated;
  context.function = function;
  context.program = program;
  context.diagnostics = diagnostics;
  context.memory_bytes = memory_bytes;
  function_label(validated->module, function, label, sizeof(label));
  snprintf(context.return_label, sizeof(context.return_label),
           "__wasm_return_%zu", function_index(validated->module, function));
  if (!emit_label(&context, label) || !emit(&context, "  push BP") ||
      !emit(&context, "  mov BP, SP") ||
      !emit(&context, "  isub SP, %u",
            (unsigned)(local_storage_words(function) + TEMP_SLOTS +
                       OUTGOING_SLOTS)) ||
      !lower_expression(&context, function->body, &result))
    return false;
  if (function->result == WASM_VALUE_I32 ||
      function->result == WASM_VALUE_F32) {
    if (result.present) {
      if (!load_slot(&context, 0, result.slot))
        return false;
    } else if (!emit(&context, "  mov R0, 0"))
      return false;
  }
  release(&context, result);
  return emit_label(&context, context.return_label) &&
         emit(&context, "  mov SP, BP") && emit(&context, "  pop BP") &&
         emit(&context, "  ret");
}

/* Emits startup, shared helpers, and all reachable functions into V32 IR. */
bool lower_module_to_vircon_ir(const ValidatedModule *validated,
                               VirconIrProgram *program,
                               Diagnostics *diagnostics) {
  Context startup = {0};
  uint32_t memory_bytes = validated->module->memory_initial_pages * 65536u;
  size_t index;
  char entry_label[64];
  bool needs_unsigned_division = false, needs_memory_copy = false,
       needs_memory_fill = false;
  startup.program = program;
  startup.diagnostics = diagnostics;
  startup.memory_bytes = memory_bytes;
  function_label(validated->module, validated->entry, entry_label,
                 sizeof(entry_label));
  if (!emit_label(&startup, "__wasm_entry") ||
      !initialize_data(validated, &startup) ||
      (validated->module->global_count != 0 &&
       (!emit(&startup, "  mov R1, 0x%08X",
              validated->module->stack_pointer_initial) ||
        !emit(&startup, "  mov [%u], R1", VIRCON_WASM_STACK_POINTER_WORD))) ||
      !emit(&startup, "  call %s", entry_label) || !emit(&startup, "  hlt") ||
      !emit_label(&startup, "__wasm_trap") ||
      !emit(&startup, "  hlt  ; Wasm memory/unreachable trap"))
    return false;
  for (index = 0; index < validated->module->function_count; ++index) {
    const WasmFunction *function = &validated->module->functions[index];
    if (validated->reachable[index] && !function->is_import &&
        (expression_uses_binary(function->body, WASM_BINARY_DIV_U) ||
         expression_uses_binary(function->body, WASM_BINARY_REM_U)))
      needs_unsigned_division = true;
    if (validated->reachable[index] && !function->is_import &&
        expression_uses_kind(function->body, WASM_EXPR_MEMORY_COPY))
      needs_memory_copy = true;
    if (validated->reachable[index] && !function->is_import &&
        expression_uses_kind(function->body, WASM_EXPR_MEMORY_FILL))
      needs_memory_fill = true;
    if (validated->reachable[index] && !function->is_import &&
        !lower_function(validated, function, program, diagnostics,
                        memory_bytes))
      return false;
  }
  if (needs_unsigned_division && !emit_unsigned_division_helper(&startup))
    return false;
  if (needs_memory_copy && !emit_memory_copy_helper(&startup))
    return false;
  if (needs_memory_fill && !emit_memory_fill_helper(&startup))
    return false;
  return true;
}
