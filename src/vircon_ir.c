#include "vircon_ir.h"
#include <stdlib.h>
#include <string.h>
void vircon_ir_init(VirconIrProgram *program) { memset(program, 0, sizeof(*program)); }
void vircon_ir_dispose(VirconIrProgram *program) { size_t index; for (index = 0; index < program->count; ++index) free(program->instructions[index].text); free(program->instructions); memset(program, 0, sizeof(*program)); }
bool vircon_ir_append_text(VirconIrProgram *program, char *text, Diagnostics *diagnostics)
{
    VirconIrInstruction *items; size_t capacity;
    if (program->count == program->capacity) { capacity = program->capacity == 0 ? 32 : program->capacity * 2; items = realloc(program->instructions, capacity * sizeof(*items)); if (items == NULL) { diagnostics_error(diagnostics, "out of memory while building V32 IR"); free(text); return false; } program->instructions = items; program->capacity = capacity; }
    program->instructions[program->count++].text = text; return true;
}
