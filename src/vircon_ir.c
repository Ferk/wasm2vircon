#include "vircon_ir.h"

#include <stdlib.h>
#include <string.h>

void vircon_ir_init(VirconIrProgram *program)
{
    memset(program, 0, sizeof(*program));
}

void vircon_ir_dispose(VirconIrProgram *program)
{
    size_t index;

    for (index = 0; index < program->count; index++) {
        free(program->instructions[index].label);
    }
    free(program->instructions);
    memset(program, 0, sizeof(*program));
}

bool vircon_ir_append(VirconIrProgram *program, VirconIrInstruction instruction,
                      Diagnostics *diagnostics)
{
    VirconIrInstruction *new_instructions;
    size_t new_capacity;

    if (program->count == program->capacity) {
        new_capacity = program->capacity == 0 ? 16 : program->capacity * 2;
        new_instructions = realloc(program->instructions,
                                   new_capacity * sizeof(*new_instructions));
        if (new_instructions == NULL) {
            diagnostics_error(diagnostics, "out of memory while building Vircon IR");
            return false;
        }
        program->instructions = new_instructions;
        program->capacity = new_capacity;
    }
    program->instructions[program->count++] = instruction;
    return true;
}
