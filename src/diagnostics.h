/* Shared error-reporting interface for every compiler stage. */

#ifndef WASM2VIRCON_DIAGNOSTICS_H
#define WASM2VIRCON_DIAGNOSTICS_H

#include <stdarg.h>
#include <stdio.h>

/* Holds the error stream and count for one compiler invocation. */
typedef struct Diagnostics {
  FILE *stream;
  unsigned errors;
} Diagnostics;

/* Initializes a diagnostic sink that writes to stream. */
void diagnostics_init(Diagnostics *diagnostics, FILE *stream);
/* Emits one printf-style compiler error and increments the error count. */
void diagnostics_error(Diagnostics *diagnostics, const char *format, ...) __attribute__((format(printf, 2, 3)));
/* Emits supplementary diagnostic guidance without changing the error count. */
void diagnostics_note(Diagnostics *diagnostics, const char *format, ...) __attribute__((format(printf, 2, 3)));

#endif
