/* Shared error-reporting interface for every compiler stage. */

#ifndef WASM2VIRCON_DIAGNOSTICS_H
#define WASM2VIRCON_DIAGNOSTICS_H

#include <stdarg.h>
#include <stdio.h>

/* MinGW's default printf attribute checks Microsoft formats, not the C99
 * formats used by the project's ANSI stdio configuration. */
#if defined(__MINGW32__) && defined(__GNUC__) && !defined(__clang__)
#define WASM2VIRCON_PRINTF_FORMAT __attribute__((format(gnu_printf, 2, 3)))
#elif defined(__GNUC__) || defined(__clang__)
#define WASM2VIRCON_PRINTF_FORMAT __attribute__((format(printf, 2, 3)))
#else
#define WASM2VIRCON_PRINTF_FORMAT
#endif

/* Holds the error stream and count for one compiler invocation. */
typedef struct Diagnostics {
  FILE *stream;
  unsigned errors;
} Diagnostics;

/* Initializes a diagnostic sink that writes to stream. */
void diagnostics_init(Diagnostics *diagnostics, FILE *stream);
/* Emits one printf-style compiler error and increments the error count. */
void diagnostics_error(Diagnostics *diagnostics, const char *format, ...) WASM2VIRCON_PRINTF_FORMAT;
/* Emits supplementary diagnostic guidance without changing the error count. */
void diagnostics_note(Diagnostics *diagnostics, const char *format, ...) WASM2VIRCON_PRINTF_FORMAT;

#endif
