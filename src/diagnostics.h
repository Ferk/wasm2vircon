#ifndef WASM2VIRCON_DIAGNOSTICS_H
#define WASM2VIRCON_DIAGNOSTICS_H

#include <stdarg.h>
#include <stdio.h>

typedef struct Diagnostics {
    FILE *stream;
    unsigned errors;
} Diagnostics;

void diagnostics_init(Diagnostics *diagnostics, FILE *stream);
void diagnostics_error(Diagnostics *diagnostics, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif
