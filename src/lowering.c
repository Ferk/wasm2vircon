/*
 * VirconWasm-to-V32-IR lowering.
 *
 * This file is the frontend boundary: it converts validated compiler-owned
 * Wasm expressions into compiler-owned V32 IR. Binaryen objects do not reach
 * this stage, and Wasm byte-memory semantics are legalized here. The current
 * instruction-selection helpers use a strict internal decoder while they are
 * incrementally migrated to direct structured constructors; no assembler text
 * is retained in the IR.
 */

#include "lowering.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target_layout.h"

#define LINEAR_BASE VIRCON_LINEAR_MEMORY_BASE
#define TEMP_SLOTS 48u
#define LOCAL_ZERO_UNROLL_LIMIT 4u
/* Keep common small calls allocation-free; this is not an ABI arity limit. */
#define INLINE_CALL_ARGUMENTS 4u

/* One compiler value. Scalars may remain immediate; stored i64 values occupy
 * consecutive low/high frame slots. */
typedef struct Value {
  int slot;
  int high_slot;
  uint32_t immediate;
  WasmValueType type;
  bool present;
  bool is_immediate;
  /* Borrowed values alias a parameter/local frame slot and must be copied
   * before evaluating another expression that could overwrite that local. */
  bool is_borrowed;
  /* A result kept transiently in one target register for an immediate
   * consumer, encoded as register number plus one so zero remains "none". */
  unsigned register_plus_one;
  /* Optional affine identity used only to reuse an already emitted bounds
   * check. The represented Wasm pointer remains a byte offset. */
  bool has_address_identity;
  uint32_t address_base_local;
  uint32_t address_offset;
} Value;
/* A structured-control target mapped from a Wasm label to an assembly label. */
typedef struct Target {
  const char *wasm_name;
  char *label;
  int result_slot;
} Target;
/* One immutable cartridge-ROM table of V32 code addresses for a br_table. */
typedef struct JumpTable {
  char *label;
  char **target_labels;
  size_t target_count;
} JumpTable;
/* One byte-address range proven in the current straight-line region. Ranges
 * become mergeable only after successful checks establish both endpoints. */
typedef struct CheckedAddress {
  uint32_t base_local, offset, width;
  bool mergeable;
} CheckedAddress;
/* Runtime source for a loop trip limit accepted by the first conservative
 * affine-loop versioner. */
typedef enum LoopBoundKind {
  LOOP_BOUND_NONE,
  LOOP_BOUND_CONSTANT,
  LOOP_BOUND_LOCAL,
  LOOP_BOUND_ALIGNED_MEMORY
} LoopBoundKind;
typedef enum LoopTripKind {
  LOOP_TRIP_NONE,
  LOOP_TRIP_INCREMENT_TO_LIMIT,
  LOOP_TRIP_COUNTDOWN
} LoopTripKind;
/* One guarded affine loop. The fast copy may omit checks only for
 * byte ranges covered by [minimum_offset, maximum_end). */
typedef struct LoopFastPath {
  bool valid;
  bool use_word_pointer;
  uint32_t base_local, counter_local, stride;
  uint32_t minimum_offset, maximum_end;
  LoopTripKind trip_kind;
  LoopBoundKind bound_kind;
  uint32_t bound_value;
} LoopFastPath;
/* Alignment facts accumulated at one structured branch target. */
typedef struct AlignmentTarget {
  const char *name;
  uint8_t *incoming;
  bool has_incoming;
} AlignmentTarget;
/* Temporary state for the structured alignment dataflow analysis. */
typedef struct AlignmentAnalysis {
  struct Context *context;
  AlignmentTarget *targets;
  size_t target_count, target_capacity;
  bool failed;
} AlignmentAnalysis;
/* Per-function lowering state, including structured targets and temp slots. */
typedef struct Context {
  const ValidatedModule *validated;
  const WasmFunction *function;
  VirconIrProgram *program;
  Diagnostics *diagnostics;
  Target *targets;
  size_t target_count, target_capacity;
  JumpTable *jump_tables;
  size_t jump_table_count, jump_table_capacity;
  unsigned next_label, temp_depth;
  char return_label[64];
  uint32_t memory_bytes;
  bool memory_can_grow;
  /* Guaranteed byte alignment for parameters and locals, capped at one
   * Vircon32 word. These facts are derived from every possible assignment;
   * the Wasm memarg alignment hint is deliberately not trusted. */
  uint8_t *local_alignments;
  size_t local_alignment_count;
  uint32_t *local_address_bases, *local_address_offsets;
  CheckedAddress checked_addresses[16];
  size_t checked_address_count;
  /* R13 may retain one proven-aligned Vircon word address inside a straight
   * line region. Calls and control-flow joins invalidate this cache. */
  bool target_word_address_cached;
  uint32_t target_word_address_base_local;
  uint32_t target_word_address_offset;
  /* A loop-version guard has proven every iteration of this affine range in
   * bounds. Wasm pointers remain byte offsets; only redundant checks are
   * skipped while lowering the guarded fast copy. */
  bool fast_memory_active;
  uint32_t fast_memory_base_local;
  uint32_t fast_memory_minimum_offset;
  uint32_t fast_memory_maximum_end;
  /* R13 is a native word pointer for a proven-aligned affine Wasm pointer;
   * R10 retains a stable increment-loop limit loaded by the version guard. */
  bool fast_word_pointer_active;
  uint32_t fast_word_pointer_base_local;
  uint32_t fast_word_pointer_stride;
  bool fast_bound_active;
  LoopBoundKind fast_bound_kind;
  uint32_t fast_bound_value;
  int preferred_result_register;
  const WasmExpr *current_expression;
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
/* Formats one selected target operation and decodes it into structured V32 IR. */
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
  return vircon_ir_append_generated_instruction(context->program, text, context->diagnostics);
}
/* Allocates a function-unique internal label for generated control flow. */
static bool fresh_label(Context *context, const char *kind, char *out, size_t size) {
  return snprintf(out, size, "__wasm_%s_%zu_%u", kind,
                  (size_t)(context->function - context->validated->module->functions), context->next_label++) > 0;
}
/* Appends one structured assembly target label. */
static bool emit_label(Context *context, const char *label) {
  size_t index;
  context->checked_address_count = 0;
  context->target_word_address_cached = false;
  /* A label may be reached without executing the preceding textual path.
   * Reset aliases to their local's own runtime value at every join. */
  for (index = 0; index < context->local_alignment_count; ++index) {
    context->local_address_bases[index] = (uint32_t)index;
    context->local_address_offsets[index] = 0;
  }
  return vircon_ir_append_label(context->program, label, context->diagnostics);
}

/* Finds an active structured-control target by its Binaryen-assigned name. */
static const Target *find_target(const Context *context, const char *name) {
  size_t index;
  for (index = context->target_count; index != 0; --index)
    if (strcmp(context->targets[index - 1].wasm_name, name) == 0)
      return &context->targets[index - 1];
  return NULL;
}
/* Returns only the target address for value-less structured operations. */
static const char *find_target_label(const Context *context, const char *name) {
  const Target *target = find_target(context, name);
  return target == NULL ? NULL : target->label;
}

/* Pushes one structured-control target onto a growable compiler-owned stack. */
static bool push_target(Context *context, const char *wasm_name, const char *label, int result_slot) {
  Target *targets;
  char *label_copy;
  size_t capacity;

  if (context->target_count == context->target_capacity) {
    capacity = context->target_capacity == 0 ? 16 : context->target_capacity * 2;
    if (capacity < context->target_capacity || capacity > SIZE_MAX / sizeof(*targets)) {
      diagnostics_error(context->diagnostics, "structured-control nesting is too large to represent");
      return false;
    }
    targets = realloc(context->targets, capacity * sizeof(*targets));
    if (targets == NULL) {
      diagnostics_error(context->diagnostics, "out of memory growing the structured-control stack");
      return false;
    }
    context->targets = targets;
    context->target_capacity = capacity;
  }

  label_copy = format_text("%s", label);
  if (label_copy == NULL) {
    diagnostics_error(context->diagnostics, "out of memory recording a structured-control target");
    return false;
  }
  context->targets[context->target_count++] = (Target){wasm_name, label_copy, result_slot};
  return true;
}

/* Pops one structured-control target and transfers its label ownership. */
static Target pop_target(Context *context) { return context->targets[--context->target_count]; }

/* Releases labels left active when lowering aborts inside nested control. */
static void dispose_targets(Context *context) {
  size_t index;
  for (index = 0; index < context->target_count; ++index)
    free(context->targets[index].label);
  free(context->targets);
  context->targets = NULL;
  context->target_count = 0;
  context->target_capacity = 0;
}

/* Releases the owned target-address tables accumulated for one function. */
static void dispose_jump_tables(Context *context) {
  size_t table_index, target_index;
  for (table_index = 0; table_index < context->jump_table_count; ++table_index) {
    JumpTable *table = &context->jump_tables[table_index];
    free(table->label);
    for (target_index = 0; target_index < table->target_count; ++target_index)
      free(table->target_labels[target_index]);
    free(table->target_labels);
  }
  free(context->jump_tables);
  context->jump_tables = NULL;
  context->jump_table_count = 0;
  context->jump_table_capacity = 0;
}

/* Records a ROM table after resolving each Wasm case label structurally. */
static bool record_jump_table(Context *context, const char *label, const WasmExpr *expression) {
  JumpTable table = {0};
  JumpTable *tables;
  size_t index, capacity;
  if (context->jump_table_count == context->jump_table_capacity) {
    capacity = context->jump_table_capacity == 0 ? 4 : context->jump_table_capacity * 2;
    tables = realloc(context->jump_tables, capacity * sizeof(*tables));
    if (tables == NULL) {
      diagnostics_error(context->diagnostics, "out of memory recording br_table targets");
      return false;
    }
    context->jump_tables = tables;
    context->jump_table_capacity = capacity;
  }
  table.label = format_text("%s", label);
  table.target_count = expression->branch_target_count;
  table.target_labels = calloc(table.target_count, sizeof(*table.target_labels));
  if (table.label == NULL || table.target_labels == NULL) {
    free(table.label);
    free(table.target_labels);
    diagnostics_error(context->diagnostics, "out of memory recording br_table targets");
    return false;
  }
  for (index = 0; index < table.target_count; ++index) {
    const char *target = find_target_label(context, expression->branch_targets[index]);
    if (target == NULL) {
      diagnostics_error(context->diagnostics,
                        "br_table targets '%s' outside active structured "
                        "control",
                        expression->branch_targets[index]);
      while (index != 0)
        free(table.target_labels[--index]);
      free(table.target_labels);
      free(table.label);
      return false;
    }
    table.target_labels[index] = format_text("%s", target);
    if (table.target_labels[index] == NULL) {
      diagnostics_error(context->diagnostics, "out of memory recording br_table targets");
      while (index != 0)
        free(table.target_labels[--index]);
      free(table.target_labels);
      free(table.label);
      return false;
    }
  }
  context->jump_tables[context->jump_table_count++] = table;
  return true;
}

/* Emits all function-local br_table address data after executable code. */
static bool emit_jump_tables(Context *context) {
  size_t table_index, target_index;
  for (table_index = 0; table_index < context->jump_table_count; ++table_index) {
    const JumpTable *table = &context->jump_tables[table_index];
    if (!emit_label(context, table->label))
      return false;
    for (target_index = 0; target_index < table->target_count; ++target_index)
      if (!vircon_ir_append_pointer(context->program, table->target_labels[target_index], context->diagnostics))
        return false;
  }
  return true;
}

/* Counts target words reserved for Wasm locals; pair-valued i64 locals use two. */
static size_t local_storage_words(const WasmFunction *function) {
  size_t index, words = 0;
  for (index = 0; index < function->local_count; ++index)
    words += function->locals[index] == WASM_VALUE_I64 ? 2u : 1u;
  return words;
}

/* Restores Wasm's per-invocation zero value for every non-parameter local.
 * Small frames use direct stores; larger frames use Vircon32's native SETS
 * operation over the contiguous local range [BP-local_words, BP-1]. */
static bool initialize_function_locals(Context *context, size_t local_words) {
  size_t index;
  if (local_words == 0)
    return true;
  if (local_words <= LOCAL_ZERO_UNROLL_LIMIT) {
    if (!emit(context, "  mov R1, 0"))
      return false;
    for (index = 1; index <= local_words; ++index)
      if (!emit(context, "  mov [BP-%zu], R1", index))
        return false;
    return true;
  }
  return emit(context, "  mov R11, %zu", local_words) && emit(context, "  mov R12, 0") &&
         emit(context, "  mov R13, BP") && emit(context, "  isub R13, %zu", local_words) &&
         emit(context, "  sets");
}

/* Finds the largest caller-owned argument area required by one function body.
 * Platform imports write ports directly, but defined calls and compiler
 * bulk-memory helpers pass their arguments through the normal stack ABI. */
static size_t outgoing_call_slots(const WasmModule *module, const WasmExpr *expression) {
  size_t index, slots = 0;

  if (expression->kind == WASM_EXPR_CALL) {
    const WasmFunction *callee = wasm_module_find_function(module, expression->name);
    if (callee != NULL && !callee->is_import)
      slots = expression->child_count;
  } else if (expression->kind == WASM_EXPR_CALL_INDIRECT) {
    slots = expression->signature_param_count;
  } else if (expression->kind == WASM_EXPR_MEMORY_COPY || expression->kind == WASM_EXPR_MEMORY_FILL) {
    slots = 3;
  }
  for (index = 0; index < expression->child_count; ++index) {
    size_t child_slots = outgoing_call_slots(module, expression->children[index]);
    if (child_slots > slots)
      slots = child_slots;
  }
  return slots;
}

/* Reserves one compiler-managed word below the current function's locals. */
static int temp_slots(Context *context, unsigned words) {
  unsigned first;
  if (words == 0 || context->temp_depth > TEMP_SLOTS - words) {
    diagnostics_error(context->diagnostics, "expression nesting exceeds VirconWasm v1 temporary-slot limit");
    return 0;
  }
  first = context->temp_depth + 1;
  context->temp_depth += words;
  return -(int)(local_storage_words(context->function) + first);
}
/* Reserves one compiler-managed word below the current function's locals. */
static int temp_slot(Context *context) { return temp_slots(context, 1); }
/* Releases the most recently reserved temporary when value owns one. */
static void release(Context *context, Value value) {
  unsigned words = value.type == WASM_VALUE_I64 ? 2u : 1u;
  if (value.present && value.register_plus_one == 0 && !value.is_immediate && !value.is_borrowed &&
      context->temp_depth >= words)
    context->temp_depth -= words;
}
/* Loads a frame slot into a target register. */
static bool load_slot(Context *context, int reg, int slot) { return emit(context, "  mov R%d, [BP%+d]", reg, slot); }
/* Stores a target register into a frame slot. */
static bool store_slot(Context *context, int slot, int reg) { return emit(context, "  mov [BP%+d], R%d", slot, reg); }
/* Loads a one-word value regardless of whether it is still an immediate or
 * has already been assigned compiler-owned frame storage. */
static bool load_value(Context *context, int reg, Value value) {
  return value.register_plus_one != 0
             ? (reg == (int)value.register_plus_one - 1 ||
                emit(context, "  mov R%d, R%d", reg, (int)value.register_plus_one - 1))
         : value.is_immediate ? emit(context, "  mov R%d, 0x%08X", reg, value.immediate)
                            : load_slot(context, reg, value.slot);
}
/* Reserves writable storage for an immediate, borrowed, or register value. */
static bool reserve_value_slot(Context *context, Value *value) {
  int slot;
  if (!value->is_immediate && !value->is_borrowed && value->register_plus_one == 0)
    return true;
  slot = temp_slot(context);
  if (slot == 0)
    return false;
  value->slot = slot;
  value->is_immediate = false;
  value->is_borrowed = false;
  value->register_plus_one = 0;
  return true;
}
/* Materializes a transient value when its original value must survive in storage. */
static bool materialize_value(Context *context, Value *value) {
  Value source = *value;
  return reserve_value_slot(context, value) &&
         ((!source.is_immediate && !source.is_borrowed && source.register_plus_one == 0) ||
          (load_value(context, 1, source) && store_slot(context, value->slot, 1)));
}

/* Copies a borrowed local only when later evaluation could invalidate it. */
static bool materialize_borrowed(Context *context, Value *value) {
  return (!value->is_borrowed && value->register_plus_one == 0) || materialize_value(context, value);
}

/* Retains a one-word result in the preferred target register when the caller
 * will consume it immediately. Register-backed values are deliberately short
 * lived: any operation which may clobber the register materializes them first. */
static bool retain_result_register(Context *context, Value *value, WasmValueType type) {
  int reg = context->preferred_result_register;
  if (reg < 0 || type == WASM_VALUE_I64)
    return false;
  release(context, *value);
  value->slot = 0;
  value->high_slot = 0;
  value->type = type;
  value->is_immediate = false;
  value->is_borrowed = false;
  value->register_plus_one = (unsigned)reg + 1u;
  value->has_address_identity = false;
  value->present = true;
  return true;
}

/* Invalidates bounds/alias facts affected by assigning one Wasm local, then
 * records a non-self affine alias when the assigned value has one. */
static void assign_local_address_identity(Context *context, uint32_t local, const Value *value) {
  size_t read_index, write_index = 0, index;

  if (local >= context->local_alignment_count)
    return;
  if (context->target_word_address_cached && context->target_word_address_base_local == local)
    context->target_word_address_cached = false;
  for (read_index = 0; read_index < context->checked_address_count; ++read_index)
    if (context->checked_addresses[read_index].base_local != local)
      context->checked_addresses[write_index++] = context->checked_addresses[read_index];
  context->checked_address_count = write_index;
  for (index = 0; index < context->local_alignment_count; ++index)
    if (context->local_address_bases[index] == local) {
      context->local_address_bases[index] = (uint32_t)index;
      context->local_address_offsets[index] = 0;
    }
  if (value->has_address_identity && value->address_base_local != local) {
    context->local_address_bases[local] = value->address_base_local;
    context->local_address_offsets[local] = value->address_offset;
  }
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

/* Returns the declared Wasm type for a parameter or function-local index. */
static WasmValueType local_value_type(const WasmFunction *function, uint32_t index) {
  if (index < function->param_count)
    return function->params[index];
  index -= (uint32_t)function->param_count;
  return index < function->local_count ? function->locals[index] : WASM_VALUE_OTHER;
}

/* Returns the useful power-of-two byte alignment of a constant, capped at a
 * Vircon32 word because wider alignment cannot simplify current memory ops. */
static uint8_t constant_word_alignment(uint32_t value) {
  if ((value & 3u) == 0)
    return 4;
  if ((value & 1u) == 0)
    return 2;
  return 1;
}

/* Derives the alignment of a binary result from its already analyzed inputs. */
static uint8_t binary_result_alignment(const WasmExpr *expression, uint8_t left, uint8_t right) {
  switch (expression->binary_op) {
  case WASM_BINARY_ADD:
  case WASM_BINARY_SUB:
    return left < right ? left : right;
  case WASM_BINARY_MUL:
    if (left >= 4 || right >= 4 || (left >= 2 && right >= 2))
      return 4;
    return left > right ? left : right;
  case WASM_BINARY_SHL:
    if (expression->children[1]->kind == WASM_EXPR_I32_CONST) {
      uint32_t shift = (uint32_t)expression->children[1]->i32_value & 31u;
      if (shift >= 2 || left >= (uint8_t)(4u >> shift))
        return 4;
      return (uint8_t)(left << shift);
    }
    return left;
  case WASM_BINARY_AND:
    return left > right ? left : right;
  case WASM_BINARY_OR:
  case WASM_BINARY_XOR:
    return left < right ? left : right;
  default:
    return 1;
  }
}

/* Merges facts from one possible control-flow edge into a destination. */
static void merge_alignment_facts(uint8_t *destination, bool *has_destination, const uint8_t *source,
                                  size_t count) {
  size_t index;
  if (!*has_destination) {
    memcpy(destination, source, count);
    *has_destination = true;
    return;
  }
  for (index = 0; index < count; ++index)
    if (source[index] < destination[index])
      destination[index] = source[index];
}

/* Pushes one Wasm structured target for branch-sensitive alignment analysis. */
static bool push_alignment_target(AlignmentAnalysis *analysis, const char *name) {
  AlignmentTarget *targets;
  size_t capacity = analysis->target_capacity;
  size_t count = analysis->context->local_alignment_count;

  if (analysis->target_count == capacity) {
    capacity = capacity == 0 ? 16 : capacity * 2;
    targets = realloc(analysis->targets, capacity * sizeof(*targets));
    if (targets == NULL)
      return false;
    analysis->targets = targets;
    analysis->target_capacity = capacity;
  }
  analysis->targets[analysis->target_count] = (AlignmentTarget){.name = name};
  analysis->targets[analysis->target_count].incoming = malloc(count == 0 ? 1 : count);
  if (analysis->targets[analysis->target_count].incoming == NULL)
    return false;
  ++analysis->target_count;
  return true;
}

/* Removes the innermost alignment target and releases its fact storage. */
static void pop_alignment_target(AlignmentAnalysis *analysis) {
  free(analysis->targets[analysis->target_count - 1].incoming);
  --analysis->target_count;
}

/* Finds a structured alignment target using Wasm's innermost-label rule. */
static AlignmentTarget *find_alignment_target(AlignmentAnalysis *analysis, const char *name) {
  size_t index;
  for (index = analysis->target_count; index != 0; --index)
    if (analysis->targets[index - 1].name != NULL && name != NULL &&
        strcmp(analysis->targets[index - 1].name, name) == 0)
      return &analysis->targets[index - 1];
  return NULL;
}

/* Records the current facts on one structured branch edge. */
static void branch_alignment_to(AlignmentAnalysis *analysis, const char *name, const uint8_t *locals) {
  AlignmentTarget *target = find_alignment_target(analysis, name);
  if (target != NULL)
    merge_alignment_facts(target->incoming, &target->has_incoming, locals,
                          analysis->context->local_alignment_count);
}

static uint8_t analyze_expression_alignment(AlignmentAnalysis *analysis, WasmExpr *expression,
                                            uint8_t *locals, bool *reachable);

/* Analyzes a sequence block and merges both fallthrough and branch-to-block
 * edges at its structured exit. */
static uint8_t analyze_block_alignment(AlignmentAnalysis *analysis, WasmExpr *expression,
                                       uint8_t *locals, bool *reachable) {
  size_t index, count = analysis->context->local_alignment_count;
  uint8_t result = 1;
  AlignmentTarget *target;
  bool has_exit = false;
  uint8_t *exit_facts = malloc(count == 0 ? 1 : count);

  if (exit_facts == NULL || !push_alignment_target(analysis, expression->name)) {
    free(exit_facts);
    analysis->failed = true;
    return 1;
  }
  for (index = 0; index < expression->child_count && *reachable; ++index)
    result = analyze_expression_alignment(analysis, expression->children[index], locals, reachable);
  target = &analysis->targets[analysis->target_count - 1];
  if (*reachable)
    merge_alignment_facts(exit_facts, &has_exit, locals, count);
  if (target->has_incoming)
    merge_alignment_facts(exit_facts, &has_exit, target->incoming, count);
  if (target->has_incoming && expression->value_type != WASM_VALUE_NONE)
    result = 1;
  if (has_exit)
    memcpy(locals, exit_facts, count);
  *reachable = has_exit;
  pop_alignment_target(analysis);
  free(exit_facts);
  return expression->value_type == WASM_VALUE_NONE ? 1 : result;
}

/* Computes a fixed point for loop-header facts from entry and back-branch
 * edges, then leaves only the loop's fallthrough facts as its exit state. */
static uint8_t analyze_loop_alignment(AlignmentAnalysis *analysis, WasmExpr *expression,
                                      uint8_t *locals, bool *reachable) {
  size_t index, count = analysis->context->local_alignment_count;
  uint8_t result = 1;
  uint8_t *entry = malloc(count == 0 ? 1 : count);
  uint8_t *header = malloc(count == 0 ? 1 : count);
  uint8_t *body = malloc(count == 0 ? 1 : count);
  bool body_reachable = true, changed;
  AlignmentTarget *target;
  size_t target_index;

  if (entry == NULL || header == NULL || body == NULL ||
      !push_alignment_target(analysis, expression->name)) {
    free(entry);
    free(header);
    free(body);
    analysis->failed = true;
    return 1;
  }
  memcpy(entry, locals, count);
  memcpy(header, locals, count);
  target_index = analysis->target_count - 1;
  do {
    memcpy(body, header, count);
    body_reachable = true;
    target = &analysis->targets[target_index];
    target->has_incoming = false;
    result = expression->child_count == 0
                 ? 1
                 : analyze_expression_alignment(analysis, expression->children[0], body, &body_reachable);
    target = &analysis->targets[target_index];
    changed = false;
    for (index = 0; index < count; ++index) {
      uint8_t merged = entry[index];
      if (target->has_incoming && target->incoming[index] < merged)
        merged = target->incoming[index];
      if (merged != header[index]) {
        header[index] = merged;
        changed = true;
      }
    }
  } while (changed && !analysis->failed);
  if (body_reachable)
    memcpy(locals, body, count);
  *reachable = body_reachable;
  pop_alignment_target(analysis);
  free(entry);
  free(header);
  free(body);
  return result;
}

/* Records flow-sensitive byte alignment while simulating Wasm evaluation
 * order. Structured targets preserve facts on early branches and loop edges. */
static uint8_t analyze_expression_alignment(AlignmentAnalysis *analysis, WasmExpr *expression,
                                            uint8_t *locals, bool *reachable) {
  Context *context = analysis->context;
  size_t index, count = context->local_alignment_count;
  uint8_t result = 1, left = 1, right = 1;

  if (!*reachable) {
    expression->guaranteed_alignment = 1;
    return 1;
  }
  switch (expression->kind) {
  case WASM_EXPR_I32_CONST:
    result = constant_word_alignment((uint32_t)expression->i32_value);
    break;
  case WASM_EXPR_LOCAL_GET:
    result = expression->index < count ? locals[expression->index] : 1;
    break;
  case WASM_EXPR_LOCAL_SET:
    if (expression->child_count != 0)
      result = analyze_expression_alignment(analysis, expression->children[0], locals, reachable);
    if (expression->index < count && local_value_type(context->function, expression->index) == WASM_VALUE_I32)
      locals[expression->index] = result;
    if (!expression->is_tee)
      result = 1;
    break;
  case WASM_EXPR_BINARY:
    if (expression->child_count == 2) {
      left = analyze_expression_alignment(analysis, expression->children[0], locals, reachable);
      right = analyze_expression_alignment(analysis, expression->children[1], locals, reachable);
      result = binary_result_alignment(expression, left, right);
    }
    break;
  case WASM_EXPR_SELECT:
    if (expression->child_count == 3) {
      left = analyze_expression_alignment(analysis, expression->children[0], locals, reachable);
      right = analyze_expression_alignment(analysis, expression->children[1], locals, reachable);
      analyze_expression_alignment(analysis, expression->children[2], locals, reachable);
      result = left < right ? left : right;
    }
    break;
  case WASM_EXPR_IF: {
    uint8_t *true_locals = malloc(count == 0 ? 1 : count);
    uint8_t *false_locals = malloc(count == 0 ? 1 : count);
    bool true_reachable = true, false_reachable = true;
    if (true_locals == NULL || false_locals == NULL || expression->child_count == 0) {
      free(true_locals);
      free(false_locals);
      analysis->failed = true;
      break;
    }
    analyze_expression_alignment(analysis, expression->children[0], locals, reachable);
    if (!*reachable) {
      free(true_locals);
      free(false_locals);
      break;
    }
    memcpy(true_locals, locals, count);
    memcpy(false_locals, locals, count);
    left = expression->child_count > 1
               ? analyze_expression_alignment(analysis, expression->children[1], true_locals, &true_reachable)
               : 1;
    right = expression->child_count > 2
                ? analyze_expression_alignment(analysis, expression->children[2], false_locals, &false_reachable)
                : 1;
    if (true_reachable && false_reachable) {
      for (index = 0; index < count; ++index)
        locals[index] = true_locals[index] < false_locals[index] ? true_locals[index] : false_locals[index];
    } else if (true_reachable)
      memcpy(locals, true_locals, count);
    else if (false_reachable)
      memcpy(locals, false_locals, count);
    *reachable = true_reachable || false_reachable;
    result = expression->child_count == 3
                 ? (!true_reachable ? right : !false_reachable ? left : left < right ? left : right)
                 : 1;
    free(true_locals);
    free(false_locals);
    break;
  }
  case WASM_EXPR_BLOCK:
    result = analyze_block_alignment(analysis, expression, locals, reachable);
    break;
  case WASM_EXPR_LOOP:
    result = analyze_loop_alignment(analysis, expression, locals, reachable);
    break;
  case WASM_EXPR_BR:
  case WASM_EXPR_BR_IF:
    for (index = 0; index < expression->child_count; ++index)
      analyze_expression_alignment(analysis, expression->children[index], locals, reachable);
    branch_alignment_to(analysis, expression->name, locals);
    if (expression->kind == WASM_EXPR_BR)
      *reachable = false;
    break;
  case WASM_EXPR_BR_TABLE:
    for (index = 0; index < expression->child_count; ++index)
      analyze_expression_alignment(analysis, expression->children[index], locals, reachable);
    for (index = 0; index < expression->branch_target_count; ++index)
      branch_alignment_to(analysis, expression->branch_targets[index], locals);
    *reachable = false;
    break;
  case WASM_EXPR_RETURN:
    for (index = 0; index < expression->child_count; ++index)
      analyze_expression_alignment(analysis, expression->children[index], locals, reachable);
    *reachable = false;
    break;
  case WASM_EXPR_UNREACHABLE:
    *reachable = false;
    break;
  default:
    for (index = 0; index < expression->child_count; ++index)
      analyze_expression_alignment(analysis, expression->children[index], locals, reachable);
    break;
  }
  expression->guaranteed_alignment = result;
  return result;
}

/* Initializes flow-sensitive alignment facts for one function. Parameters are
 * unconstrained, while Wasm's zero-initialized locals begin word-aligned. */
static bool initialize_local_alignments(Context *context) {
  AlignmentAnalysis analysis = {.context = context};
  size_t count = context->function->param_count + context->function->local_count;
  size_t index;
  bool reachable = true;

  context->local_alignments = malloc(count == 0 ? 1 : count);
  context->local_address_bases = malloc((count == 0 ? 1 : count) * sizeof(*context->local_address_bases));
  context->local_address_offsets = malloc((count == 0 ? 1 : count) * sizeof(*context->local_address_offsets));
  if (context->local_alignments == NULL || context->local_address_bases == NULL ||
      context->local_address_offsets == NULL) {
    diagnostics_error(context->diagnostics, "out of memory analyzing local alignment");
    return false;
  }
  context->local_alignment_count = count;
  for (index = 0; index < count; ++index) {
    context->local_alignments[index] = index < context->function->param_count ? 1 : 4;
    context->local_address_bases[index] = (uint32_t)index;
    context->local_address_offsets[index] = 0;
  }
  analyze_expression_alignment(&analysis, (WasmExpr *)context->function->body,
                               context->local_alignments, &reachable);
  if (analysis.failed)
    diagnostics_error(context->diagnostics, "out of memory analyzing structured alignment");
  free(analysis.targets);
  return !analysis.failed;
}

/* Returns whether adding the static memarg offset always yields a word-aligned
 * Wasm byte address. The align immediate is only an optimization hint. */
static bool word_access_is_proven_aligned(const Context *context, const WasmExpr *pointer, uint32_t offset) {
  (void)context;
  return pointer->guaranteed_alignment >= 4 && (offset & 3u) == 0;
}

/* Returns the high target word of a validated two-word i64 local. */
static int i64_local_high_slot(const WasmFunction *function, uint32_t index) { return local_slot(function, index) - 1; }
/* Converts a function pointer within module storage to its Wasm index. */
static size_t function_index(const WasmModule *module, const WasmFunction *function) {
  return (size_t)(function - module->functions);
}
/* Produces the generated assembly label for one defined Wasm function. */
static void function_label(const WasmModule *module, const WasmFunction *function, char *out, size_t size) {
  snprintf(out, size, "__wasm_function_%zu", function_index(module, function));
}

static bool lower_expression(Context *context, const WasmExpr *expression, Value *value);
static bool lower_expression_impl(Context *context, const WasmExpr *expression, Value *value);
static bool emit_i32_shr_s(Context *context);
static bool emit_i32_extend_s(Context *context, unsigned bits);

#define LOOP_ACCESS_GROUP_LIMIT 8u

/* One syntactically affine load group discovered while inspecting a loop. */
typedef struct LoopAccessGroup {
  uint32_t base_local, minimum_offset, maximum_end, access_count;
  bool all_word_aligned;
} LoopAccessGroup;

/* Conservative scratch state for recognizing a versionable affine loop. */
typedef struct LoopScan {
  const Context *context;
  const char *loop_name;
  uint32_t *assignment_counts;
  uint32_t *strides;
  bool *stride_assignments;
  size_t local_count;
  LoopAccessGroup groups[LOOP_ACCESS_GROUP_LIMIT];
  size_t group_count;
  bool rejected;
  bool has_store;
  bool has_backedge;
  uint32_t counter_local;
  LoopTripKind trip_kind;
  LoopBoundKind bound_kind;
  uint32_t bound_value;
} LoopScan;

/* Recognizes local.get plus an optional nonnegative constant byte offset. */
static bool syntactic_affine_local(const WasmExpr *expression, uint32_t *local, uint32_t *offset) {
  uint64_t sum;
  uint32_t child_local, child_offset;

  if (expression->kind == WASM_EXPR_LOCAL_GET) {
    *local = expression->index;
    *offset = 0;
    return true;
  }
  if (expression->kind == WASM_EXPR_LOCAL_SET && expression->is_tee && expression->child_count == 1)
    return syntactic_affine_local(expression->children[0], local, offset);
  if (expression->kind != WASM_EXPR_BINARY || expression->binary_op != WASM_BINARY_ADD ||
      expression->child_count != 2)
    return false;
  if (syntactic_affine_local(expression->children[0], &child_local, &child_offset) &&
      expression->children[1]->kind == WASM_EXPR_I32_CONST && expression->children[1]->i32_value >= 0) {
    sum = (uint64_t)child_offset + (uint32_t)expression->children[1]->i32_value;
  } else if (expression->children[0]->kind == WASM_EXPR_I32_CONST &&
             expression->children[0]->i32_value >= 0 &&
             syntactic_affine_local(expression->children[1], &child_local, &child_offset)) {
    sum = (uint64_t)child_offset + (uint32_t)expression->children[0]->i32_value;
  } else
    return false;
  if (sum > UINT32_MAX)
    return false;
  *local = child_local;
  *offset = (uint32_t)sum;
  return true;
}

/* Recognizes the side-effect-free subset of affine pointer expressions. This
 * deliberately excludes local.tee: a cached target operand may replace the
 * address calculation, but it must never remove a Wasm local assignment. */
static bool syntactic_pure_affine_local(const WasmExpr *expression, uint32_t *local, uint32_t *offset) {
  uint64_t sum;
  uint32_t child_local, child_offset;

  if (expression->kind == WASM_EXPR_LOCAL_GET) {
    *local = expression->index;
    *offset = 0;
    return true;
  }
  if (expression->kind != WASM_EXPR_BINARY || expression->binary_op != WASM_BINARY_ADD ||
      expression->child_count != 2)
    return false;
  if (syntactic_pure_affine_local(expression->children[0], &child_local, &child_offset) &&
      expression->children[1]->kind == WASM_EXPR_I32_CONST && expression->children[1]->i32_value >= 0) {
    sum = (uint64_t)child_offset + (uint32_t)expression->children[1]->i32_value;
  } else if (expression->children[0]->kind == WASM_EXPR_I32_CONST &&
             expression->children[0]->i32_value >= 0 &&
             syntactic_pure_affine_local(expression->children[1], &child_local, &child_offset)) {
    sum = (uint64_t)child_offset + (uint32_t)expression->children[0]->i32_value;
  } else
    return false;
  if (sum > UINT32_MAX)
    return false;
  *local = child_local;
  *offset = (uint32_t)sum;
  return true;
}

/* Recognizes the positive constant update local = local + stride. */
static bool syntactic_positive_local_stride(const WasmExpr *expression, uint32_t local, uint32_t *stride) {
  const WasmExpr *left, *right;
  if (expression->kind != WASM_EXPR_BINARY || expression->binary_op != WASM_BINARY_ADD ||
      expression->child_count != 2)
    return false;
  left = expression->children[0];
  right = expression->children[1];
  if (left->kind == WASM_EXPR_I32_CONST) {
    const WasmExpr *swap = left;
    left = right;
    right = swap;
  }
  if (left->kind != WASM_EXPR_LOCAL_GET || left->index != local || right->kind != WASM_EXPR_I32_CONST ||
      right->i32_value <= 0)
    return false;
  *stride = (uint32_t)right->i32_value;
  return true;
}

/* Accepts a loop-limit expression that can be read without introducing a new
 * trap. Constant-address memory is allowed only when it is aligned and known
 * to be inside the fixed initial memory. */
static bool parse_loop_bound(const Context *context, const WasmExpr *expression,
                             LoopBoundKind *kind, uint32_t *value) {
  uint64_t address;
  if (expression->kind == WASM_EXPR_I32_CONST) {
    *kind = LOOP_BOUND_CONSTANT;
    *value = (uint32_t)expression->i32_value;
    return true;
  }
  if (expression->kind == WASM_EXPR_LOCAL_GET &&
      local_value_type(context->function, expression->index) == WASM_VALUE_I32) {
    *kind = LOOP_BOUND_LOCAL;
    *value = expression->index;
    return true;
  }
  if (expression->kind != WASM_EXPR_LOAD || expression->bytes != 4 || expression->child_count != 1 ||
      expression->children[0]->kind != WASM_EXPR_I32_CONST)
    return false;
  address = (uint64_t)(uint32_t)expression->children[0]->i32_value + expression->offset;
  if ((address & 3u) != 0 || address > context->memory_bytes || 4u > (uint64_t)context->memory_bytes - address)
    return false;
  *kind = LOOP_BOUND_ALIGNED_MEMORY;
  *value = (uint32_t)address;
  return true;
}

/* Recognizes the two canonical counted backedges currently versioned: a
 * forward index compared with a stable limit, or a positive counter reduced
 * by one until zero. */
static bool parse_counted_backedge(const LoopScan *scan, const WasmExpr *branch,
                                   uint32_t *counter_local, LoopTripKind *trip_kind,
                                   LoopBoundKind *bound_kind, uint32_t *bound_value) {
  const WasmExpr *condition, *increment, *left, *right;
  uint32_t stride;
  if (branch->kind != WASM_EXPR_BR_IF || branch->name == NULL || scan->loop_name == NULL ||
      strcmp(branch->name, scan->loop_name) != 0 || branch->child_count == 0)
    return false;
  condition = branch->children[branch->child_count - 1];
  if (condition->kind == WASM_EXPR_LOCAL_SET && condition->is_tee && condition->child_count == 1) {
    increment = condition->children[0];
    if (increment->kind != WASM_EXPR_BINARY || increment->binary_op != WASM_BINARY_ADD ||
        increment->child_count != 2)
      return false;
    left = increment->children[0];
    right = increment->children[1];
    if (left->kind == WASM_EXPR_I32_CONST) {
      const WasmExpr *swap = left;
      left = right;
      right = swap;
    }
    if (left->kind != WASM_EXPR_LOCAL_GET || left->index != condition->index ||
        right->kind != WASM_EXPR_I32_CONST || right->i32_value != -1)
      return false;
    *counter_local = condition->index;
    *trip_kind = LOOP_TRIP_COUNTDOWN;
    *bound_kind = LOOP_BOUND_NONE;
    *bound_value = 0;
    return true;
  }
  if (condition->kind != WASM_EXPR_BINARY || condition->binary_op != WASM_BINARY_LT_S ||
      condition->child_count != 2 || condition->children[0]->kind != WASM_EXPR_LOCAL_SET ||
      !condition->children[0]->is_tee || condition->children[0]->child_count != 1)
    return false;
  *counter_local = condition->children[0]->index;
  increment = condition->children[0]->children[0];
  if (!syntactic_positive_local_stride(increment, *counter_local, &stride) || stride != 1)
    return false;
  *trip_kind = LOOP_TRIP_INCREMENT_TO_LIMIT;
  return parse_loop_bound(scan->context, condition->children[1], bound_kind, bound_value);
}

/* Adds one direct affine load/store to a bounded set of candidate base locals. */
static void record_loop_access(LoopScan *scan, const WasmExpr *expression) {
  uint32_t local, pointer_offset;
  uint64_t start, end;
  size_t index;
  LoopAccessGroup *group = NULL;

  if (expression->child_count == 0)
    return;
  if (!syntactic_affine_local(expression->children[0], &local, &pointer_offset)) {
    return;
  }
  start = (uint64_t)pointer_offset + expression->offset;
  end = start + expression->bytes;
  if (start > UINT32_MAX || end > UINT32_MAX)
    return;
  for (index = 0; index < scan->group_count; ++index)
    if (scan->groups[index].base_local == local) {
      group = &scan->groups[index];
      break;
    }
  if (group == NULL) {
    if (scan->group_count == LOOP_ACCESS_GROUP_LIMIT)
      return;
    group = &scan->groups[scan->group_count++];
    *group = (LoopAccessGroup){.base_local = local,
                               .minimum_offset = (uint32_t)start,
                               .maximum_end = (uint32_t)end,
                               .all_word_aligned = true};
  }
  if (start < group->minimum_offset)
    group->minimum_offset = (uint32_t)start;
  if (end > group->maximum_end)
    group->maximum_end = (uint32_t)end;
  if (expression->bytes != 4 || (start & 3u) != 0)
    group->all_word_aligned = false;
  ++group->access_count;
}

/* Walks one loop body without interpreting arbitrary control flow. Rejection
 * is deliberately conservative: the first implementation accepts only
 * affine loads/stores, imported hardware calls, and no nested loops. */
static void scan_versionable_loop(LoopScan *scan, const WasmExpr *expression) {
  size_t index;
  if (scan->rejected)
    return;
  if (expression->kind == WASM_EXPR_LOOP) {
    scan->rejected = true;
    return;
  }
  if (expression->kind == WASM_EXPR_I64_CONST_STORE || expression->kind == WASM_EXPR_I64_LOAD_STORE ||
      expression->kind == WASM_EXPR_I64_LOAD_STORE_LOCAL_TEE ||
      expression->kind == WASM_EXPR_I64_PACKED_I32_STORE || expression->kind == WASM_EXPR_MEMORY_COPY ||
      expression->kind == WASM_EXPR_MEMORY_FILL || expression->kind == WASM_EXPR_MEMORY_GROW) {
    scan->rejected = true;
    return;
  }
  if (expression->kind == WASM_EXPR_CALL_INDIRECT) {
    scan->rejected = true;
    return;
  }
  if (expression->kind == WASM_EXPR_CALL) {
    const WasmFunction *callee = wasm_module_find_function(scan->context->validated->module, expression->name);
    if (callee == NULL || !callee->is_import) {
      scan->rejected = true;
      return;
    }
  }
  if ((expression->kind == WASM_EXPR_BR && expression->name != NULL && scan->loop_name != NULL &&
       strcmp(expression->name, scan->loop_name) == 0) ||
      (expression->kind == WASM_EXPR_BR_TABLE && scan->loop_name != NULL)) {
    size_t target_index;
    if (expression->kind == WASM_EXPR_BR ||
        (expression->name != NULL && strcmp(expression->name, scan->loop_name) == 0)) {
      scan->rejected = true;
      return;
    }
    for (target_index = 0; target_index < expression->branch_target_count; ++target_index)
      if (strcmp(expression->branch_targets[target_index], scan->loop_name) == 0) {
        scan->rejected = true;
        return;
      }
  }
  if (expression->kind == WASM_EXPR_LOAD || expression->kind == WASM_EXPR_STORE) {
    record_loop_access(scan, expression);
    if (expression->kind == WASM_EXPR_STORE)
      scan->has_store = true;
  }
  if (expression->kind == WASM_EXPR_LOCAL_SET && expression->index < scan->local_count) {
    uint32_t stride;
    ++scan->assignment_counts[expression->index];
    if (expression->child_count == 1 &&
        syntactic_positive_local_stride(expression->children[0], expression->index, &stride)) {
      scan->strides[expression->index] = stride;
      scan->stride_assignments[expression->index] = true;
    }
  }
  if (expression->kind == WASM_EXPR_BR_IF && expression->name != NULL && scan->loop_name != NULL &&
      strcmp(expression->name, scan->loop_name) == 0) {
    uint32_t counter;
    LoopTripKind trip_kind;
    LoopBoundKind bound_kind;
    uint32_t bound_value;
    if (scan->has_backedge ||
        !parse_counted_backedge(scan, expression, &counter, &trip_kind, &bound_kind, &bound_value)) {
      scan->rejected = true;
      return;
    }
    scan->has_backedge = true;
    scan->counter_local = counter;
    scan->trip_kind = trip_kind;
    scan->bound_kind = bound_kind;
    scan->bound_value = bound_value;
  }
  for (index = 0; index < expression->child_count; ++index)
    scan_versionable_loop(scan, expression->children[index]);
}

/* Returns whether one table target has the exact signature declared by an
 * indirect call site. Validation has already checked supported scalar types. */
static bool indirect_target_matches(const WasmFunction *function, const WasmExpr *call) {
  size_t index;
  if (function->param_count != call->signature_param_count || function->result != call->signature_result)
    return false;
  for (index = 0; index < function->param_count; ++index)
    if (function->params[index] != call->signature_params[index])
      return false;
  return true;
}

/* Lowers an immutable Wasm function-table call into checked direct-call
 * cases. Arguments and the selector are each evaluated exactly once in Wasm
 * order. Out-of-range, null, and signature-mismatched slots share the normal
 * trap path; matching slots retain the ordinary VirconWasm stack ABI. */
static bool lower_call_indirect(Context *context, const WasmExpr *expression, Value *value) {
  const WasmTable *table = wasm_module_find_table(context->validated->module, expression->name);
  Value inline_arguments[INLINE_CALL_ARGUMENTS] = {{0}};
  Value *arguments = inline_arguments;
  Value selector = {0};
  bool heap_arguments = false;
  char done[64];
  size_t index;

  if (table == NULL)
    return false;
  if (expression->signature_param_count > INLINE_CALL_ARGUMENTS) {
    arguments = calloc(expression->signature_param_count, sizeof(*arguments));
    if (arguments == NULL) {
      diagnostics_error(context->diagnostics, "out of memory lowering %zu indirect-call arguments",
                        expression->signature_param_count);
      return false;
    }
    heap_arguments = true;
  }
  for (index = 0; index < expression->signature_param_count; ++index)
    if (!lower_expression(context, expression->children[index], &arguments[index]) || !arguments[index].present ||
        !materialize_value(context, &arguments[index]))
      goto fail;
  if (!lower_expression(context, expression->children[expression->signature_param_count], &selector) ||
      !selector.present || !materialize_value(context, &selector))
    goto fail;

  for (index = 0; index < expression->signature_param_count; ++index)
    if (!load_value(context, 1, arguments[index]) || !emit(context, "  mov [SP+%zu], R1", index))
      goto fail;
  if (!load_value(context, 3, selector))
    goto fail;
  release(context, selector);
  for (index = expression->signature_param_count; index != 0; --index)
    release(context, arguments[index - 1]);

  /* Bias both operands so Vircon32's signed comparison implements Wasm's
   * unsigned table-index range check without wraparound. */
  if (!emit(context, "  mov R1, R3") || !emit(context, "  xor R1, 0x80000000") ||
      !emit(context, "  ilt R1, 0x%08X", table->initial ^ UINT32_C(0x80000000)) ||
      !emit(context, "  jf R1, __wasm_trap") || !fresh_label(context, "call_indirect_done", done, sizeof(done)))
    goto fail_released;

  for (index = 0; index < table->initial; ++index) {
    const WasmFunction *callee;
    char next_case[64], callee_label[64];
    if (table->slots[index] == NULL)
      continue;
    callee = wasm_module_find_function(context->validated->module, table->slots[index]);
    if (callee == NULL || !indirect_target_matches(callee, expression))
      continue;
    function_label(context->validated->module, callee, callee_label, sizeof(callee_label));
    if (!fresh_label(context, "call_indirect_next", next_case, sizeof(next_case)) ||
        !emit(context, "  mov R1, R3") || !emit(context, "  ieq R1, %zu", index) ||
        !emit(context, "  jf R1, %s", next_case) || !emit(context, "  call %s", callee_label) ||
        !emit(context, "  jmp %s", done) || !emit_label(context, next_case))
      goto fail_released;
  }
  if (!emit(context, "  jmp __wasm_trap") || !emit_label(context, done))
    goto fail_released;
  context->target_word_address_cached = false;
  if (expression->signature_result == WASM_VALUE_I32 || expression->signature_result == WASM_VALUE_F32) {
    int slot = temp_slot(context);
    if (slot == 0 || !store_slot(context, slot, 0))
      goto fail_released;
    value->slot = slot;
    value->type = expression->signature_result;
    value->present = true;
  } else {
    value->present = false;
  }
  if (heap_arguments)
    free(arguments);
  return true;

fail:
  release(context, selector);
  for (index = expression->signature_param_count; index != 0; --index)
    release(context, arguments[index - 1]);
fail_released:
  if (heap_arguments)
    free(arguments);
  return false;
}

/* Builds the narrow affine-loop plan used by the guarded fast/checked copies. */
static LoopFastPath analyze_loop_fast_path(const Context *context, const WasmExpr *loop) {
  LoopFastPath plan = {0};
  LoopScan scan = {.context = context, .loop_name = loop->name};
  size_t index, best = SIZE_MAX;

  if (context->memory_can_grow || loop->name == NULL || loop->child_count != 1)
    return plan;
  scan.local_count = context->function->param_count + context->function->local_count;
  scan.assignment_counts = calloc(scan.local_count == 0 ? 1 : scan.local_count,
                                  sizeof(*scan.assignment_counts));
  scan.strides = calloc(scan.local_count == 0 ? 1 : scan.local_count, sizeof(*scan.strides));
  scan.stride_assignments = calloc(scan.local_count == 0 ? 1 : scan.local_count,
                                   sizeof(*scan.stride_assignments));
  if (scan.assignment_counts == NULL || scan.strides == NULL || scan.stride_assignments == NULL)
    goto done;
  scan_versionable_loop(&scan, loop->children[0]);
  if (scan.rejected || !scan.has_backedge || scan.counter_local >= scan.local_count ||
      scan.assignment_counts[scan.counter_local] != 1)
    goto done;
  /* A linear-memory limit is stable across imported hardware calls, but an
   * arbitrary store in the same loop could alias it. Countdown loops and
   * local/constant limits do not have that ambiguity. */
  if (scan.has_store && scan.bound_kind == LOOP_BOUND_ALIGNED_MEMORY)
    goto done;
  if (scan.bound_kind == LOOP_BOUND_LOCAL &&
      (scan.bound_value >= scan.local_count || scan.assignment_counts[scan.bound_value] != 0))
    goto done;
  for (index = 0; index < scan.group_count; ++index) {
    const LoopAccessGroup *group = &scan.groups[index];
    if (group->access_count < 2 || group->base_local >= scan.local_count ||
        group->base_local == scan.counter_local || scan.assignment_counts[group->base_local] != 1 ||
        !scan.stride_assignments[group->base_local] || scan.strides[group->base_local] == 0 ||
        scan.strides[group->base_local] > INT32_MAX ||
        local_value_type(context->function, group->base_local) != WASM_VALUE_I32 ||
        local_value_type(context->function, scan.counter_local) != WASM_VALUE_I32 ||
        group->maximum_end > context->memory_bytes)
      continue;
    if (best == SIZE_MAX || group->access_count > scan.groups[best].access_count)
      best = index;
  }
  if (best != SIZE_MAX) {
    const LoopAccessGroup *group = &scan.groups[best];
    plan.valid = true;
    plan.base_local = group->base_local;
    plan.counter_local = scan.counter_local;
    plan.stride = scan.strides[group->base_local];
    plan.minimum_offset = group->minimum_offset;
    plan.maximum_end = group->maximum_end;
    plan.trip_kind = scan.trip_kind;
    plan.bound_kind = scan.bound_kind;
    plan.bound_value = scan.bound_value;
    plan.use_word_pointer = group->all_word_aligned && (plan.stride & 3u) == 0 &&
                            context->local_alignments[plan.base_local] >= 4;
  }
done:
  free(scan.assignment_counts);
  free(scan.strides);
  free(scan.stride_assignments);
  return plan;
}

/* Reads one accepted loop bound into a target register. */
static bool emit_loop_bound(Context *context, const LoopFastPath *plan, int reg) {
  switch (plan->bound_kind) {
  case LOOP_BOUND_CONSTANT:
    return emit(context, "  mov R%d, 0x%08X", reg, plan->bound_value);
  case LOOP_BOUND_LOCAL:
    return load_slot(context, reg, local_slot(context->function, plan->bound_value));
  case LOOP_BOUND_ALIGNED_MEMORY:
    return emit(context, "  mov R%d, [%u]", reg, LINEAR_BASE + plan->bound_value / 4u);
  case LOOP_BOUND_NONE:
    break;
  }
  return false;
}

/* Emits a non-trapping loop-version guard. Failure selects the original
 * checked loop; success proves every planned affine access in every iteration.
 * Keeping the checked fallback is what preserves Wasm's exact trap order. */
static bool emit_loop_fast_guard(Context *context, const LoopFastPath *plan, const char *checked_label) {
  uint32_t maximum_base = context->memory_bytes - plan->maximum_end;
  int counter_slot = local_slot(context->function, plan->counter_local);
  int base_slot = local_slot(context->function, plan->base_local);

  if (!load_slot(context, 1, counter_slot))
    return false;
  if (plan->trip_kind == LOOP_TRIP_COUNTDOWN) {
    if (!emit(context, "  mov R3, R1") || !emit(context, "  igt R1, 0") ||
        !emit(context, "  jf R1, %s", checked_label))
      return false;
  } else if (plan->trip_kind == LOOP_TRIP_INCREMENT_TO_LIMIT) {
    if (!emit(context, "  mov R2, R1") || !emit(context, "  ilt R2, 0") ||
        !emit(context, "  jt R2, %s", checked_label) || !emit_loop_bound(context, plan, 3) ||
        !emit(context, "  mov R10, R3") ||
        !emit(context, "  mov R2, R3") || !emit(context, "  igt R2, R1") ||
        !emit(context, "  jf R2, %s", checked_label) || !emit(context, "  isub R3, R1"))
      return false;
  } else
    return false;

  return load_slot(context, 2, base_slot) && emit(context, "  mov R4, R2") &&
         emit(context, "  xor R4, 0x80000000") &&
         emit(context, "  igt R4, 0x%08X", maximum_base ^ 0x80000000u) &&
         emit(context, "  jt R4, %s", checked_label) && emit(context, "  mov R4, %u", maximum_base) &&
         emit(context, "  isub R4, R2") && emit(context, "  idiv R4, %u", plan->stride) &&
         emit(context, "  isub R3, 1") && emit(context, "  igt R3, R4") &&
         emit(context, "  jt R3, %s", checked_label);
}

/* Emits one copy of a structured loop body, selecting whether the surrounding
 * runtime guard permits its affine memory accesses to bypass bounds checks. */
static bool lower_loop_copy(Context *context, const WasmExpr *loop, const LoopFastPath *plan,
                            const char *label, bool fast) {
  Value body_value = {0};
  Target target;
  bool success;

  if (!push_target(context, loop->name, label, 0))
    return false;
  if (!emit_label(context, label)) {
    target = pop_target(context);
    free(target.label);
    return false;
  }
  context->fast_memory_active = fast;
  if (fast) {
    context->fast_memory_base_local = plan->base_local;
    context->fast_memory_minimum_offset = plan->minimum_offset;
    context->fast_memory_maximum_end = plan->maximum_end;
    context->fast_word_pointer_active = plan->use_word_pointer;
    context->fast_word_pointer_base_local = plan->base_local;
    context->fast_word_pointer_stride = plan->stride / 4u;
    context->fast_bound_active = plan->trip_kind == LOOP_TRIP_INCREMENT_TO_LIMIT;
    context->fast_bound_kind = plan->bound_kind;
    context->fast_bound_value = plan->bound_value;
  }
  success = lower_expression(context, loop->children[0], &body_value);
  context->fast_memory_active = false;
  context->fast_word_pointer_active = false;
  context->fast_bound_active = false;
  release(context, body_value);
  target = pop_target(context);
  free(target.label);
  return success;
}

/* Duplicates one recognized loop behind a safe span guard. The normal checked
 * copy remains the semantic fallback for every unusual or invalid runtime
 * state, while the common valid path uses native aligned memory operations. */
static bool lower_versioned_loop(Context *context, const WasmExpr *loop,
                                 const LoopFastPath *plan, Value *value) {
  char fast_label[64], checked_label[64], join_label[64];
  unsigned entry_temp_depth = context->temp_depth;

  if (!fresh_label(context, "loop_fast", fast_label, sizeof(fast_label)) ||
      !fresh_label(context, "loop_checked", checked_label, sizeof(checked_label)) ||
      !fresh_label(context, "loop_join", join_label, sizeof(join_label)) ||
      !emit_loop_fast_guard(context, plan, checked_label) ||
      (plan->use_word_pointer &&
       (!emit(context, "  shl R2, -2") || !emit(context, "  iadd R2, %u", LINEAR_BASE) ||
        !emit(context, "  mov R13, R2"))) ||
      !lower_loop_copy(context, loop, plan, fast_label, true) ||
      !emit(context, "  jmp %s", join_label))
    return false;

  /* Both copies start with identical Wasm locals. Compiler-owned temporary
   * slots may therefore be reused even though only one copy executes. */
  context->temp_depth = entry_temp_depth;
  if (!lower_loop_copy(context, loop, plan, checked_label, false) || !emit_label(context, join_label))
    return false;
  value->present = false;
  return true;
}

/* Lowers a structured if and merges an optional one-word result from its two arms. */
static bool lower_if(Context *context, const WasmExpr *expression, Value *value) {
  Value condition = {0}, arm = {0}, result = {0};
  char else_label[64], end_label[64];
  bool has_result = expression->value_type == WASM_VALUE_I32 || expression->value_type == WASM_VALUE_F32;
  int previous_result_register = context->preferred_result_register;

  context->preferred_result_register = 1;
  bool lowered_condition = lower_expression(context, expression->children[0], &condition);
  context->preferred_result_register = previous_result_register;
  if (!lowered_condition || !condition.present ||
      !fresh_label(context, "if_end", end_label, sizeof(end_label)) || !load_value(context, 1, condition))
    return false;
  release(context, condition);

  if (has_result) {
    result.slot = temp_slot(context);
    result.type = expression->value_type;
    result.present = result.slot != 0;
    if (!result.present)
      return false;
  }

  if (expression->child_count == 3) {
    if (!fresh_label(context, "if_else", else_label, sizeof(else_label)) ||
        !emit(context, "  jf R1, %s", else_label) ||
        !lower_expression(context, expression->children[1], &arm))
      return false;
    if (has_result && arm.present && (!load_value(context, 1, arm) || !store_slot(context, result.slot, 1)))
      return false;
    release(context, arm);
    arm.present = false;
    if (!emit(context, "  jmp %s", end_label) || !emit_label(context, else_label) ||
        !lower_expression(context, expression->children[2], &arm))
      return false;
    if (has_result && arm.present && (!load_value(context, 1, arm) || !store_slot(context, result.slot, 1)))
      return false;
    release(context, arm);
    if (!emit_label(context, end_label))
      return false;
  } else {
    if (!emit(context, "  jf R1, %s", end_label) ||
        !lower_expression(context, expression->children[1], &arm))
      return false;
    release(context, arm);
    if (!emit_label(context, end_label))
      return false;
  }

  *value = result;
  return true;
}

/* Lowers a resultless Wasm br_table through an immutable V32 ROM address table.
 */
static bool lower_br_table(Context *context, const WasmExpr *expression, Value *value) {
  Value selector = {0};
  const char *default_target;
  char table_label[64];
  uint32_t table_count;

  if (!lower_expression(context, expression->children[0], &selector) || !selector.present)
    return false;
  default_target = find_target_label(context, expression->name);
  if (default_target == NULL) {
    diagnostics_error(context->diagnostics,
                      "br_table default targets '%s' outside active "
                      "structured control",
                      expression->name);
    return false;
  }
  if (expression->branch_target_count == 0) {
    release(context, selector);
    value->present = false;
    return emit(context, "  jmp %s", default_target);
  }
  if (expression->branch_target_count > UINT32_MAX ||
      !fresh_label(context, "br_table", table_label, sizeof(table_label)) ||
      !record_jump_table(context, table_label, expression) || !load_value(context, 2, selector))
    return false;
  table_count = (uint32_t)expression->branch_target_count;
  release(context, selector);
  value->present = false;

  /* Wasm selects a table entry only for unsigned selector values below the
   * table length. Biasing changes that unsigned order into Vircon's signed
   * ILT comparison; all remaining values take the default branch. */
  return emit(context, "  mov R1, R2") && emit(context, "  xor R1, 0x80000000") &&
         emit(context, "  mov R3, 0x%08X", table_count) && emit(context, "  xor R3, 0x80000000") &&
         emit(context, "  ilt R1, R3") && emit(context, "  jf R1, %s", default_target) &&
         emit(context, "  mov R3, %s", table_label) && emit(context, "  iadd R3, R2") &&
         emit(context, "  mov R4, [R3]") && emit(context, "  jmp R4");
}

/* Checks a Wasm byte address against the current dynamic length and leaves
 * its effective byte address in R2. Valid target sizes are below 2^31 bytes,
 * so a negative signed value identifies every out-of-range unsigned address. */
static bool checked_identity_range(Value pointer, uint32_t offset, uint32_t width,
                                   uint32_t *start, uint64_t *end) {
  uint64_t range_start;

  if (!pointer.has_address_identity)
    return false;
  range_start = (uint64_t)pointer.address_offset + offset;
  if (range_start > UINT32_MAX || range_start + width > (uint64_t)UINT32_MAX + 1u)
    return false;
  *start = (uint32_t)range_start;
  *end = range_start + width;
  return true;
}

/* Reports whether an earlier successful check proves the requested range. */
static bool checked_address_is_reusable(const Context *context, Value pointer, uint32_t offset, uint32_t width) {
  size_t index;
  uint32_t identity_offset;
  uint32_t range_start = 0;
  uint64_t range_end = 0;
  bool range_is_mergeable;

  if (!pointer.has_address_identity)
    return false;
  identity_offset = pointer.address_offset + offset;
  range_is_mergeable = checked_identity_range(pointer, offset, width, &range_start, &range_end);
  for (index = 0; index < context->checked_address_count; ++index) {
    const CheckedAddress *checked = &context->checked_addresses[index];
    if (checked->base_local == pointer.address_base_local && checked->offset == identity_offset &&
        checked->width >= width)
      return true;
    if (range_is_mergeable && checked->mergeable && checked->base_local == pointer.address_base_local &&
        range_start >= checked->offset && range_end <= (uint64_t)checked->offset + checked->width)
      return true;
  }
  return false;
}

/* Remembers a successful check. Two checks sharing an affine base may prove
 * the interval between them once their bounded offset distance rules out an
 * intervening i32 wrap. This never moves a trap ahead of either check. */
static void record_checked_address(Context *context, Value pointer, uint32_t offset, uint32_t width) {
  CheckedAddress candidate;
  uint64_t candidate_end = 0;
  size_t index = 0;

  if (!pointer.has_address_identity)
    return;
  candidate.base_local = pointer.address_base_local;
  candidate.offset = pointer.address_offset + offset;
  candidate.width = width;
  candidate.mergeable = checked_identity_range(pointer, offset, width, &candidate.offset, &candidate_end);

  while (candidate.mergeable && index < context->checked_address_count) {
    CheckedAddress *checked = &context->checked_addresses[index];
    uint64_t checked_end = (uint64_t)checked->offset + checked->width;
    uint32_t union_start;
    uint64_t union_end;

    if (!checked->mergeable || checked->base_local != candidate.base_local) {
      ++index;
      continue;
    }
    union_start = checked->offset < candidate.offset ? checked->offset : candidate.offset;
    union_end = checked_end > candidate_end ? checked_end : candidate_end;
    if (union_end - union_start > VIRCON_LINEAR_MEMORY_BYTES) {
      ++index;
      continue;
    }
    candidate.offset = union_start;
    candidate.width = (uint32_t)(union_end - union_start);
    candidate_end = union_end;
    context->checked_addresses[index] = context->checked_addresses[--context->checked_address_count];
  }

  if (context->checked_address_count == sizeof(context->checked_addresses) / sizeof(context->checked_addresses[0]))
    context->checked_address_count = 0;
  context->checked_addresses[context->checked_address_count++] = candidate;
}

/* Reports whether the active loop-version guard proves this affine access for
 * every fast-path iteration. The exclusive end calculation is widened so a
 * wrapping Wasm offset can never become an unchecked target access. */
static bool fast_loop_covers_access(const Context *context, Value pointer, uint32_t offset, uint32_t width) {
  uint64_t start, end;
  if (!context->fast_memory_active || !pointer.has_address_identity ||
      pointer.address_base_local != context->fast_memory_base_local)
    return false;
  start = (uint64_t)pointer.address_offset + offset;
  end = start + width;
  return start >= context->fast_memory_minimum_offset &&
         end <= context->fast_memory_maximum_end;
}

/* Emits or reuses a Wasm bounds check and leaves the effective byte address
 * in R2. Cache entries never cross labels or assignments to their base local. */
static bool effective_address(Context *context, Value pointer, uint32_t offset, uint32_t width,
                              bool materialize_address) {
  uint64_t required = (uint64_t)offset + width;
  bool success;
  if (required > VIRCON_LINEAR_MEMORY_BYTES)
    return emit(context, "  jmp __wasm_trap");
  if (fast_loop_covers_access(context, pointer, offset, width))
    return !materialize_address ||
           (load_value(context, 2, pointer) &&
            (offset == 0 || emit(context, "  iadd R2, 0x%08X", offset)));
  if (pointer.is_immediate) {
    uint64_t address = (uint64_t)pointer.immediate + offset;
    if (address > context->memory_bytes || width > (uint64_t)context->memory_bytes - address)
      return emit(context, "  jmp __wasm_trap");
    return !materialize_address || emit(context, "  mov R2, 0x%08X", (uint32_t)address);
  }
  if (checked_address_is_reusable(context, pointer, offset, width))
    return !materialize_address ||
           (load_value(context, 2, pointer) && (offset == 0 || emit(context, "  iadd R2, 0x%08X", offset)));
  if (!context->memory_can_grow) {
    if (required > context->memory_bytes)
      return emit(context, "  jmp __wasm_trap");
    uint32_t maximum = context->memory_bytes - (uint32_t)required;
    /* Biasing both sides maps unsigned i32 order onto Vircon's signed
     * comparison, including addresses with bit 31 set. */
    success = load_value(context, 2, pointer) && emit(context, "  mov R1, R2") &&
              emit(context, "  xor R1, 0x80000000") &&
              emit(context, "  igt R1, 0x%08X", maximum ^ 0x80000000u) &&
              emit(context, "  jt R1, __wasm_trap") &&
              (offset == 0 || emit(context, "  iadd R2, 0x%08X", offset));
    if (success)
      record_checked_address(context, pointer, offset, width);
    return success;
  }
  success = load_value(context, 2, pointer) && emit(context, "  mov R1, R2") && emit(context, "  ilt R1, 0") &&
            emit(context, "  jt R1, __wasm_trap") &&
            emit(context, "  mov R1, [%u]", VIRCON_WASM_MEMORY_PAGES_WORD) && emit(context, "  imul R1, 65536") &&
            emit(context, "  mov R3, 0x%08X", (uint32_t)required) && emit(context, "  igt R3, R1") &&
            emit(context, "  jt R3, __wasm_trap") && emit(context, "  isub R1, R3") &&
            /* Comparisons write their result into the first operand on
             * Vircon32. Keep R2 intact for the following packed access. */
            emit(context, "  mov R3, R2") && emit(context, "  igt R3, R1") && emit(context, "  jt R3, __wasm_trap") &&
            (offset == 0 || emit(context, "  iadd R2, 0x%08X", offset));
  if (success)
    record_checked_address(context, pointer, offset, width);
  return success;
}
/* Extracts one little-endian Wasm byte at the checked byte address in R2. */
static bool load_byte_at_r2(Context *context, int result) {
  return emit(context, "  mov R3, R2") && emit(context, "  and R3, 3") && emit(context, "  imul R3, -8") &&
         emit(context, "  mov R4, R2") && emit(context, "  mov R5, -2") && emit(context, "  shl R4, R5") &&
         emit(context, "  iadd R4, %u", LINEAR_BASE) && emit(context, "  mov R%d, [R4]", result) &&
         emit(context, "  shl R%d, R3", result) && emit(context, "  and R%d, 0x000000FF", result);
}
/* Replaces one little-endian Wasm byte at the checked byte address in R2. */
static bool store_byte_at_r2(Context *context, int value_register) {
  /* Vircon32 BNOT is logical-not, not a bitwise complement. XOR builds the
   * inverted lane mask required for Wasm's preserving read-modify-write. */
  return emit(context, "  mov R3, R2") && emit(context, "  and R3, 3") && emit(context, "  imul R3, 8") &&
         emit(context, "  mov R4, R2") && emit(context, "  mov R5, -2") && emit(context, "  shl R4, R5") &&
         emit(context, "  iadd R4, %u", LINEAR_BASE) && emit(context, "  mov R5, [R4]") &&
         emit(context, "  mov R6, 0x000000FF") && emit(context, "  shl R6, R3") &&
         emit(context, "  xor R6, 0xFFFFFFFF") && emit(context, "  and R5, R6") &&
         emit(context, "  mov R6, R%d", value_register) && emit(context, "  and R6, 0x000000FF") &&
         emit(context, "  shl R6, R3") && emit(context, "  or R5, R6") && emit(context, "  mov [R4], R5");
}

/* Formats an R13-relative word operand for a cached aligned Wasm address. */
static void cached_word_operand(int32_t displacement, char *operand, size_t size) {
  if (displacement == 0)
    snprintf(operand, size, "[R13]");
  else if (displacement > 0)
    snprintf(operand, size, "[R13+%d]", displacement);
  else
    snprintf(operand, size, "[R13%d]", displacement);
}

/* Returns a cached target-word operand when two aligned Wasm addresses share
 * the same affine local base. The byte-offset difference becomes a target
 * word displacement without changing the Wasm pointer representation. */
static bool find_cached_word_operand(Context *context, Value pointer, uint32_t offset,
                                     char *operand, size_t size) {
  uint32_t effective_offset, byte_difference;
  int32_t signed_difference;
  int32_t word_displacement;

  if (context->fast_word_pointer_active && pointer.has_address_identity &&
      pointer.address_base_local == context->fast_word_pointer_base_local) {
    effective_offset = pointer.address_offset + offset;
    if ((effective_offset & 3u) == 0 &&
        fast_loop_covers_access(context, pointer, offset, 4)) {
      cached_word_operand((int32_t)(effective_offset / 4u), operand, size);
      return true;
    }
  }
  if (!context->target_word_address_cached || !pointer.has_address_identity ||
      context->target_word_address_base_local != pointer.address_base_local)
    return false;
  effective_offset = pointer.address_offset + offset;
  byte_difference = effective_offset - context->target_word_address_offset;
  if ((byte_difference & 3u) != 0)
    return false;
  /* Interpret the wrapping byte delta as a signed two's-complement offset
   * without relying on an implementation-defined unsigned-to-signed cast. */
  memcpy(&signed_difference, &byte_difference, sizeof(signed_difference));
  word_displacement = signed_difference / 4;
  cached_word_operand(word_displacement, operand, size);
  return true;
}

/* Tests whether an aligned access can use the retained target address. */
static bool has_cached_word_operand(Context *context, Value pointer, uint32_t offset) {
  char operand[48];
  return find_cached_word_operand(context, pointer, offset, operand, sizeof(operand));
}

/* Builds a compiler value for a pure affine pointer without emitting its byte
 * arithmetic when the guarded loop already carries the corresponding native
 * word pointer. Wasm pointer values remain byte offsets everywhere else. */
static bool make_fast_word_pointer(Context *context, const WasmExpr *expression,
                                   uint32_t memory_offset, Value *pointer) {
  uint32_t local, offset, base;

  if (!context->fast_word_pointer_active ||
      !syntactic_pure_affine_local(expression, &local, &offset) ||
      local >= context->local_alignment_count)
    return false;
  base = context->local_address_bases[local];
  offset += context->local_address_offsets[local];
  *pointer = (Value){.slot = local_slot(context->function, local),
                     .type = WASM_VALUE_I32,
                     .present = true,
                     .is_borrowed = true,
                     .has_address_identity = true,
                     .address_base_local = base,
                     .address_offset = offset};
  return fast_loop_covers_access(context, *pointer, memory_offset, 4) &&
         has_cached_word_operand(context, *pointer, memory_offset);
}

/* Records the target word address currently held in R13. */
static void record_cached_word_address(Context *context, Value pointer, uint32_t offset) {
  if (!pointer.has_address_identity)
    return;
  context->target_word_address_cached = true;
  context->target_word_address_base_local = pointer.address_base_local;
  context->target_word_address_offset = pointer.address_offset + offset;
}

/* Loads an i32 at the already checked Wasm byte address in R2. */
static bool load_i32_at_r2(Context *context, int result_register, bool proven_aligned,
                           Value pointer, uint32_t offset) {
  char aligned[64], done[64];
  char operand[48];
  if (proven_aligned) {
    if (find_cached_word_operand(context, pointer, offset, operand, sizeof(operand)))
      return emit(context, "  mov R%d, %s", result_register, operand);
    if (!emit(context, "  shl R2, -2") || !emit(context, "  iadd R2, %u", LINEAR_BASE))
      return false;
    if (pointer.has_address_identity && !context->fast_word_pointer_active) {
      if (!emit(context, "  mov R13, R2"))
        return false;
      record_cached_word_address(context, pointer, offset);
      return emit(context, "  mov R%d, [R13]", result_register);
    }
    return emit(context, "  mov R%d, [R2]", result_register);
  }
  if (!fresh_label(context, "load_aligned", aligned, sizeof(aligned)) ||
      !fresh_label(context, "load_done", done, sizeof(done)) || !emit(context, "  mov R1, R2") ||
      !emit(context, "  and R1, 3") || !emit(context, "  jf R1, %s", aligned) || !emit(context, "  mov R6, 0"))
    return false;
  /* Unaligned accesses reconstruct the word from packed byte lanes. */
  for (unsigned byte = 0; byte < 4; ++byte) {
    if (!load_byte_at_r2(context, 5) || (byte != 0 && !emit(context, "  mov R3, %u", byte * 8)) ||
        (byte != 0 && !emit(context, "  shl R5, R3")) || !emit(context, "  or R6, R5") ||
        (byte != 3 && !emit(context, "  iadd R2, 1")))
      return false;
  }
  return emit(context, "  mov R%d, R6", result_register) && emit(context, "  jmp %s", done) &&
         emit_label(context, aligned) && emit(context, "  mov R3, R2") && emit(context, "  mov R4, -2") &&
         emit(context, "  shl R3, R4") && emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R%d, [R3]", result_register) && emit_label(context, done);
}

/* Stores an i32 at the already checked Wasm byte address in R2. */
static bool store_i32_at_r2(Context *context, int value_register, bool proven_aligned,
                            Value pointer, uint32_t offset) {
  char aligned[64], done[64];
  char operand[48];
  if (proven_aligned) {
    if (find_cached_word_operand(context, pointer, offset, operand, sizeof(operand)))
      return emit(context, "  mov %s, R%d", operand, value_register);
    if (!emit(context, "  shl R2, -2") || !emit(context, "  iadd R2, %u", LINEAR_BASE))
      return false;
    if (pointer.has_address_identity && !context->fast_word_pointer_active) {
      if (!emit(context, "  mov R13, R2"))
        return false;
      record_cached_word_address(context, pointer, offset);
      return emit(context, "  mov [R13], R%d", value_register);
    }
    return emit(context, "  mov [R2], R%d", value_register);
  }
  if (!fresh_label(context, "store_aligned", aligned, sizeof(aligned)) ||
      !fresh_label(context, "store_done", done, sizeof(done)) || !emit(context, "  mov R7, R2") ||
      !emit(context, "  and R7, 3") || !emit(context, "  jf R7, %s", aligned))
    return false;
  /* Unaligned accesses split the word into preserving byte stores. */
  for (unsigned byte = 0; byte < 4; ++byte) {
    /* store_byte_at_r2 uses R3-R6 internally, so R7 preserves this byte. */
    if (!emit(context, "  mov R7, R%d", value_register) || (byte != 0 && !emit(context, "  mov R3, -%u", byte * 8)) ||
        (byte != 0 && !emit(context, "  shl R7, R3")) || !store_byte_at_r2(context, 7) ||
        (byte != 3 && !emit(context, "  iadd R2, 1")))
      return false;
  }
  return emit(context, "  jmp %s", done) && emit_label(context, aligned) && emit(context, "  mov R3, R2") &&
         emit(context, "  mov R4, -2") && emit(context, "  shl R3, R4") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) && emit(context, "  mov [R3], R%d", value_register) &&
         emit_label(context, done);
}

/* Lowers a validated byte, halfword, or word Wasm load into packed-memory
 * operations. Narrow values are reconstructed in little-endian order and
 * sign-extended only when requested by the Wasm opcode. */
static bool lower_load(Context *context, const WasmExpr *expression, Value *value) {
  Value pointer = {0}, result = {0};
  bool proven_aligned, cached_word_address;
  int slot;
  proven_aligned = expression->bytes == 4 &&
                   word_access_is_proven_aligned(context, expression->children[0], expression->offset);
  if (!(proven_aligned && make_fast_word_pointer(context, expression->children[0], expression->offset, &pointer)) &&
      (!lower_expression(context, expression->children[0], &pointer) || !pointer.present))
    return false;
  cached_word_address = proven_aligned && has_cached_word_operand(context, pointer, expression->offset);
  result = pointer;
  if (!reserve_value_slot(context, &result) ||
      !effective_address(context, pointer, expression->offset, expression->bytes, !cached_word_address))
    return false;
  result.has_address_identity = false;
  if (expression->bytes == 1) {
    if (!load_byte_at_r2(context, 1) || (expression->is_signed && !emit_i32_extend_s(context, 8)) ||
        !store_slot(context, result.slot, 1))
      return false;
    *value = result;
    return true;
  }
  if (expression->bytes == 2) {
    if (!load_byte_at_r2(context, 1) || !emit(context, "  iadd R2, 1") || !load_byte_at_r2(context, 5) ||
        !emit(context, "  shl R5, 8") || !emit(context, "  or R1, R5") ||
        (expression->is_signed && !emit_i32_extend_s(context, 16)) || !store_slot(context, result.slot, 1))
      return false;
    *value = result;
    return true;
  }
  if (!load_i32_at_r2(context, 1, proven_aligned, pointer, expression->offset))
    return false;
  slot = result.slot;
  if (!store_slot(context, slot, 1))
    return false;
  *value = result;
  return true;
}
/* Lowers a validated byte, halfword, or word Wasm store into packed-memory
 * operations. Halfword stores preserve every neighboring byte even when the
 * two-byte range crosses a Vircon32 word boundary. */
static bool lower_store(Context *context, const WasmExpr *expression, Value *value) {
  Value pointer = {0}, input = {0};
  bool proven_aligned, cached_word_address, pointer_is_fast = false;
  proven_aligned = expression->bytes == 4 &&
                   word_access_is_proven_aligned(context, expression->children[0], expression->offset);
  if (proven_aligned)
    pointer_is_fast = make_fast_word_pointer(context, expression->children[0], expression->offset, &pointer);
  if (!pointer_is_fast &&
      (!lower_expression(context, expression->children[0], &pointer) || !pointer.present))
    return false;
  if ((!pointer_is_fast && !materialize_borrowed(context, &pointer)) ||
      !lower_expression(context, expression->children[1], &input) || !input.present)
    return false;
  cached_word_address = proven_aligned && has_cached_word_operand(context, pointer, expression->offset);
  if (!effective_address(context, pointer, expression->offset, expression->bytes, !cached_word_address) ||
      !load_value(context, 1, input))
    return false;
  if (expression->bytes == 1) {
    if (!store_byte_at_r2(context, 1))
      return false;
    release(context, input);
    release(context, pointer);
    value->present = false;
    return true;
  }
  if (expression->bytes == 2) {
    if (!store_byte_at_r2(context, 1) || !emit(context, "  iadd R2, 1") || !emit(context, "  mov R7, R1") ||
        !emit(context, "  shl R7, -8") || !store_byte_at_r2(context, 7))
      return false;
    release(context, input);
    release(context, pointer);
    value->present = false;
    return true;
  }
  if (!store_i32_at_r2(context, 1, proven_aligned, pointer, expression->offset))
    return false;
  release(context, input);
  release(context, pointer);
  value->present = false;
  return true;
}

/* Creates a two-word temporary result whose low word is followed by its high word. */
static bool reserve_i64_value(Context *context, Value *value) {
  int low = temp_slots(context, 2);
  if (low == 0)
    return false;
  value->slot = low;
  value->high_slot = low - 1;
  value->type = WASM_VALUE_I64;
  value->present = true;
  return true;
}

/* Loads an i64 as two little-endian i32 words while retaining Wasm bounds checks. */
static bool lower_i64_load(Context *context, const WasmExpr *expression, Value *value) {
  Value pointer = {0}, result = {0};
  int high_slot;

  if (!lower_expression(context, expression->children[0], &pointer) || !pointer.present)
    return false;
  result = pointer;
  if (!reserve_value_slot(context, &result) || !effective_address(context, pointer, expression->offset, 8, true))
    return false;
  high_slot = temp_slot(context);
  if (high_slot == 0 || !store_slot(context, high_slot, 2) ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, result.slot, 1) || !load_slot(context, 2, high_slot) || !emit(context, "  iadd R2, 4") ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) || !store_slot(context, high_slot, 1))
    return false;
  result.high_slot = high_slot;
  result.type = WASM_VALUE_I64;
  *value = result;
  return true;
}

/* Stores a general two-word i64 value with Wasm's value-before-store semantics. */
static bool lower_i64_store(Context *context, const WasmExpr *expression, Value *value) {
  Value pointer = {0}, input = {0};
  int address_slot;

  if (!lower_expression(context, expression->children[0], &pointer) || !materialize_borrowed(context, &pointer) ||
      !lower_expression(context, expression->children[1], &input) || !pointer.present || !input.present ||
      input.type != WASM_VALUE_I64 || !effective_address(context, pointer, expression->offset, 8, true))
    return false;
  address_slot = temp_slot(context);
  if (address_slot == 0 || !store_slot(context, address_slot, 2) || !load_slot(context, 1, input.slot) ||
      !store_i32_at_r2(context, 1, false, (Value){0}, 0) || !load_slot(context, 2, address_slot) ||
      !emit(context, "  iadd R2, 4") || !load_slot(context, 1, input.high_slot) ||
      !store_i32_at_r2(context, 1, false, (Value){0}, 0))
    return false;

  release(context, (Value){.slot = address_slot, .type = WASM_VALUE_I32, .present = true});
  release(context, input);
  release(context, pointer);
  value->present = false;
  return true;
}

/* Lowers i64 construction from an i32, preserving signed or unsigned extension. */
static bool lower_i64_extend_i32(Context *context, const WasmExpr *expression, Value *value, bool is_signed) {
  Value input = {0};
  int high_slot;
  char nonnegative[64];

  if (!lower_expression(context, expression->children[0], &input) || !input.present ||
      !materialize_value(context, &input) || !load_value(context, 1, input) || !store_slot(context, input.slot, 1))
    return false;
  high_slot = temp_slot(context);
  if (high_slot == 0)
    return false;
  if (!is_signed) {
    if (!emit(context, "  mov R1, 0") || !store_slot(context, high_slot, 1))
      return false;
  } else {
    if (!fresh_label(context, "i64_extend_nonnegative", nonnegative, sizeof(nonnegative)) || !emit(context, "  mov R1, 0") ||
        !load_slot(context, 2, input.slot) || !emit(context, "  ilt R2, 0") || !emit(context, "  jf R2, %s", nonnegative) ||
        !emit(context, "  mov R1, -1") || !emit_label(context, nonnegative) || !store_slot(context, high_slot, 1))
      return false;
  }
  input.high_slot = high_slot;
  input.type = WASM_VALUE_I64;
  *value = input;
  return true;
}

/* Wraps a pair-valued i64 to its low i32 word. */
static bool lower_i32_wrap_i64(Context *context, const WasmExpr *expression, Value *value) {
  Value input = {0};
  if (!lower_expression(context, expression->children[0], &input) || !input.present || input.type != WASM_VALUE_I64)
    return false;
  if (!load_slot(context, 1, input.slot) || !store_slot(context, input.slot, 1))
    return false;
  input.type = WASM_VALUE_I32;
  /* The low word is retained; drop the high reservation beneath it. */
  if (context->temp_depth == 0)
    return false;
  --context->temp_depth;
  *value = input;
  return true;
}

/* Compares the two words of an i64 for Wasm eqz and returns an i32 boolean. */
static bool lower_i64_eqz(Context *context, const WasmExpr *expression, Value *value) {
  Value input = {0};
  if (!lower_expression(context, expression->children[0], &input) || !input.present || input.type != WASM_VALUE_I64 ||
      !load_slot(context, 1, input.slot) || !emit(context, "  ieq R1, 0") || !load_slot(context, 2, input.high_slot) ||
      !emit(context, "  ieq R2, 0") || !emit(context, "  and R1, R2") || !store_slot(context, input.slot, 1))
    return false;
  input.type = WASM_VALUE_I32;
  --context->temp_depth;
  *value = input;
  return true;
}

/* Lowers the supported pair-wise i64 arithmetic, bit operations, and equality. */
static bool lower_i64_binary(Context *context, const WasmExpr *expression, Value *value) {
  Value left = {0}, right = {0};
  char zero[64], small[64], nonnegative[64], done[64];

  if (!lower_expression(context, expression->children[0], &left) ||
      !lower_expression(context, expression->children[1], &right) || !left.present || !right.present ||
      left.type != WASM_VALUE_I64 || right.type != WASM_VALUE_I64)
    return false;

  switch (expression->binary_op) {
  case WASM_BINARY_I64_AND:
  case WASM_BINARY_I64_OR:
  case WASM_BINARY_I64_XOR:
    if (!load_slot(context, 1, left.slot) || !load_slot(context, 2, right.slot) ||
        !(expression->binary_op == WASM_BINARY_I64_AND ? emit(context, "  and R1, R2")
          : expression->binary_op == WASM_BINARY_I64_OR ? emit(context, "  or R1, R2")
                                                       : emit(context, "  xor R1, R2")) ||
        !store_slot(context, left.slot, 1) || !load_slot(context, 1, left.high_slot) ||
        !load_slot(context, 2, right.high_slot) ||
        !(expression->binary_op == WASM_BINARY_I64_AND ? emit(context, "  and R1, R2")
          : expression->binary_op == WASM_BINARY_I64_OR ? emit(context, "  or R1, R2")
                                                       : emit(context, "  xor R1, R2")) ||
        !store_slot(context, left.high_slot, 1))
      return false;
    break;
  case WASM_BINARY_I64_ADD:
  case WASM_BINARY_I64_SUB:
    if (!load_slot(context, 1, left.slot) || !load_slot(context, 2, right.slot) || !emit(context, "  mov R3, R1") ||
        !(expression->binary_op == WASM_BINARY_I64_ADD ? emit(context, "  iadd R3, R2") : emit(context, "  isub R3, R2")) ||
        !emit(context, "  mov R4, R3") || !emit(context, "  xor R4, 0x80000000") ||
        !emit(context, "  xor R1, 0x80000000") ||
        !(expression->binary_op == WASM_BINARY_I64_ADD ? emit(context, "  ilt R4, R1") : emit(context, "  igt R4, R1")) ||
        !load_slot(context, 1, left.high_slot) || !load_slot(context, 2, right.high_slot) ||
        !(expression->binary_op == WASM_BINARY_I64_ADD ? emit(context, "  iadd R1, R2") : emit(context, "  isub R1, R2")) ||
        !(expression->binary_op == WASM_BINARY_I64_ADD ? emit(context, "  iadd R1, R4") : emit(context, "  isub R1, R4")) ||
        !store_slot(context, left.slot, 3) || !store_slot(context, left.high_slot, 1))
      return false;
    break;
  case WASM_BINARY_I64_MUL: {
    Value product_low = {0}, middle = {0}, product_middle = {0};

    /*
     * Split each 32-bit word into two 16-bit limbs. Every limb product fits
     * in one target word, and the carries accumulated below reconstruct the
     * low 64 bits of the Wasm product without needing a target i64 register.
     */
    product_low.slot = temp_slot(context);
    middle.slot = temp_slot(context);
    product_middle.slot = temp_slot(context);
    product_low.type = middle.type = product_middle.type = WASM_VALUE_I32;
    product_low.present = middle.present = product_middle.present = true;
    if (product_low.slot == 0 || middle.slot == 0 || product_middle.slot == 0 ||
        /* p0 = a0 * b0 */
        !load_slot(context, 1, left.slot) || !emit(context, "  and R1, 0x0000FFFF") ||
        !load_slot(context, 2, right.slot) || !emit(context, "  and R2, 0x0000FFFF") || !emit(context, "  imul R1, R2") ||
        !store_slot(context, product_low.slot, 1) ||
        /* t = a1 * b0 + (p0 >> 16) */
        !load_slot(context, 1, left.slot) || !emit(context, "  mov R2, -16") || !emit(context, "  shl R1, R2") ||
        !emit(context, "  and R1, 0x0000FFFF") || !load_slot(context, 2, right.slot) ||
        !emit(context, "  and R2, 0x0000FFFF") || !emit(context, "  imul R1, R2") ||
        !load_slot(context, 2, product_low.slot) || !emit(context, "  mov R3, -16") || !emit(context, "  shl R2, R3") ||
        !emit(context, "  iadd R1, R2") || !store_slot(context, middle.slot, 1) ||
        /* u = (t & 0xffff) + a0 * b1 */
        !load_slot(context, 1, middle.slot) || !emit(context, "  and R1, 0x0000FFFF") ||
        !load_slot(context, 2, left.slot) || !emit(context, "  and R2, 0x0000FFFF") ||
        !load_slot(context, 3, right.slot) || !emit(context, "  mov R4, -16") || !emit(context, "  shl R3, R4") ||
        !emit(context, "  and R3, 0x0000FFFF") || !emit(context, "  imul R2, R3") || !emit(context, "  iadd R1, R2") ||
        !store_slot(context, product_middle.slot, 1) ||
        /* high(a0 * b0) = a1 * b1 + (t >> 16) + (u >> 16) */
        !load_slot(context, 1, left.slot) || !emit(context, "  mov R2, -16") || !emit(context, "  shl R1, R2") ||
        !emit(context, "  and R1, 0x0000FFFF") || !load_slot(context, 2, right.slot) ||
        !emit(context, "  mov R3, -16") || !emit(context, "  shl R2, R3") || !emit(context, "  and R2, 0x0000FFFF") ||
        !emit(context, "  imul R1, R2") || !load_slot(context, 2, middle.slot) || !emit(context, "  mov R3, -16") ||
        !emit(context, "  shl R2, R3") || !emit(context, "  iadd R1, R2") || !load_slot(context, 2, product_middle.slot) ||
        !emit(context, "  mov R3, -16") || !emit(context, "  shl R2, R3") || !emit(context, "  iadd R1, R2") ||
        /* Add low(a0 * b1) and low(a1 * b0); terms at bit 64 and above drop. */
        !load_slot(context, 2, left.slot) || !load_slot(context, 3, right.high_slot) || !emit(context, "  imul R2, R3") ||
        !emit(context, "  iadd R1, R2") || !load_slot(context, 2, left.high_slot) || !load_slot(context, 3, right.slot) ||
        !emit(context, "  imul R2, R3") || !emit(context, "  iadd R1, R2") ||
        !store_slot(context, left.high_slot, 1) ||
        /* low = (u << 16) | (p0 & 0xffff) */
        !load_slot(context, 1, product_middle.slot) || !emit(context, "  and R1, 0x0000FFFF") ||
        !emit(context, "  shl R1, 16") || !load_slot(context, 2, product_low.slot) ||
        !emit(context, "  and R2, 0x0000FFFF") || !emit(context, "  or R1, R2") || !store_slot(context, left.slot, 1))
      return false;
    release(context, product_middle);
    release(context, middle);
    release(context, product_low);
    break;
  }
  case WASM_BINARY_I64_EQ:
  case WASM_BINARY_I64_NE:
    if (!load_slot(context, 1, left.slot) || !load_slot(context, 2, right.slot) || !emit(context, "  ieq R1, R2") ||
        !load_slot(context, 2, left.high_slot) || !load_slot(context, 3, right.high_slot) || !emit(context, "  ieq R2, R3") ||
        !emit(context, "  and R1, R2") ||
        (expression->binary_op == WASM_BINARY_I64_NE && !emit(context, "  ieq R1, 0")) || !store_slot(context, left.slot, 1))
      return false;
    release(context, right);
    /* The comparison replaces the left pair with one ordinary i32 word. */
    if (context->temp_depth == 0)
      return false;
    --context->temp_depth;
    left.type = WASM_VALUE_I32;
    left.high_slot = 0;
    *value = left;
    return true;
  case WASM_BINARY_I64_LT_S:
  case WASM_BINARY_I64_LT_U:
  case WASM_BINARY_I64_LE_S:
  case WASM_BINARY_I64_LE_U:
  case WASM_BINARY_I64_GT_S:
  case WASM_BINARY_I64_GT_U:
  case WASM_BINARY_I64_GE_S:
  case WASM_BINARY_I64_GE_U: {
    bool unsigned_compare = expression->binary_op == WASM_BINARY_I64_LT_U ||
                            expression->binary_op == WASM_BINARY_I64_LE_U ||
                            expression->binary_op == WASM_BINARY_I64_GT_U ||
                            expression->binary_op == WASM_BINARY_I64_GE_U;
    bool less_compare = expression->binary_op == WASM_BINARY_I64_LT_S ||
                        expression->binary_op == WASM_BINARY_I64_LT_U ||
                        expression->binary_op == WASM_BINARY_I64_LE_S ||
                        expression->binary_op == WASM_BINARY_I64_LE_U;
    bool inclusive = expression->binary_op == WASM_BINARY_I64_LE_S ||
                     expression->binary_op == WASM_BINARY_I64_LE_U ||
                     expression->binary_op == WASM_BINARY_I64_GE_S ||
                     expression->binary_op == WASM_BINARY_I64_GE_U;
    const char *preferred = less_compare ? "ilt" : "igt";
    const char *opposite = less_compare ? "igt" : "ilt";
    const char *low_compare = less_compare ? (inclusive ? "ile" : "ilt") : (inclusive ? "ige" : "igt");
    char true_label[64], false_label[64], done_label[64];

    if (!fresh_label(context, "i64_compare_true", true_label, sizeof(true_label)) ||
        !fresh_label(context, "i64_compare_false", false_label, sizeof(false_label)) ||
        !fresh_label(context, "i64_compare_done", done_label, sizeof(done_label)) ||
        !load_slot(context, 1, left.high_slot) || !load_slot(context, 2, right.high_slot))
      return false;
    /* Unsigned 32-bit ordering becomes signed ordering after flipping bit 31. */
    if (unsigned_compare && (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000")))
      return false;
    if (!emit(context, "  mov R3, R1") || !emit(context, "  %s R3, R2", preferred) ||
        !emit(context, "  jt R3, %s", true_label) || !emit(context, "  mov R3, R1") ||
        !emit(context, "  %s R3, R2", opposite) || !emit(context, "  jt R3, %s", false_label) ||
        !load_slot(context, 1, left.slot) || !load_slot(context, 2, right.slot) ||
        !emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  %s R1, R2", low_compare) || !emit(context, "  jmp %s", done_label) ||
        !emit_label(context, true_label) || !emit(context, "  mov R1, 1") || !emit(context, "  jmp %s", done_label) ||
        !emit_label(context, false_label) || !emit(context, "  mov R1, 0") || !emit_label(context, done_label) ||
        !store_slot(context, left.slot, 1))
      return false;

    release(context, right);
    /* Replace the left pair by its single i32 comparison result. */
    if (context->temp_depth == 0)
      return false;
    --context->temp_depth;
    left.type = WASM_VALUE_I32;
    left.high_slot = 0;
    *value = left;
    return true;
  }
  case WASM_BINARY_I64_SHL:
  case WASM_BINARY_I64_SHR_U:
  case WASM_BINARY_I64_SHR_S:
    if (!fresh_label(context, "i64_shift_zero", zero, sizeof(zero)) ||
        !fresh_label(context, "i64_shift_small", small, sizeof(small)) ||
        !fresh_label(context, "i64_shift_nonnegative", nonnegative, sizeof(nonnegative)) ||
        !fresh_label(context, "i64_shift_done", done, sizeof(done)) || !load_slot(context, 2, right.slot) ||
        !emit(context, "  and R2, 63") || !emit(context, "  jf R2, %s", zero) || !emit(context, "  mov R3, R2") ||
        !emit(context, "  ilt R3, 32") || !emit(context, "  jt R3, %s", small))
      return false;
    if (expression->binary_op == WASM_BINARY_I64_SHL) {
      if (!emit(context, "  isub R2, 32") || !load_slot(context, 1, left.slot) || !emit(context, "  shl R1, R2") ||
          !store_slot(context, left.high_slot, 1) || !emit(context, "  mov R1, 0") || !store_slot(context, left.slot, 1))
        return false;
    } else {
      /* For shifts of 32..63, the old high word becomes the low result. */
      if (!emit(context, "  isub R2, 32") || !emit(context, "  mov R3, 0") || !emit(context, "  isub R3, R2") ||
          !load_slot(context, 1, left.high_slot))
        return false;
      if (expression->binary_op == WASM_BINARY_I64_SHR_S) {
        if (!emit_i32_shr_s(context))
          return false;
      } else if (!emit(context, "  shl R1, R3"))
        return false;
      if (!emit(context, "  mov R7, R1"))
        return false;
      if (expression->binary_op == WASM_BINARY_I64_SHR_S) {
        if (!load_slot(context, 1, left.high_slot) || !emit(context, "  ilt R1, 0") ||
            !emit(context, "  jf R1, %s", nonnegative) || !emit(context, "  mov R1, -1") ||
            !store_slot(context, left.high_slot, 1) || !store_slot(context, left.slot, 7) ||
            !emit(context, "  jmp %s", done) || !emit_label(context, nonnegative))
          return false;
      }
      if (!emit(context, "  mov R1, 0") || !store_slot(context, left.high_slot, 1) ||
          !store_slot(context, left.slot, 7))
        return false;
    }
    if (!emit(context, "  jmp %s", done) || !emit_label(context, small))
      return false;
    if (expression->binary_op == WASM_BINARY_I64_SHL) {
      if (!load_slot(context, 1, left.slot) || !load_slot(context, 3, left.high_slot) || !emit(context, "  mov R4, R1") ||
          !emit(context, "  shl R4, R2") || !emit(context, "  mov R5, R2") || !emit(context, "  isub R5, 32") ||
          !emit(context, "  shl R1, R5") || !emit(context, "  shl R3, R2") || !emit(context, "  or R3, R1") ||
          !store_slot(context, left.slot, 4) || !store_slot(context, left.high_slot, 3))
        return false;
    } else {
      if (!load_slot(context, 1, left.slot) || !load_slot(context, 3, left.high_slot) || !emit(context, "  mov R4, R3") ||
          !emit(context, "  mov R5, 0") || !emit(context, "  isub R5, R2") || !emit(context, "  shl R1, R5") ||
          !emit(context, "  mov R5, 32") || !emit(context, "  isub R5, R2") || !emit(context, "  shl R3, R5") ||
          !emit(context, "  or R1, R3"))
        return false;
      if (expression->binary_op == WASM_BINARY_I64_SHR_S) {
        /* Preserve the reconstructed low word while the signed high shift uses R1. */
        if (!emit(context, "  mov R7, R1") || !emit(context, "  mov R1, R4") || !emit_i32_shr_s(context) ||
            !store_slot(context, left.high_slot, 1) || !store_slot(context, left.slot, 7))
          return false;
      } else if (!emit(context, "  mov R5, 0") || !emit(context, "  isub R5, R2") || !emit(context, "  shl R4, R5") ||
                 !store_slot(context, left.high_slot, 4))
        return false;
      if (expression->binary_op != WASM_BINARY_I64_SHR_S && !store_slot(context, left.slot, 1))
        return false;
    }
    if (!emit(context, "  jmp %s", done) || !emit_label(context, zero) || !emit_label(context, done))
      return false;
    break;
  default:
    diagnostics_error(context->diagnostics, "internal error: unsupported i64 binary operation passed validation");
    return false;
  }
  release(context, right);
  *value = left;
  return true;
}
/* Emits the validated constant, aligned i64-store form as two i32 stores. */
static bool lower_i64_const_store(Context *context, const WasmExpr *expression, Value *value) {
  uint64_t address = (uint64_t)(uint32_t)expression->children[0]->i32_value + expression->offset;
  uint32_t low = (uint32_t)expression->i64_value;
  uint32_t high = (uint32_t)(expression->i64_value >> 32);
  unsigned word = LINEAR_BASE + (unsigned)(address >> 2);

  /* Validation has already proved the full eight-byte range and the
   * four-byte alignment. Store low then high to preserve Wasm little-endian
   * layout without introducing an i64 target value or arithmetic path. */
  if (!emit(context, "  mov R1, 0x%08X", low) || !emit(context, "  mov [%u], R1", word) ||
      !emit(context, "  mov R1, 0x%08X", high) || !emit(context, "  mov [%u], R1", word + 1))
    return false;
  value->present = false;
  return true;
}

/* Computes the low i32 word after a validated constant logical i64 shift. */
static bool extract_i64_word(Context *context, int low_slot, int high_slot, uint64_t shift) {
  if (shift == 0)
    return load_slot(context, 1, low_slot);
  if (shift < 32) {
    return load_slot(context, 1, low_slot) && emit(context, "  mov R2, -%u", (unsigned)shift) &&
           emit(context, "  shl R1, R2") && load_slot(context, 3, high_slot) &&
           emit(context, "  mov R2, %u", 32u - (unsigned)shift) && emit(context, "  shl R3, R2") &&
           emit(context, "  or R1, R3");
  }
  if (shift == 32)
    return load_slot(context, 1, high_slot);
  return load_slot(context, 1, high_slot) && emit(context, "  mov R2, -%u", (unsigned)(shift - 32)) &&
         emit(context, "  shl R1, R2");
}

/*
 * Lowers the only dynamic i64 form accepted by this profile: an i64.load used
 * directly as an i64.store value. The two temporary slots hold the loaded low
 * and high i32 words before either destination write, preserving Wasm's
 * value-then-store behavior even when the eight-byte ranges overlap.
 */
static bool lower_i64_load_store(Context *context, const WasmExpr *expression, Value *value) {
  Value destination = {0}, source = {0}, high_word = {0}, destination_address = {0};

  /* A Wasm store evaluates its destination address before its value. */
  if (!lower_expression(context, expression->children[0], &destination) ||
      !materialize_value(context, &destination) || !lower_expression(context, expression->children[1], &source) ||
      !source.present || !materialize_value(context, &source) ||
      !effective_address(context, source, expression->source_offset, 8, true))
    return false;

  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, source.slot, 1) || !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, high_word.slot, 1))
    return false;
  high_word.present = true;

  if (!effective_address(context, destination, expression->offset, 8, true))
    return false;
  destination_address.slot = temp_slot(context);
  if (destination_address.slot == 0 || !store_slot(context, destination_address.slot, 2) ||
      !load_slot(context, 2, destination_address.slot) || !load_slot(context, 1, source.slot) ||
      !store_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !load_slot(context, 2, destination_address.slot) || !emit(context, "  iadd R2, 4") ||
      !load_slot(context, 1, high_word.slot) || !store_i32_at_r2(context, 1, false, (Value){0}, 0))
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
static bool lower_i64_load_store_local_tee(Context *context, const WasmExpr *expression, Value *value) {
  Value destination = {0}, source = {0}, high_word = {0}, destination_address = {0};
  int local_low = local_slot(context->function, expression->index);
  int local_high = i64_local_high_slot(context->function, expression->index);

  if (!lower_expression(context, expression->children[0], &destination) ||
      !materialize_value(context, &destination) || !lower_expression(context, expression->children[1], &source) ||
      !source.present || !materialize_value(context, &source) ||
      !effective_address(context, source, expression->source_offset, 8, true))
    return false;
  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, source.slot, 1) || !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, high_word.slot, 1))
    return false;
  high_word.present = true;

  /* local.tee writes the pair before the enclosing i64.store observes it. */
  if (!load_slot(context, 1, source.slot) || !store_slot(context, local_low, 1) ||
      !load_slot(context, 1, high_word.slot) || !store_slot(context, local_high, 1) ||
      !effective_address(context, destination, expression->offset, 8, true))
    return false;
  destination_address.slot = temp_slot(context);
  if (destination_address.slot == 0 || !store_slot(context, destination_address.slot, 2) ||
      !load_slot(context, 2, destination_address.slot) || !load_slot(context, 1, source.slot) ||
      !store_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !load_slot(context, 2, destination_address.slot) || !emit(context, "  iadd R2, 4") ||
      !load_slot(context, 1, high_word.slot) || !store_i32_at_r2(context, 1, false, (Value){0}, 0))
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
static bool lower_i64_packed_i32_store(Context *context, const WasmExpr *expression, Value *value) {
  Value destination = {0}, high_word = {0}, low_word = {0};

  /* Wasm evaluates the store address, high expression, then low expression. */
  if (!lower_expression(context, expression->children[0], &destination) ||
      !materialize_borrowed(context, &destination) ||
      !lower_expression(context, expression->children[1], &high_word) || !materialize_borrowed(context, &high_word) ||
      !lower_expression(context, expression->children[2], &low_word) || !destination.present || !high_word.present ||
      !low_word.present || !effective_address(context, destination, expression->offset, 8, true) ||
      !load_value(context, 1, low_word) || !store_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !load_value(context, 2, destination) || !emit(context, "  iadd R2, 4") ||
      !load_value(context, 1, high_word) || !store_i32_at_r2(context, 1, false, (Value){0}, 0))
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
static bool lower_i64_word_extract(Context *context, const WasmExpr *expression, Value *value) {
  Value pointer = {0}, low_word = {0}, high_word = {0};
  uint64_t shift = expression->i64_value;

  if (!lower_expression(context, expression->children[0], &pointer) || !pointer.present)
    return false;
  low_word = pointer;
  if (!reserve_value_slot(context, &low_word) ||
      !effective_address(context, pointer, expression->source_offset, 8, true))
    return false;
  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, low_word.slot, 1) || !load_slot(context, 2, high_word.slot) ||
      !emit(context, "  iadd R2, 4") || !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
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
static bool lower_i64_local_word_extract(Context *context, const WasmExpr *expression, Value *value) {
  int result_slot = temp_slot(context);
  int local_low = local_slot(context->function, expression->index);
  int local_high = i64_local_high_slot(context->function, expression->index);
  if (result_slot == 0 || !extract_i64_word(context, local_low, local_high, expression->i64_value) ||
      !store_slot(context, result_slot, 1))
    return false;
  value->slot = result_slot;
  value->present = true;
  return true;
}

/* Loads a pair into a restricted i64 local and returns one extracted i32 word.
 */
static bool lower_i64_local_tee_word_extract(Context *context, const WasmExpr *expression, Value *value) {
  Value pointer = {0}, high_word = {0};
  int result_slot = temp_slot(context);
  int local_low = local_slot(context->function, expression->index);
  int local_high = i64_local_high_slot(context->function, expression->index);

  if (result_slot == 0 || !lower_expression(context, expression->children[0], &pointer) || !pointer.present ||
      !effective_address(context, pointer, expression->source_offset, 8, true))
    return false;
  high_word.slot = temp_slot(context);
  if (high_word.slot == 0 || !store_slot(context, high_word.slot, 2) ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) ||
      !store_slot(context, local_low, 1) || !load_slot(context, 2, high_word.slot) || !emit(context, "  iadd R2, 4") ||
      !load_i32_at_r2(context, 1, false, (Value){0}, 0) || !store_slot(context, local_high, 1) ||
      !extract_i64_word(context, local_low, local_high, expression->i64_value) || !store_slot(context, result_slot, 1))
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
static bool lower_bulk_memory(Context *context, const WasmExpr *expression, Value *value) {
  Value arguments[3] = {{0}};
  size_t index;
  const char *helper = expression->kind == WASM_EXPR_MEMORY_COPY ? "__wasm_memory_copy" : "__wasm_memory_fill";

  /* Wasm evaluates destination, source/value, then length exactly once. */
  for (index = 0; index < 3; ++index)
    if (!lower_expression(context, expression->children[index], &arguments[index]) || !arguments[index].present ||
        (index != 2 && !materialize_borrowed(context, &arguments[index])))
      return false;
  for (index = 0; index < 3; ++index)
    if (!load_value(context, 1, arguments[index]) || !emit(context, "  mov [SP+%zu], R1", index))
      return false;
  for (index = 3; index != 0; --index)
    release(context, arguments[index - 1]);
  if (!emit(context, "  call %s", helper))
    return false;
  context->target_word_address_cached = false;
  value->present = false;
  return true;
}

/* Returns the largest page count this target can honor for this memory. A
 * module may declare a larger maximum, but Wasm permits an implementation to
 * impose a smaller physical limit and report memory.grow failure. */
static uint32_t memory_growth_limit_pages(const Context *context) {
  uint32_t limit = (uint32_t)VIRCON_LINEAR_MEMORY_MAX_PAGES;
  if (context->validated->module->memory_has_max && context->validated->module->memory_max_pages < limit)
    limit = context->validated->module->memory_max_pages;
  return limit;
}

/* Lowers memory.size by reading compiler-owned current-page state. */
static bool lower_memory_size(Context *context, Value *value) {
  int slot = temp_slot(context);
  if (slot == 0 || !emit(context, "  mov R1, [%u]", VIRCON_WASM_MEMORY_PAGES_WORD) || !store_slot(context, slot, 1))
    return false;
  value->slot = slot;
  value->present = true;
  return true;
}

/* Lowers memory.grow, including Wasm's zero-initialization and -1 failure
 * result. The generated loop writes target words, while Wasm remains in byte
 * units everywhere outside this target-specific legalization. */
static bool lower_memory_grow(Context *context, const WasmExpr *expression, Value *value) {
  Value delta = {0}, delta_source = {0};
  char clear[64], failed[64], succeeded[64], done[64];
  uint32_t maximum = memory_growth_limit_pages(context);

  if (!lower_expression(context, expression->children[0], &delta) || !delta.present)
    return false;
  delta_source = delta;
  if (!reserve_value_slot(context, &delta) ||
      !fresh_label(context, "memory_grow_clear", clear, sizeof(clear)) ||
      !fresh_label(context, "memory_grow_failed", failed, sizeof(failed)) ||
      !fresh_label(context, "memory_grow_succeeded", succeeded, sizeof(succeeded)) ||
      !fresh_label(context, "memory_grow_done", done, sizeof(done)) || !load_value(context, 2, delta_source))
    return false;
  /* R6 preserves the old page count for the successful Wasm result. */
  if (!emit(context, "  mov R6, [%u]", VIRCON_WASM_MEMORY_PAGES_WORD) || !emit(context, "  mov R1, R2") ||
      !emit(context, "  ilt R1, 0") || !emit(context, "  jt R1, %s", failed) ||
      !emit(context, "  mov R1, 0x%08X", maximum) || !emit(context, "  isub R1, R6") ||
      /* Vircon comparisons write their boolean result into their first
       * operand. Keep R2 intact: it is the requested page count used below
       * for both the new page count and the zero-initialization span. */
      !emit(context, "  mov R3, R2") || !emit(context, "  igt R3, R1") || !emit(context, "  jt R3, %s", failed) ||
      !emit(context, "  mov R5, R6") ||
      !emit(context, "  iadd R5, R2") || !emit(context, "  mov R3, R6") || !emit(context, "  imul R3, 16384") ||
      !emit(context, "  iadd R3, %u", LINEAR_BASE) || !emit(context, "  mov R4, R2") ||
      !emit(context, "  imul R4, 16384") || !emit(context, "  mov R1, 0") || !emit_label(context, clear) ||
      !emit(context, "  jf R4, %s", succeeded) ||
      !emit(context, "  mov [R3], R1") || !emit(context, "  iadd R3, 1") || !emit(context, "  isub R4, 1") ||
      !emit(context, "  jmp %s", clear) || !emit_label(context, succeeded) ||
      !emit(context, "  mov [%u], R5", VIRCON_WASM_MEMORY_PAGES_WORD) || !emit(context, "  mov R1, R6") ||
      !emit(context, "  jmp %s", done) || !emit_label(context, failed) || !emit(context, "  mov R1, -1") ||
      !emit_label(context, done) || !store_slot(context, delta.slot, 1))
    return false;
  *value = delta;
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

  return emit(context, "  and R2, 31") && emit(context, "  jf R2, %s", done) && emit(context, "  mov R3, R1") &&
         emit(context, "  ilt R3, 0") && emit(context, "  jf R3, %s", logical) && emit(context, "  mov R3, 0") &&
         emit(context, "  isub R3, R2") && emit(context, "  shl R1, R3") && emit(context, "  mov R4, 32") &&
         emit(context, "  isub R4, R2") && emit(context, "  mov R5, 0xFFFFFFFF") && emit(context, "  shl R5, R4") &&
         emit(context, "  or R1, R5") && emit(context, "  jmp %s", done) && emit_label(context, logical) &&
         emit(context, "  mov R3, 0") && emit(context, "  isub R3, R2") && emit(context, "  shl R1, R3") &&
         emit_label(context, done);
}

/* Sign-extends the low 8 or 16 bits currently held in R1. */
static bool emit_i32_extend_s(Context *context, unsigned bits) {
  uint32_t value_mask = bits == 8 ? 0x000000FFu : 0x0000FFFFu;
  uint32_t sign_mask = bits == 8 ? 0x00000080u : 0x00008000u;
  uint32_t extension_mask = bits == 8 ? 0xFFFFFF00u : 0xFFFF0000u;
  char done[64];

  if (!fresh_label(context, "extend_s_done", done, sizeof(done)))
    return false;
  return emit(context, "  and R1, 0x%08X", value_mask) && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 0x%08X", sign_mask) && emit(context, "  jf R2, %s", done) &&
         emit(context, "  or R1, 0x%08X", extension_mask) && emit_label(context, done);
}

/* Rotates R1 by the low five bits of R2 using Vircon32's bidirectional SHL. */
static bool emit_i32_rotate(Context *context, bool rotate_left) {
  char done[64];

  if (!fresh_label(context, rotate_left ? "rotl_done" : "rotr_done", done, sizeof(done)))
    return false;
  if (!emit(context, "  and R2, 31") || !emit(context, "  jf R2, %s", done) || !emit(context, "  mov R3, R1"))
    return false;

  if (rotate_left) {
    if (!emit(context, "  shl R1, R2") || !emit(context, "  mov R4, R2") || !emit(context, "  isub R4, 32") ||
        !emit(context, "  shl R3, R4"))
      return false;
  } else {
    if (!emit(context, "  mov R4, 0") || !emit(context, "  isub R4, R2") || !emit(context, "  shl R1, R4") ||
        !emit(context, "  mov R4, 32") || !emit(context, "  isub R4, R2") || !emit(context, "  shl R3, R4"))
      return false;
  }
  return emit(context, "  or R1, R3") && emit_label(context, done);
}

/* Emit a correctly rounded conversion from an unsigned Wasm i32 to f32.
 * CIF accepts only signed values. For the upper unsigned half, shifting once
 * and retaining bit zero as a sticky bit preserves round-to-nearest-even when
 * the converted half is doubled. */
static bool emit_f32_convert_i32_u(Context *context) {
  char signed_input[64], done[64];
  if (!fresh_label(context, "u32_to_f32_signed", signed_input, sizeof(signed_input)) ||
      !fresh_label(context, "u32_to_f32_done", done, sizeof(done)))
    return false;

  return emit(context, "  mov R2, R1") && emit(context, "  ilt R2, 0") && emit(context, "  jf R2, %s", signed_input) &&
         emit(context, "  mov R2, R1") && emit(context, "  and R2, 1") && emit(context, "  mov R3, R1") &&
         emit(context, "  mov R4, -1") && emit(context, "  shl R3, R4") && emit(context, "  or R3, R2") &&
         emit(context, "  cif R3") && emit(context, "  fadd R3, R3") && emit(context, "  mov R1, R3") &&
         emit(context, "  jmp %s", done) && emit_label(context, signed_input) && emit(context, "  cif R1") &&
         emit_label(context, done);
}

/* Reads one hardware port into the normal single-word compiler value model. */
static bool lower_port_read(Context *context, Value *value, const char *port) {
  int slot = temp_slot(context);
  if (slot == 0 || !emit(context, "  in R0, %s", port) || !store_slot(context, slot, 0))
    return false;
  value->slot = slot;
  value->present = true;
  return true;
}

/* Lowers a unary CPU instruction in place so its argument slot becomes the
 * returned value and remains owned by the enclosing expression. */
static bool lower_cpu_unary(Context *context, const Value *argument, Value *value, const char *instruction) {
  Value result = *argument;
  if (!reserve_value_slot(context, &result))
    return false;
  if (!load_value(context, 0, *argument) || !emit(context, "  %s R0", instruction) ||
      !store_slot(context, result.slot, 0))
    return false;
  *value = result;
  return true;
}

/* Lowers a binary CPU instruction into its left slot and releases the right
 * operand, matching the ownership convention used by ordinary Wasm binary
 * expressions. */
static bool lower_cpu_binary(Context *context, const Value *left, const Value *right, Value *value,
                             const char *instruction) {
  Value result;
  if (!left->is_immediate)
    result = *left;
  else if (!right->is_immediate)
    result = *right;
  else {
    result = *left;
    if (!reserve_value_slot(context, &result))
      return false;
  }
  if (!load_value(context, 0, *left) || !load_value(context, 1, *right) ||
      !emit(context, "  %s R0, R1", instruction) || !store_slot(context, result.slot, 0))
    return false;
  if (result.slot != right->slot || right->is_immediate)
    release(context, *right);
  *value = result;
  return true;
}

/* Evaluates arguments, then lowers either a platform import or direct call. */
static bool lower_call(Context *context, const WasmExpr *expression, Value *value) {
  const WasmFunction *callee = wasm_module_find_function(context->validated->module, expression->name);
  Value inline_arguments[INLINE_CALL_ARGUMENTS] = {{0}};
  Value *arguments = inline_arguments;
  bool heap_arguments = false;
  char label[64];
  size_t index;

  if (expression->child_count > INLINE_CALL_ARGUMENTS) {
    arguments = calloc(expression->child_count, sizeof(*arguments));
    if (arguments == NULL) {
      diagnostics_error(context->diagnostics, "out of memory lowering %zu call arguments", expression->child_count);
      return false;
    }
    heap_arguments = true;
  }
  for (index = 0; index < expression->child_count; ++index)
    if (!lower_expression(context, expression->children[index], &arguments[index]) || !arguments[index].present ||
        (index + 1 < expression->child_count && !materialize_borrowed(context, &arguments[index]))) {
      if (heap_arguments)
        free(arguments);
      return false;
    }
  if (callee->is_import) {
    if (strcmp(callee->import_name, "vircon_set_background_color") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out GPU_ClearColor, R1") ||
          !emit(context, "  out GPU_Command, GPUCommand_ClearScreen"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_end_frame") == 0) {
      if (!emit(context, "  wait"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_selected_texture") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !emit(context, "  in R0, GPU_SelectedTexture") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_gpu_select_texture") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out GPU_SelectedTexture, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_selected_region") == 0) {
      int slot = temp_slot(context);
      if (slot == 0 || !emit(context, "  in R0, GPU_SelectedRegion") || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else if (strcmp(callee->import_name, "vircon_gpu_select_region") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out GPU_SelectedRegion, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_point") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  out GPU_DrawingPointX, R1") || !emit(context, "  out GPU_DrawingPointY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region") == 0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegion"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_region_minimum") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  out GPU_RegionMinX, R1") || !emit(context, "  out GPU_RegionMinY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_region_maximum") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  out GPU_RegionMaxX, R1") || !emit(context, "  out GPU_RegionMaxY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_region_hotspot") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  out GPU_RegionHotSpotX, R1") || !emit(context, "  out GPU_RegionHotSpotY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_select_channel") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_SelectedChannel, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_select_sound") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_SelectedSound, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_get_selected_sound") == 0) {
      return lower_port_read(context, value, "SPU_SelectedSound");
    } else if (strcmp(callee->import_name, "vircon_spu_get_selected_channel") == 0) {
      return lower_port_read(context, value, "SPU_SelectedChannel");
    } else if (strcmp(callee->import_name, "vircon_spu_set_sound_play_with_loop") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_SoundPlayWithLoop, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_sound_loop_start") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_SoundLoopStart, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_sound_loop_end") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_SoundLoopEnd, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_assigned_sound") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_ChannelAssignedSound, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_play_selected_channel") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_PlaySelectedChannel"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_pause_selected_channel") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_PauseSelectedChannel"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_stop_selected_channel") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_StopSelectedChannel"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_volume") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_ChannelVolume, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_speed") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_ChannelSpeed, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_position") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_ChannelPosition, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_channel_loop_enabled") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_ChannelLoopEnabled, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_set_global_volume") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out SPU_GlobalVolume, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_get_channel_speed") == 0) {
      return lower_port_read(context, value, "SPU_ChannelSpeed");
    } else if (strcmp(callee->import_name, "vircon_spu_get_channel_position") == 0) {
      return lower_port_read(context, value, "SPU_ChannelPosition");
    } else if (strcmp(callee->import_name, "vircon_spu_get_global_volume") == 0) {
      return lower_port_read(context, value, "SPU_GlobalVolume");
    } else if (strcmp(callee->import_name, "vircon_spu_pause_all_channels") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_PauseAllChannels"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_stop_all_channels") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_StopAllChannels"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_spu_resume_all_channels") == 0) {
      if (!emit(context, "  out SPU_Command, SPUCommand_ResumeAllChannels"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_rng_set_current_value") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out RNG_CurrentValue, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_memcard_read_word") == 0) {
      Value result = arguments[0];
      if (!reserve_value_slot(context, &result) || !load_value(context, 1, arguments[0]) ||
          !emit(context, "  iadd R1, 0x30000000") || !emit(context, "  mov R0, [R1]") ||
          !store_slot(context, result.slot, 0))
        return false;
      *value = result;
      return true;
    } else if (strcmp(callee->import_name, "vircon_memcard_write_word") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  iadd R1, 0x30000000") || !emit(context, "  mov [R1], R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_multiply_color") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out GPU_MultiplyColor, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_multiply_color") == 0) {
      return lower_port_read(context, value, "GPU_MultiplyColor");
    } else if (strcmp(callee->import_name, "vircon_gpu_set_active_blending") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out GPU_ActiveBlending, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_active_blending") == 0) {
      return lower_port_read(context, value, "GPU_ActiveBlending");
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_point_x") == 0) {
      return lower_port_read(context, value, "GPU_DrawingPointX");
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_point_y") == 0) {
      return lower_port_read(context, value, "GPU_DrawingPointY");
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_scale_bits") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  out GPU_DrawingScaleX, R1") || !emit(context, "  out GPU_DrawingScaleY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_scale") == 0) {
      if (!load_value(context, 1, arguments[0]) || !load_value(context, 2, arguments[1]) ||
          !emit(context, "  out GPU_DrawingScaleX, R1") || !emit(context, "  out GPU_DrawingScaleY, R2"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_scale_x") == 0) {
      return lower_port_read(context, value, "GPU_DrawingScaleX");
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_scale_y") == 0) {
      return lower_port_read(context, value, "GPU_DrawingScaleY");
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region_zoomed") == 0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionZoomed"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_set_drawing_angle") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out GPU_DrawingAngle, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_get_drawing_angle") == 0) {
      return lower_port_read(context, value, "GPU_DrawingAngle");
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region_rotated") == 0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionRotated"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_gpu_draw_region_rotozoomed") == 0) {
      if (!emit(context, "  out GPU_Command, GPUCommand_DrawRegionRotozoomed"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_cpu_sin") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "sin");
    } else if (strcmp(callee->import_name, "vircon_cpu_acos") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "acos");
    } else if (strcmp(callee->import_name, "vircon_cpu_log") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "log");
    } else if (strcmp(callee->import_name, "vircon_cpu_pow") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "pow");
    } else if (strcmp(callee->import_name, "vircon_cpu_fmod") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "fmod");
    } else if (strcmp(callee->import_name, "vircon_cpu_imin") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "imin");
    } else if (strcmp(callee->import_name, "vircon_cpu_imax") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "imax");
    } else if (strcmp(callee->import_name, "vircon_cpu_iabs") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "iabs");
    } else if (strcmp(callee->import_name, "vircon_cpu_fmin") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "fmin");
    } else if (strcmp(callee->import_name, "vircon_cpu_fmax") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "fmax");
    } else if (strcmp(callee->import_name, "vircon_cpu_fabs") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "fabs");
    } else if (strcmp(callee->import_name, "vircon_cpu_floor") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "flr");
    } else if (strcmp(callee->import_name, "vircon_cpu_ceil") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "ceil");
    } else if (strcmp(callee->import_name, "vircon_cpu_round") == 0) {
      return lower_cpu_unary(context, &arguments[0], value, "round");
    } else if (strcmp(callee->import_name, "vircon_cpu_atan2") == 0) {
      return lower_cpu_binary(context, &arguments[0], &arguments[1], value, "atan2");
    } else if (strcmp(callee->import_name, "vircon_cpu_halt") == 0) {
      if (!emit(context, "  hlt"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_input_select_gamepad") == 0) {
      if (!load_value(context, 1, arguments[0]) || !emit(context, "  out INP_SelectedGamepad, R1"))
        return false;
    } else if (strcmp(callee->import_name, "vircon_input_get_selected_gamepad") == 0) {
      return lower_port_read(context, value, "INP_SelectedGamepad");
    } else if (strcmp(callee->import_name, "vircon_timer_get_cycle_counter") == 0) {
      return lower_port_read(context, value, "TIM_CycleCounter");
    } else if (strcmp(callee->import_name, "vircon_input_gamepad_left") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_right") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_up") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_down") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_connected") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_a") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_b") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_x") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_y") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_l") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_r") == 0 ||
               strcmp(callee->import_name, "vircon_input_gamepad_button_start") == 0 ||
               strcmp(callee->import_name, "vircon_timer_get_frame_counter") == 0 ||
               strcmp(callee->import_name, "vircon_timer_get_current_time") == 0 ||
               strcmp(callee->import_name, "vircon_timer_get_current_date") == 0 ||
               strcmp(callee->import_name, "vircon_rng_get_current_value") == 0 ||
               strcmp(callee->import_name, "vircon_spu_get_channel_state") == 0 ||
               strcmp(callee->import_name, "vircon_memcard_is_connected") == 0) {
      const char *port = strcmp(callee->import_name, "vircon_input_gamepad_left") == 0        ? "INP_GamepadLeft"
                         : strcmp(callee->import_name, "vircon_input_gamepad_right") == 0     ? "INP_GamepadRight"
                         : strcmp(callee->import_name, "vircon_input_gamepad_up") == 0        ? "INP_GamepadUp"
                         : strcmp(callee->import_name, "vircon_input_gamepad_down") == 0      ? "INP_GamepadDown"
                         : strcmp(callee->import_name, "vircon_input_gamepad_connected") == 0 ? "INP_GamepadConnected"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_a") == 0  ? "INP_GamepadButtonA"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_b") == 0  ? "INP_GamepadButtonB"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_x") == 0  ? "INP_GamepadButtonX"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_y") == 0  ? "INP_GamepadButtonY"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_l") == 0  ? "INP_GamepadButtonL"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_r") == 0  ? "INP_GamepadButtonR"
                         : strcmp(callee->import_name, "vircon_input_gamepad_button_start") == 0
                             ? "INP_GamepadButtonStart"
                         : strcmp(callee->import_name, "vircon_timer_get_current_time") == 0 ? "TIM_CurrentTime"
                         : strcmp(callee->import_name, "vircon_timer_get_current_date") == 0 ? "TIM_CurrentDate"
                         : strcmp(callee->import_name, "vircon_rng_get_current_value") == 0  ? "RNG_CurrentValue"
                         : strcmp(callee->import_name, "vircon_memcard_is_connected") == 0   ? "MEM_Connected"
                         : strcmp(callee->import_name, "vircon_spu_get_channel_state") == 0  ? "SPU_ChannelState"
                                                                                             : "TIM_FrameCounter";
      int slot = temp_slot(context);
      if (slot == 0 || !emit(context, "  in R0, %s", port) || !store_slot(context, slot, 0))
        return false;
      value->slot = slot;
      value->present = true;
      return true;
    } else {
      diagnostics_error(context->diagnostics, "internal error: unsupported import passed validation");
      return false;
    }
    for (index = expression->child_count; index != 0; --index)
      release(context, arguments[index - 1]);
    value->present = false;
    return true;
  }
  for (index = 0; index < expression->child_count; ++index)
    if (!load_value(context, 1, arguments[index]) || !emit(context, "  mov [SP+%zu], R1", index)) {
      if (heap_arguments)
        free(arguments);
      return false;
    }
  for (index = expression->child_count; index != 0; --index)
    release(context, arguments[index - 1]);
  function_label(context->validated->module, callee, label, sizeof(label));
  if (!emit(context, "  call %s", label)) {
    if (heap_arguments)
      free(arguments);
    return false;
  }
  context->target_word_address_cached = false;
  if (callee->result == WASM_VALUE_I32 || callee->result == WASM_VALUE_F32) {
    int slot = temp_slot(context);
    if (slot == 0 || !store_slot(context, slot, 0)) {
      if (heap_arguments)
        free(arguments);
      return false;
    }
    value->slot = slot;
    value->present = true;
  } else
    value->present = false;
  if (heap_arguments)
    free(arguments);
  return true;
}

/* Returns the target opcode for integer operations that accept a literal
 * right operand without first consuming another target register. */
static const char *binary_immediate_opcode(WasmBinaryOp operation) {
  switch (operation) {
  case WASM_BINARY_ADD:
    return "iadd";
  case WASM_BINARY_SUB:
    return "isub";
  case WASM_BINARY_MUL:
    return "imul";
  case WASM_BINARY_AND:
    return "and";
  case WASM_BINARY_OR:
    return "or";
  case WASM_BINARY_XOR:
    return "xor";
  case WASM_BINARY_EQ:
    return "ieq";
  case WASM_BINARY_NE:
    return "ine";
  case WASM_BINARY_LT_S:
    return "ilt";
  case WASM_BINARY_GT_S:
    return "igt";
  case WASM_BINARY_GE_S:
    return "ige";
  case WASM_BINARY_LE_S:
    return "ile";
  default:
    return NULL;
  }
}

/* Retains only affine pointer identities that ordinary i32 arithmetic proves
 * exactly under Wasm's wrapping arithmetic. */
static void set_binary_address_identity(Value *result, const Value *left, const Value *right,
                                        WasmBinaryOp operation) {
  result->has_address_identity = false;
  if (left->has_address_identity && right->is_immediate &&
      (operation == WASM_BINARY_ADD || operation == WASM_BINARY_SUB)) {
    result->has_address_identity = true;
    result->address_base_local = left->address_base_local;
    result->address_offset = operation == WASM_BINARY_ADD ? left->address_offset + right->immediate
                                                          : left->address_offset - right->immediate;
  } else if (operation == WASM_BINARY_ADD && left->is_immediate && right->has_address_identity) {
    result->has_address_identity = true;
    result->address_base_local = right->address_base_local;
    result->address_offset = right->address_offset + left->immediate;
  }
}

/* Matches the stable limit captured by an active loop-version guard. The
 * guarded fast copy may reuse R10 because the loop scan rejected assignments
 * to local limits and rejected stores when the limit resides in memory. */
static bool is_cached_fast_loop_bound(const Context *context, const WasmExpr *expression) {
  LoopBoundKind kind;
  uint32_t value;
  return context->fast_bound_active && parse_loop_bound(context, expression, &kind, &value) &&
         kind == context->fast_bound_kind && value == context->fast_bound_value;
}

/* Lowers one supported typed binary operation through the compiler value model,
 * retaining a right-hand integer constant as a target immediate when legal. */
static bool lower_binary(Context *context, const WasmExpr *expression, Value *value) {
  Value left = {0}, left_source = {0}, right = {0};
  char normal[64], done[64];
  const char *immediate_opcode;
  bool cached_loop_bound = expression->binary_op == WASM_BINARY_LT_S &&
                           is_cached_fast_loop_bound(context, expression->children[1]);
  if (!lower_expression(context, expression->children[0], &left) || !left.present)
    return false;
  /* A constant right operand cannot mutate a borrowed local. Other right
   * expressions may contain local.set/calls, so preserve Wasm's left-first
   * value before lowering them. */
  if (expression->children[1]->kind != WASM_EXPR_I32_CONST &&
      expression->children[1]->kind != WASM_EXPR_F32_CONST && !materialize_borrowed(context, &left))
    return false;
  left_source = left;
  if (!reserve_value_slot(context, &left))
    return false;
  if (cached_loop_bound) {
    if (!load_value(context, 1, left_source) || !emit(context, "  ilt R1, R10") ||
        (!retain_result_register(context, &left, expression->value_type) && !store_slot(context, left.slot, 1)))
      return false;
    if (left.register_plus_one != 0) {
      *value = left;
      return true;
    }
    left.has_address_identity = false;
    *value = left;
    return true;
  }
  if (!lower_expression(context, expression->children[1], &right) || !right.present)
    return false;
  immediate_opcode = right.is_immediate ? binary_immediate_opcode(expression->binary_op) : NULL;
  if (immediate_opcode != NULL) {
    if (!load_value(context, 1, left_source) ||
        !emit(context, "  %s R1, 0x%08X", immediate_opcode, right.immediate))
      return false;
    if (retain_result_register(context, &left, expression->value_type)) {
      release(context, right);
      *value = left;
      return true;
    }
    if (!store_slot(context, left.slot, 1))
      return false;
    set_binary_address_identity(&left, &left_source, &right, expression->binary_op);
    *value = left;
    return true;
  }
  /* A nested right expression may itself be retained in R1. Preserve it in
   * R2 before restoring the left operand into R1; doing these loads in the
   * opposite order would silently replace both operands with the left value. */
  if (right.register_plus_one == 2) {
    if (!load_value(context, 2, right) || !load_value(context, 1, left_source))
      return false;
  } else if (!load_value(context, 1, left_source) || !load_value(context, 2, right))
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
    if (!fresh_label(context, "div_s_normal", normal, sizeof(normal)) || !emit(context, "  mov R3, R2") ||
        !emit(context, "  ieq R3, 0") || !emit(context, "  jt R3, __wasm_trap") || !emit(context, "  mov R3, R1") ||
        !emit(context, "  ieq R3, 0x80000000") || !emit(context, "  jf R3, %s", normal) ||
        !emit(context, "  mov R3, R2") || !emit(context, "  ieq R3, -1") || !emit(context, "  jt R3, __wasm_trap") ||
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
    if (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") ||
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
  case WASM_BINARY_F32_EQ:
    if (!emit(context, "  feq R1, R2"))
      return false;
    break;
  case WASM_BINARY_F32_NE:
    if (!emit(context, "  fne R1, R2"))
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
  case WASM_BINARY_F32_GE:
    if (!emit(context, "  fge R1, R2"))
      return false;
    break;
  case WASM_BINARY_LT_U:
    if (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  ilt R1, R2"))
      return false;
    break;
  case WASM_BINARY_GT_U:
    if (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  igt R1, R2"))
      return false;
    break;
  case WASM_BINARY_GE_U:
    if (!emit(context, "  xor R1, 0x80000000") || !emit(context, "  xor R2, 0x80000000") ||
        !emit(context, "  ige R1, R2"))
      return false;
    break;
  case WASM_BINARY_SHR_U:
    if (!emit(context, "  and R2, 31") || !emit(context, "  mov R3, 0") || !emit(context, "  isub R3, R2") ||
        !emit(context, "  shl R1, R3"))
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
  case WASM_BINARY_ROTL:
    if (!emit_i32_rotate(context, true))
      return false;
    break;
  case WASM_BINARY_ROTR:
    if (!emit_i32_rotate(context, false))
      return false;
    break;
  case WASM_BINARY_DIV_U:
    if (!emit(context, "  call __wasm_i32_div_u") || !store_slot(context, left.slot, 0))
      return false;
    context->target_word_address_cached = false;
    left.has_address_identity = false;
    release(context, right);
    *value = left;
    return true;
  case WASM_BINARY_REM_U:
    if (!emit(context, "  call __wasm_i32_div_u") || !load_slot(context, 1, left.slot) ||
        !emit(context, "  imul R0, R2") || !emit(context, "  isub R1, R0"))
      return false;
    context->target_word_address_cached = false;
    break;
  case WASM_BINARY_REM_S:
    if (!fresh_label(context, "rem_s_normal", normal, sizeof(normal)) ||
        !fresh_label(context, "rem_s_done", done, sizeof(done)) || !emit(context, "  mov R3, R2") ||
        !emit(context, "  ieq R3, 0") || !emit(context, "  jt R3, __wasm_trap") || !emit(context, "  ieq R3, -1") ||
        !emit(context, "  jf R3, %s", normal) || !emit(context, "  mov R1, 0") || !emit(context, "  jmp %s", done) ||
        !emit_label(context, normal) || !emit(context, "  imod R1, R2") || !emit_label(context, done))
      return false;
    break;
  case WASM_BINARY_I64_ADD:
  case WASM_BINARY_I64_SUB:
  case WASM_BINARY_I64_MUL:
  case WASM_BINARY_I64_AND:
  case WASM_BINARY_I64_OR:
  case WASM_BINARY_I64_XOR:
  case WASM_BINARY_I64_SHL:
  case WASM_BINARY_I64_SHR_U:
  case WASM_BINARY_I64_SHR_S:
  case WASM_BINARY_I64_EQ:
  case WASM_BINARY_I64_NE:
  case WASM_BINARY_I64_LT_S:
  case WASM_BINARY_I64_LT_U:
  case WASM_BINARY_I64_LE_S:
  case WASM_BINARY_I64_LE_U:
  case WASM_BINARY_I64_GT_S:
  case WASM_BINARY_I64_GT_U:
  case WASM_BINARY_I64_GE_S:
  case WASM_BINARY_I64_GE_U:
    diagnostics_error(context->diagnostics, "internal error: i64 binary operation bypassed pair lowering");
    return false;
  case WASM_BINARY_OTHER:
    diagnostics_error(context->diagnostics, "internal error: unsupported binary operation passed validation");
    return false;
  }
  if (retain_result_register(context, &left, expression->value_type)) {
    release(context, right);
    *value = left;
    return true;
  }
  if (!store_slot(context, left.slot, 1))
    return false;
  set_binary_address_identity(&left, &left_source, &right, expression->binary_op);
  release(context, right);
  *value = left;
  return true;
}

/* Recursively detects one binary operation in an expression tree. */
static bool expression_uses_binary(const WasmExpr *expression, WasmBinaryOp operation) {
  size_t index;
  if (expression->kind == WASM_EXPR_BINARY && expression->binary_op == operation)
    return true;
  for (index = 0; index < expression->child_count; ++index)
    if (expression_uses_binary(expression->children[index], operation))
      return true;
  return false;
}

/* Returns whether a reachable function needs a compiler-generated bulk helper.
 */
static bool expression_uses_kind(const WasmExpr *expression, WasmExprKind kind) {
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
  return emit_label(context, "__wasm_i32_div_u") && emit(context, "  mov R3, R2") && emit(context, "  ieq R3, 0") &&
         emit(context, "  jt R3, __wasm_trap") && emit(context, "  mov R3, 0") && emit(context, "  mov R4, 0") &&
         emit(context, "  mov R5, 32") && emit_label(context, "__wasm_i32_div_u_loop") &&
         emit(context, "  mov R6, R1") && emit(context, "  mov R7, -31") && emit(context, "  shl R6, R7") &&
         /* Vircon32 right shifts are arithmetic. Mask the extracted sign bit
          * so the restoring divider appends 0 or 1, never 0xFFFFFFFF. */
         emit(context, "  and R6, 1") && emit(context, "  shl R1, 1") && emit(context, "  shl R4, 1") &&
         emit(context, "  or R4, R6") &&
         emit(context, "  mov R6, R4") && emit(context, "  xor R6, 0x80000000") && emit(context, "  mov R7, R2") &&
         emit(context, "  xor R7, 0x80000000") && emit(context, "  ige R6, R7") &&
         emit(context, "  jf R6, __wasm_i32_div_u_skip") && emit(context, "  isub R4, R2") &&
         emit(context, "  shl R3, 1") && emit(context, "  or R3, 1") &&
         emit(context, "  jmp __wasm_i32_div_u_decrement") && emit_label(context, "__wasm_i32_div_u_skip") &&
         emit(context, "  shl R3, 1") && emit_label(context, "__wasm_i32_div_u_decrement") &&
         emit(context, "  isub R5, 1") && emit(context, "  jt R5, __wasm_i32_div_u_loop") &&
         emit(context, "  mov R0, R3") && emit(context, "  ret");
}

/* Emits the shared bounds checks used before either bulk helper mutates RAM. */
static bool emit_bulk_bounds_checks(Context *context, bool has_source) {
  if (!emit(context, "  mov R2, [%u]", VIRCON_WASM_MEMORY_PAGES_WORD) || !emit(context, "  imul R2, 65536") ||
      !emit(context, "  mov R1, [BP-1]") || !emit(context, "  ilt R1, 0") || !emit(context, "  jt R1, __wasm_trap") ||
      !emit(context, "  igt R1, R2") || !emit(context, "  jt R1, __wasm_trap") || !emit(context, "  mov R3, R2") ||
      !emit(context, "  isub R3, R1") || !emit(context, "  mov R1, [BP-3]") || !emit(context, "  ilt R1, 0") ||
      !emit(context, "  jt R1, __wasm_trap") || !emit(context, "  igt R1, R3") || !emit(context, "  jt R1, __wasm_trap"))
    return false;
  if (!has_source)
    return true;
  return emit(context, "  mov R1, [BP-2]") && emit(context, "  ilt R1, 0") && emit(context, "  jt R1, __wasm_trap") &&
         emit(context, "  igt R1, R2") && emit(context, "  jt R1, __wasm_trap") && emit(context, "  mov R3, R2") &&
         emit(context, "  isub R3, R1") && emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  igt R1, R3") && emit(context, "  jt R1, __wasm_trap");
}

/* Emits overlap-safe Wasm memory.copy over packed byte-addressed linear memory.
 */
static bool emit_memory_copy_helper(Context *context) {
  return emit_label(context, "__wasm_memory_copy") && emit(context, "  push BP") && emit(context, "  mov BP, SP") &&
         emit(context, "  isub SP, 3") && emit(context, "  mov R1, [BP+2]") && emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP+3]") && emit(context, "  mov [BP-2], R1") && emit(context, "  mov R1, [BP+4]") &&
         emit(context, "  mov [BP-3], R1") && emit_bulk_bounds_checks(context, true) &&
         /* Copy backward only when destination starts inside the source range.
          */
         emit(context, "  mov R1, [BP-1]") && emit(context, "  mov R2, [BP-2]") && emit(context, "  igt R1, R2") &&
         emit(context, "  jf R1, __wasm_memory_copy_forward") && emit(context, "  mov R1, [BP-2]") &&
         emit(context, "  mov R2, [BP-3]") && emit(context, "  iadd R1, R2") && emit(context, "  mov R2, [BP-1]") &&
         emit(context, "  ilt R2, R1") && emit(context, "  jt R2, __wasm_memory_copy_backward") &&
         emit_label(context, "__wasm_memory_copy_forward") && emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  jf R1, __wasm_memory_copy_done") && emit_label(context, "__wasm_memory_copy_forward_loop") &&
         /* Read source byte R4. */
         emit(context, "  mov R1, [BP-2]") && emit(context, "  mov R2, R1") && emit(context, "  and R2, 3") &&
         emit(context, "  imul R2, -8") && emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") && emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R4, [R3]") && emit(context, "  shl R4, R2") && emit(context, "  and R4, 0x000000FF") &&
         /* Insert R4 into the destination byte lane. */
         emit(context, "  mov R1, [BP-1]") && emit(context, "  mov R2, R1") && emit(context, "  and R2, 3") &&
         emit(context, "  imul R2, 8") && emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") &&
         emit(context, "  shl R3, R5") && emit(context, "  iadd R3, %u", LINEAR_BASE) &&
         emit(context, "  mov R5, [R3]") && emit(context, "  shl R4, R2") && emit(context, "  mov R6, 0x000000FF") &&
         emit(context, "  shl R6, R2") && emit(context, "  xor R6, 0xFFFFFFFF") && emit(context, "  and R5, R6") &&
         emit(context, "  or R5, R4") && emit(context, "  mov [R3], R5") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  iadd R1, 1") && emit(context, "  mov [BP-1], R1") && emit(context, "  mov R1, [BP-2]") &&
         emit(context, "  iadd R1, 1") && emit(context, "  mov [BP-2], R1") && emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  isub R1, 1") && emit(context, "  mov [BP-3], R1") &&
         emit(context, "  jt R1, __wasm_memory_copy_forward_loop") && emit(context, "  jmp __wasm_memory_copy_done") &&
         emit_label(context, "__wasm_memory_copy_backward") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  mov R2, [BP-3]") && emit(context, "  iadd R1, R2") && emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP-2]") && emit(context, "  iadd R1, R2") && emit(context, "  mov [BP-2], R1") &&
         emit_label(context, "__wasm_memory_copy_backward_loop") && emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  jf R1, __wasm_memory_copy_done") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  isub R1, 1") && emit(context, "  mov [BP-1], R1") && emit(context, "  mov R1, [BP-2]") &&
         emit(context, "  isub R1, 1") && emit(context, "  mov [BP-2], R1") && emit(context, "  mov R2, R1") &&
         emit(context, "  and R2, 3") && emit(context, "  imul R2, -8") && emit(context, "  mov R3, R1") &&
         emit(context, "  mov R5, -2") && emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) && emit(context, "  mov R4, [R3]") &&
         emit(context, "  shl R4, R2") && emit(context, "  and R4, 0x000000FF") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  mov R2, R1") && emit(context, "  and R2, 3") && emit(context, "  imul R2, 8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") && emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) && emit(context, "  mov R5, [R3]") &&
         emit(context, "  shl R4, R2") && emit(context, "  mov R6, 0x000000FF") && emit(context, "  shl R6, R2") &&
         emit(context, "  xor R6, 0xFFFFFFFF") && emit(context, "  and R5, R6") && emit(context, "  or R5, R4") &&
         emit(context, "  mov [R3], R5") && emit(context, "  mov R1, [BP-3]") && emit(context, "  isub R1, 1") &&
         emit(context, "  mov [BP-3], R1") && emit(context, "  jmp __wasm_memory_copy_backward_loop") &&
         emit_label(context, "__wasm_memory_copy_done") && emit(context, "  mov SP, BP") && emit(context, "  pop BP") &&
         emit(context, "  ret");
}

/* Emits Wasm memory.fill over packed byte-addressed linear memory. */
static bool emit_memory_fill_helper(Context *context) {
  return emit_label(context, "__wasm_memory_fill") && emit(context, "  push BP") && emit(context, "  mov BP, SP") &&
         emit(context, "  isub SP, 3") && emit(context, "  mov R1, [BP+2]") && emit(context, "  mov [BP-1], R1") &&
         emit(context, "  mov R1, [BP+3]") && emit(context, "  mov [BP-2], R1") && emit(context, "  mov R1, [BP+4]") &&
         emit(context, "  mov [BP-3], R1") && emit_bulk_bounds_checks(context, false) &&
         emit_label(context, "__wasm_memory_fill_loop") && emit(context, "  mov R1, [BP-3]") &&
         emit(context, "  jf R1, __wasm_memory_fill_done") && emit(context, "  mov R1, [BP-1]") &&
         emit(context, "  mov R2, R1") && emit(context, "  and R2, 3") && emit(context, "  imul R2, 8") &&
         emit(context, "  mov R3, R1") && emit(context, "  mov R5, -2") && emit(context, "  shl R3, R5") &&
         emit(context, "  iadd R3, %u", LINEAR_BASE) && emit(context, "  mov R4, [BP-2]") &&
         emit(context, "  and R4, 0x000000FF") && emit(context, "  shl R4, R2") && emit(context, "  mov R5, [R3]") &&
         emit(context, "  mov R6, 0x000000FF") && emit(context, "  shl R6, R2") &&
         emit(context, "  xor R6, 0xFFFFFFFF") && emit(context, "  and R5, R6") && emit(context, "  or R5, R4") &&
         emit(context, "  mov [R3], R5") && emit(context, "  mov R1, [BP-1]") && emit(context, "  iadd R1, 1") &&
         emit(context, "  mov [BP-1], R1") && emit(context, "  mov R1, [BP-3]") && emit(context, "  isub R1, 1") &&
         emit(context, "  mov [BP-3], R1") && emit(context, "  jmp __wasm_memory_fill_loop") &&
         emit_label(context, "__wasm_memory_fill_done") && emit(context, "  mov SP, BP") && emit(context, "  pop BP") &&
         emit(context, "  ret");
}

/* Implements one validated expression after diagnostic-path tracking begins. */
static bool lower_expression_impl(Context *context, const WasmExpr *expression, Value *value) {
  Value left = {0}, right = {0}, condition = {0}, input = {0};
  char label[64], end[64], false_label[64];
  size_t index;
  *value = (Value){0};
  switch (expression->kind) {
  case WASM_EXPR_I32_CONST:
    value->immediate = (uint32_t)expression->i32_value;
    value->type = WASM_VALUE_I32;
    value->present = true;
    value->is_immediate = true;
    return true;
  case WASM_EXPR_I64_CONST: {
    Value result = {0};
    if (!reserve_i64_value(context, &result) || !emit(context, "  mov R1, 0x%08X", (uint32_t)expression->i64_value) ||
        !store_slot(context, result.slot, 1) ||
        !emit(context, "  mov R1, 0x%08X", (uint32_t)(expression->i64_value >> 32)) ||
        !store_slot(context, result.high_slot, 1))
      return false;
    *value = result;
    return true;
  }
  case WASM_EXPR_F32_CONST: {
    union {
      float value;
      uint32_t bits;
    } constant;
    constant.value = expression->f32_value;
    value->immediate = constant.bits;
    value->type = WASM_VALUE_F32;
    value->present = true;
    value->is_immediate = true;
    return true;
  }
  case WASM_EXPR_LOCAL_GET: {
    if (local_value_type(context->function, expression->index) == WASM_VALUE_I64) {
      Value result = {0};
      if (!reserve_i64_value(context, &result) || !load_slot(context, 1, local_slot(context->function, expression->index)) ||
          !store_slot(context, result.slot, 1) || !load_slot(context, 1, i64_local_high_slot(context->function, expression->index)) ||
          !store_slot(context, result.high_slot, 1))
        return false;
      *value = result;
      return true;
    }
    value->slot = local_slot(context->function, expression->index);
    value->present = true;
    value->is_borrowed = true;
    value->type = local_value_type(context->function, expression->index);
    value->has_address_identity = true;
    value->address_base_local = context->local_address_bases[expression->index];
    value->address_offset = context->local_address_offsets[expression->index];
    return true;
  }
  case WASM_EXPR_LOCAL_SET:
    if (!lower_expression(context, expression->children[0], &left) || !left.present)
      return false;
    if (local_value_type(context->function, expression->index) == WASM_VALUE_I64) {
      if (left.type != WASM_VALUE_I64 || !load_slot(context, 1, left.slot) ||
          !store_slot(context, local_slot(context->function, expression->index), 1) || !load_slot(context, 1, left.high_slot) ||
          !store_slot(context, i64_local_high_slot(context->function, expression->index), 1))
        return false;
    } else if (!load_value(context, 1, left) || !store_slot(context, local_slot(context->function, expression->index), 1))
      return false;
    /* Keep the guarded native pointer synchronized with the actual Wasm
     * induction-local assignment. Updating here, rather than at a branch,
     * remains correct even when structured control has multiple backedges. */
    if (context->fast_word_pointer_active && expression->index == context->fast_word_pointer_base_local &&
        !emit(context, "  iadd R13, %u", context->fast_word_pointer_stride))
      return false;
    if (local_value_type(context->function, expression->index) == WASM_VALUE_I32)
      assign_local_address_identity(context, expression->index, &left);
    if (expression->is_tee) {
      if (!materialize_borrowed(context, &left))
        return false;
      *value = left;
      return true;
    }
    release(context, left);
    return true;
  case WASM_EXPR_STACK_POINTER_GET: {
    int slot = temp_slot(context);
    if (slot == 0 || !emit(context, "  mov R1, [%u]", VIRCON_WASM_STACK_POINTER_WORD) || !store_slot(context, slot, 1))
      return false;
    value->slot = slot;
    value->present = true;
    return true;
  }
  case WASM_EXPR_STACK_POINTER_SET:
    if (!lower_expression(context, expression->children[0], &left) || !left.present ||
        !load_value(context, 1, left) || !emit(context, "  mov [%u], R1", VIRCON_WASM_STACK_POINTER_WORD))
      return false;
    release(context, left);
    return true;
  case WASM_EXPR_UNARY:
    if (expression->unary_op == WASM_UNARY_I64_EQZ)
      return lower_i64_eqz(context, expression, value);
    if (expression->unary_op == WASM_UNARY_I32_WRAP_I64)
      return lower_i32_wrap_i64(context, expression, value);
    if (expression->unary_op == WASM_UNARY_I64_EXTEND_I32_S || expression->unary_op == WASM_UNARY_I64_EXTEND_I32_U)
      return lower_i64_extend_i32(context, expression, value,
                                  expression->unary_op == WASM_UNARY_I64_EXTEND_I32_S);
    if (!lower_expression(context, expression->children[0], &left) || !left.present)
      return false;
    if (expression->unary_op == WASM_UNARY_REINTERPRET_F32_TO_I32 ||
        expression->unary_op == WASM_UNARY_REINTERPRET_I32_TO_F32) {
      left.type = expression->value_type;
      left.has_address_identity = false;
      *value = left;
      return true;
    }
    input = left;
    if (!reserve_value_slot(context, &left) || !load_value(context, 1, input))
      return false;
    if (expression->unary_op == WASM_UNARY_EQZ) {
      if (!emit(context, "  ieq R1, 0"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_EXTEND8_S) {
      if (!emit_i32_extend_s(context, 8))
        return false;
    } else if (expression->unary_op == WASM_UNARY_EXTEND16_S) {
      if (!emit_i32_extend_s(context, 16))
        return false;
    } else if (expression->unary_op == WASM_UNARY_CONVERT_I32_S_TO_F32) {
      if (!emit(context, "  cif R1"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_CONVERT_I32_U_TO_F32) {
      if (!emit_f32_convert_i32_u(context))
        return false;
    } else if (expression->unary_op == WASM_UNARY_TRUNC_SAT_F32_TO_I32) {
      char special[64], nan[64], done[64];
      bool address_was_cached = context->target_word_address_cached;
      uint32_t cached_base_local = context->target_word_address_base_local;
      uint32_t cached_offset = context->target_word_address_offset;
      /* Values below +2^31 can be clamped at -2^31 and converted directly.
       * The false comparison path combines the upper saturation and NaN
       * cases; NaN is the only value unequal to itself. */
      if (!fresh_label(context, "trunc_special", special, sizeof(special)) ||
          !fresh_label(context, "trunc_nan", nan, sizeof(nan)) ||
          !fresh_label(context, "trunc_done", done, sizeof(done)) || !emit(context, "  mov R2, 0x4F000000") ||
          !emit(context, "  fgt R2, R1") || !emit(context, "  jf R2, %s", special) ||
          !emit(context, "  fmax R1, 0xCF000000") || !emit(context, "  cfi R1") ||
          !emit(context, "  jmp %s", done) || !emit_label(context, special) || !emit(context, "  mov R2, R1") ||
          !emit(context, "  feq R2, R1") || !emit(context, "  jf R2, %s", nan) ||
          !emit(context, "  mov R1, 0x7FFFFFFF") || !emit(context, "  jmp %s", done) || !emit_label(context, nan) ||
          !emit(context, "  mov R1, 0") || !emit_label(context, done))
        return false;
      /* These private branches alter only R1/R2. Every route to the merge
       * therefore preserves the target word address held in R13. */
      context->target_word_address_cached = address_was_cached;
      context->target_word_address_base_local = cached_base_local;
      context->target_word_address_offset = cached_offset;
    } else if (expression->unary_op == WASM_UNARY_F32_NEG) {
      if (!emit(context, "  fsgn R1"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_F32_ABS) {
      if (!emit(context, "  fabs R1"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_F32_FLOOR) {
      if (!emit(context, "  flr R1"))
        return false;
    } else if (expression->unary_op == WASM_UNARY_F32_CEIL) {
      if (!emit(context, "  ceil R1"))
        return false;
    } else
      return false;
    if (retain_result_register(context, &left, expression->value_type)) {
      *value = left;
      return true;
    }
    if (!store_slot(context, left.slot, 1))
      return false;
    left.has_address_identity = false;
    *value = left;
    return true;
  case WASM_EXPR_BINARY:
    if (expression->binary_op >= WASM_BINARY_I64_ADD && expression->binary_op <= WASM_BINARY_I64_GE_U)
      return lower_i64_binary(context, expression, value);
    return lower_binary(context, expression, value);
  case WASM_EXPR_SELECT:
    /* Children retain Wasm's evaluation order: first, second, condition. */
    if (!lower_expression(context, expression->children[0], &left) || !materialize_value(context, &left) ||
        !lower_expression(context, expression->children[1], &right) || !materialize_borrowed(context, &right) ||
        !lower_expression(context, expression->children[2], &condition) || !left.present || !right.present ||
        !condition.present || !fresh_label(context, "select_false", false_label, sizeof(false_label)) ||
        !fresh_label(context, "select_done", end, sizeof(end)) || !load_value(context, 1, condition))
      return false;
    if (expression->value_type == WASM_VALUE_I64) {
      /* Reuse the true pair and copy both false words only on the false path. */
      if (left.type != WASM_VALUE_I64 || right.type != WASM_VALUE_I64 || !emit(context, "  jf R1, %s", false_label) ||
          !emit(context, "  jmp %s", end) || !emit_label(context, false_label) || !load_slot(context, 1, right.slot) ||
          !store_slot(context, left.slot, 1) || !load_slot(context, 1, right.high_slot) ||
          !store_slot(context, left.high_slot, 1) || !emit_label(context, end))
        return false;
    } else if (!emit(context, "  jf R1, %s", false_label) || !load_value(context, 1, left) ||
               !emit(context, "  jmp %s", end) || !emit_label(context, false_label) || !load_value(context, 1, right) ||
               !emit_label(context, end) || !store_slot(context, left.slot, 1))
      return false;
    release(context, condition);
    release(context, right);
    if (!left.has_address_identity || !right.has_address_identity ||
        left.address_base_local != right.address_base_local || left.address_offset != right.address_offset)
      left.has_address_identity = false;
    *value = left;
    return true;
  case WASM_EXPR_LOAD:
    if (expression->bytes == 8 && expression->value_type == WASM_VALUE_I64)
      return lower_i64_load(context, expression, value);
    return lower_load(context, expression, value);
  case WASM_EXPR_STORE:
    if (expression->bytes == 8 && expression->value_type == WASM_VALUE_I64)
      return lower_i64_store(context, expression, value);
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
  case WASM_EXPR_MEMORY_SIZE:
    return lower_memory_size(context, value);
  case WASM_EXPR_MEMORY_GROW:
    return lower_memory_grow(context, expression, value);
  case WASM_EXPR_CALL:
    return lower_call(context, expression, value);
  case WASM_EXPR_CALL_INDIRECT:
    return lower_call_indirect(context, expression, value);
  case WASM_EXPR_NOP:
    value->present = false;
    return true;
  case WASM_EXPR_BLOCK:
    {
    Value result = {0};
    if (expression->name != NULL &&
        (expression->value_type == WASM_VALUE_I32 || expression->value_type == WASM_VALUE_F32)) {
      result.slot = temp_slot(context);
      if (result.slot == 0)
        return false;
      result.present = true;
    }
    if (expression->name != NULL) {
      if (!fresh_label(context, "block_end", label, sizeof(label)) ||
          !push_target(context, expression->name, label, result.slot))
        return false;
    }
    for (index = 0; index < expression->child_count; ++index) {
      if (value->present)
        release(context, *value);
      value->present = false;
      if (!lower_expression(context, expression->children[index], value))
        return false;
    }
    /* Store the ordinary fallthrough value before the branch target. A
     * value-carrying br has already stored its own result and must jump past
     * this copy; placing the copy after the label would overwrite immediate
     * branch values with the syntactic fallthrough expression. */
    if (result.present) {
      if (value->present && (!load_value(context, 1, *value) || !store_slot(context, result.slot, 1)))
        return false;
      release(context, *value);
      *value = result;
    }
    if (expression->name != NULL) {
      Target target = pop_target(context);
      if (!emit_label(context, target.label)) {
        free(target.label);
        return false;
      }
      free(target.label);
    }
    return true;
    }
  case WASM_EXPR_LOOP:
    {
    LoopFastPath fast_path = analyze_loop_fast_path(context, expression);
    if (fast_path.valid)
      return lower_versioned_loop(context, expression, &fast_path, value);
    if (!fresh_label(context, "loop", label, sizeof(label)) ||
        !push_target(context, expression->name, label, 0))
      return false;
    if (!emit_label(context, label) || !lower_expression(context, expression->children[0], value))
      return false;
    free(pop_target(context).label);
    value->present = false;
    return true;
    }
  case WASM_EXPR_BR: {
    const Target *branch_target = find_target(context, expression->name);
    const char *target = branch_target == NULL ? NULL : branch_target->label;
    int result_slot = branch_target == NULL ? 0 : branch_target->result_slot;
    if (expression->child_count == 1) {
      if (result_slot == 0 ||
          !lower_expression(context, expression->children[0], &left) || !left.present ||
          !load_value(context, 1, left) || !store_slot(context, result_slot, 1))
        return false;
      release(context, left);
    }
    if (target != NULL)
      return emit(context, "  jmp %s", target);
  }
    diagnostics_error(context->diagnostics, "branch targets '%s' outside active structured control", expression->name);
    return false;
  case WASM_EXPR_BR_IF:
    if (expression->child_count == 2) {
      const Target *branch_target = find_target(context, expression->name);
      const char *target = branch_target == NULL ? NULL : branch_target->label;
      int result_slot = branch_target == NULL ? 0 : branch_target->result_slot;
      int previous_result_register = context->preferred_result_register;
      bool lowered_condition;
      if (target == NULL || result_slot == 0 || !lower_expression(context, expression->children[0], &left) ||
          !left.present || !materialize_borrowed(context, &left))
        return false;
      context->preferred_result_register = 1;
      lowered_condition = lower_expression(context, expression->children[1], &condition);
      context->preferred_result_register = previous_result_register;
      if (!lowered_condition || !condition.present || !fresh_label(context, "br_if_fallthrough", end, sizeof(end)) ||
          !load_value(context, 1, condition) || !emit(context, "  jf R1, %s", end) || !load_value(context, 1, left) ||
          !store_slot(context, result_slot, 1) || !emit(context, "  jmp %s", target))
        return false;
      release(context, condition);
      if (!emit_label(context, end))
        return false;
      *value = left;
      return true;
    }
    {
      int previous_result_register = context->preferred_result_register;
      bool lowered_condition;
      context->preferred_result_register = 1;
      lowered_condition = lower_expression(context, expression->children[0], &condition);
      context->preferred_result_register = previous_result_register;
      if (!lowered_condition || !condition.present ||
        !load_value(context, 1, condition))
        return false;
    }
    release(context, condition);
    {
      const char *target = find_target_label(context, expression->name);
      if (target != NULL)
        return emit(context, "  jt R1, %s", target);
    }
    diagnostics_error(context->diagnostics, "conditional branch targets '%s' outside active structured control",
                      expression->name);
    return false;
  case WASM_EXPR_BR_TABLE:
    return lower_br_table(context, expression, value);
  case WASM_EXPR_IF:
    return lower_if(context, expression, value);
  case WASM_EXPR_RETURN:
    if (expression->child_count != 0) {
      if (!lower_expression(context, expression->children[0], &left) || !left.present ||
          !load_value(context, 0, left))
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
    diagnostics_error(context->diagnostics, "internal error: unsupported global passed validation");
    return false;
  }
  diagnostics_error(context->diagnostics, "internal error: unhandled Wasm expression");
  return false;
}

/* Tracks the deepest expression responsible for a lowering failure. */
static bool lower_expression(Context *context, const WasmExpr *expression, Value *value) {
  const WasmExpr *previous = context->current_expression;
  context->current_expression = expression;
  if (!lower_expression_impl(context, expression, value))
    return false;
  context->current_expression = previous;
  return true;
}

/* Packs active Wasm data segments into writable target RAM at startup. */
static bool initialize_data(const ValidatedModule *validated, Context *context) {
  size_t bytes = (size_t)context->memory_bytes, words = (bytes + 3) / 4, index, segment_index;
  unsigned char *memory = calloc(bytes, 1);
  bool *touched = calloc(words, sizeof(*touched));
  if (memory == NULL || touched == NULL) {
    free(memory);
    free(touched);
    diagnostics_error(context->diagnostics, "out of memory constructing initial linear memory");
    return false;
  }
  for (segment_index = 0; segment_index < validated->module->data_segment_count; ++segment_index) {
    const WasmDataSegment *segment = &validated->module->data_segments[segment_index];
    memcpy(memory + segment->offset, segment->bytes, segment->size);
    for (index = segment->offset / 4; index <= (segment->offset + segment->size - 1) / 4 && segment->size != 0; ++index)
      touched[index] = true;
  }
  for (index = 0; index < words; ++index)
    if (touched[index]) {
      uint32_t word = (uint32_t)memory[index * 4] | ((uint32_t)memory[index * 4 + 1] << 8) |
                      ((uint32_t)memory[index * 4 + 2] << 16) | ((uint32_t)memory[index * 4 + 3] << 24);
      if (!emit(context, "  mov R1, 0x%08X", word) || !emit(context, "  mov [%u], R1", LINEAR_BASE + (unsigned)index)) {
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
static bool lower_function(const ValidatedModule *validated, const WasmFunction *function, VirconIrProgram *program,
                           Diagnostics *diagnostics, uint32_t memory_bytes, bool memory_can_grow) {
  Context context = {0};
  Value result = {0};
  char label[64];
  bool success = false;
  size_t outgoing_slots, frame_slots, local_words;
  unsigned errors_before = diagnostics->errors;
  context.validated = validated;
  context.function = function;
  context.program = program;
  context.diagnostics = diagnostics;
  context.memory_bytes = memory_bytes;
  context.memory_can_grow = memory_can_grow;
  context.preferred_result_register = -1;
  if (!initialize_local_alignments(&context))
    goto done;
  outgoing_slots = outgoing_call_slots(validated->module, function->body);
  local_words = local_storage_words(function);
  frame_slots = local_words;
  if (frame_slots > SIZE_MAX - TEMP_SLOTS || outgoing_slots > SIZE_MAX - frame_slots - TEMP_SLOTS) {
    diagnostics_error(diagnostics, "function %zu requires an unrepresentable stack frame",
                      function_index(validated->module, function));
    goto done;
  }
  frame_slots += TEMP_SLOTS + outgoing_slots;
  if (frame_slots > UINT32_MAX) {
    diagnostics_error(diagnostics, "function %zu requires a stack frame larger than Vircon32 can address",
                      function_index(validated->module, function));
    goto done;
  }
  function_label(validated->module, function, label, sizeof(label));
  snprintf(context.return_label, sizeof(context.return_label), "__wasm_return_%zu",
           function_index(validated->module, function));
  if (!emit_label(&context, label) || !emit(&context, "  push BP") || !emit(&context, "  mov BP, SP") ||
      !emit(&context, "  isub SP, %zu", frame_slots) || !initialize_function_locals(&context, local_words) ||
      !lower_expression(&context, function->body, &result))
    goto done;
  if (function->result == WASM_VALUE_I32 || function->result == WASM_VALUE_F32) {
    if (result.present) {
      if (!load_value(&context, 0, result))
        goto done;
    } else if (!emit(&context, "  mov R0, 0"))
      goto done;
  }
  release(&context, result);
  if (!emit_label(&context, context.return_label) || !emit(&context, "  mov SP, BP") || !emit(&context, "  pop BP") ||
      !emit(&context, "  ret") || !emit_jump_tables(&context))
    goto done;
  success = true;
done:
  if (!success && diagnostics->errors == errors_before) {
    size_t index = function_index(validated->module, function);
    const WasmExpr *expression = context.current_expression;
    if (expression != NULL && function->diagnostic_name != NULL && function->diagnostic_name[0] != '\0') {
      if (function->source_file != NULL)
        diagnostics_error(
            diagnostics,
            "internal compiler error lowering Wasm %s in function %zu '%s' at %s:%u:%u, expression path %s",
            expression->opcode, index, function->diagnostic_name, function->source_file, function->source_line,
            function->source_column, expression->path);
      else
        diagnostics_error(diagnostics,
                          "internal compiler error lowering Wasm %s in function %zu '%s' at expression path %s",
                          expression->opcode, index, function->diagnostic_name, expression->path);
    }
    else if (expression != NULL) {
      if (function->source_file != NULL)
        diagnostics_error(diagnostics,
                          "internal compiler error lowering Wasm %s in function %zu at %s:%u:%u, expression path %s",
                          expression->opcode, index, function->source_file, function->source_line,
                          function->source_column, expression->path);
      else
        diagnostics_error(diagnostics,
                          "internal compiler error lowering Wasm %s in function %zu at expression path %s",
                          expression->opcode, index, expression->path);
    }
    else if (function->diagnostic_name != NULL && function->diagnostic_name[0] != '\0') {
      if (function->source_file != NULL)
        diagnostics_error(diagnostics, "internal compiler error lowering function %zu '%s' at %s:%u:%u", index,
                          function->diagnostic_name, function->source_file, function->source_line,
                          function->source_column);
      else
        diagnostics_error(diagnostics, "internal compiler error lowering function %zu '%s'", index,
                          function->diagnostic_name);
    }
    else
      diagnostics_error(diagnostics, "internal compiler error lowering function %zu", index);
  }
  dispose_targets(&context);
  dispose_jump_tables(&context);
  free(context.local_alignments);
  free(context.local_address_bases);
  free(context.local_address_offsets);
  return success;
}

/* Emits startup, shared helpers, and all reachable functions into V32 IR. */
bool lower_module_to_vircon_ir(const ValidatedModule *validated, VirconIrProgram *program, Diagnostics *diagnostics) {
  Context startup = {0};
  uint32_t memory_bytes = validated->module->memory_initial_pages * 65536u;
  size_t index;
  char entry_label[64];
  bool needs_unsigned_division = false, needs_memory_copy = false, needs_memory_fill = false;
  bool memory_can_grow = false;
  startup.program = program;
  startup.diagnostics = diagnostics;
  startup.memory_bytes = memory_bytes;
  for (index = 0; index < validated->module->function_count; ++index) {
    const WasmFunction *function = &validated->module->functions[index];
    if (validated->reachable[index] && !function->is_import &&
        expression_uses_kind(function->body, WASM_EXPR_MEMORY_GROW)) {
      memory_can_grow = true;
      break;
    }
  }
  startup.memory_can_grow = memory_can_grow;
  function_label(validated->module, validated->entry, entry_label, sizeof(entry_label));
  if (!emit_label(&startup, "__wasm_entry") || !initialize_data(validated, &startup) ||
      !emit(&startup, "  mov R1, %u", validated->module->memory_initial_pages) ||
      !emit(&startup, "  mov [%u], R1", VIRCON_WASM_MEMORY_PAGES_WORD) ||
      (validated->module->global_count != 0 &&
       (!emit(&startup, "  mov R1, 0x%08X", validated->module->stack_pointer_initial) ||
        !emit(&startup, "  mov [%u], R1", VIRCON_WASM_STACK_POINTER_WORD))) ||
      !emit(&startup, "  call %s", entry_label) || !emit(&startup, "  hlt") || !emit_label(&startup, "__wasm_trap") ||
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
        !lower_function(validated, function, program, diagnostics, memory_bytes, memory_can_grow))
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
