/* Minimal compiler-owned V32 IR storage and ownership operations. */

#include "vircon_ir.h"

#include <stdlib.h>
#include <string.h>

/* Clears a program before its first appended instruction. */
void vircon_ir_init(VirconIrProgram *program) { memset(program, 0, sizeof(*program)); }
/* Releases the instruction strings and backing allocation of a program. */
void vircon_ir_dispose(VirconIrProgram *program) {
  size_t index;
  for (index = 0; index < program->count; ++index)
    free(program->instructions[index].text);
  free(program->instructions);
  memset(program, 0, sizeof(*program));
}
/* Takes ownership of text and appends it to the linear V32 IR. */
bool vircon_ir_append_text(VirconIrProgram *program, char *text, Diagnostics *diagnostics) {
  VirconIrInstruction *items;
  size_t capacity;
  if (program->count == program->capacity) {
    capacity = program->capacity == 0 ? 32 : program->capacity * 2;
    items = realloc(program->instructions, capacity * sizeof(*items));
    if (items == NULL) {
      diagnostics_error(diagnostics, "out of memory while building V32 IR");
      free(text);
      return false;
    }
    program->instructions = items;
    program->capacity = capacity;
  }
  program->instructions[program->count++].text = text;
  return true;
}
