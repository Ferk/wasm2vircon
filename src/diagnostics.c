#include "diagnostics.h"

#include <stdarg.h>

void diagnostics_init(Diagnostics *diagnostics, FILE *stream)
{
    diagnostics->stream = stream;
    diagnostics->errors = 0;
}

void diagnostics_error(Diagnostics *diagnostics, const char *format, ...)
{
    va_list arguments;

    diagnostics->errors++;
    fprintf(diagnostics->stream, "wasm2vircon: error: ");
    va_start(arguments, format);
    vfprintf(diagnostics->stream, format, arguments);
    va_end(arguments);
    fputc('\n', diagnostics->stream);
}
