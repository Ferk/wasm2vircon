#include <vircon.h>

/* Exercises the public page-arena helpers and the Wasm memory instructions
 * they intentionally compile to. The result keeps every operation live. */
int main(void) {
  unsigned before = vircon_memory_size_pages();
  volatile unsigned *first = (volatile unsigned *)malloc(12);
  volatile unsigned char *zeroed = (volatile unsigned char *)calloc(4, 3);
  volatile unsigned *grown;

  if (first == (void *)0 || zeroed == (void *)0)
    return -1;
  first[0] = 17;
  first[1] = 25;
  grown = (volatile unsigned *)realloc((void *)first, 70000);
  if (grown == (void *)0)
    return -2;
  free((void *)zeroed);
  zeroed = (volatile unsigned char *)calloc(4, 3);
  if (zeroed == (void *)0)
    return -3;
  free((void *)grown);
  return (int)(vircon_memory_size_pages() - before) + (int)grown[0] + (int)zeroed[0];
}
