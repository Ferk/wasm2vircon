/* Diagnostics implementation: writes consistent compiler errors to one stream.
 */

#include "diagnostics.h"

#include <stdarg.h>

/* Initializes an error sink before any compiler stage reports failures. */
void diagnostics_init(Diagnostics *diagnostics, FILE *stream) {
  diagnostics->stream = stream;
  diagnostics->errors = 0;
}

/* Prefixes and records one formatted compiler error. */
void diagnostics_error(Diagnostics *diagnostics, const char *format, ...) {
  va_list arguments;

  diagnostics->errors++;
  fprintf(diagnostics->stream, "wasm2vircon: error: ");
  va_start(arguments, format);
  vfprintf(diagnostics->stream, format, arguments);
  va_end(arguments);
  fputc('\n', diagnostics->stream);
}

/* Prints contextual guidance associated with a preceding compiler error. */
void diagnostics_note(Diagnostics *diagnostics, const char *format, ...) {
  va_list arguments;

  fprintf(diagnostics->stream, "wasm2vircon: note: ");
  va_start(arguments, format);
  vfprintf(diagnostics->stream, format, arguments);
  va_end(arguments);
  fputc('\n', diagnostics->stream);
}
