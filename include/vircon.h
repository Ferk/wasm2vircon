#ifndef VIRCON_RUNTIME_H
#define VIRCON_RUNTIME_H

/* Application-facing runtime API. print_at is library code, not a Wasm import.
 * Its text argument is a NUL-terminated CP-1252 byte string, not UTF-8. */
void clear_screen(int color);
void print_at(int drawing_x, int drawing_y, const char *text);
void end_frame(void);

#endif
