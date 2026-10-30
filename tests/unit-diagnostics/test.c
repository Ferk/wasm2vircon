/* Check diagnostic formatting and error counting using a temporary stream. */
#include "diagnostics.h"

#include <string.h>

/* Return a nonzero status if messages or the error count are incorrect. */
int main(void) {
  char output[256];
  Diagnostics diagnostics;
  FILE *stream = tmpfile();
  size_t length;
  if (stream == NULL)
    return 1;
  diagnostics_init(&diagnostics, stream);
  diagnostics_error(&diagnostics, "bad operand %zu", (size_t)42);
  diagnostics_note(&diagnostics, "check %s", "input");
  if (diagnostics.errors != 1 || fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return 1;
  }
  length = fread(output, 1, sizeof(output) - 1, stream);
  output[length] = '\0';
  fclose(stream);
  return strcmp(output, "wasm2vircon: error: bad operand 42\nwasm2vircon: note: check input\n") != 0;
}
