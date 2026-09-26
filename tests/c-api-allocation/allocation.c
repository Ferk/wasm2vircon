#define VIRCON_IMPLEMENTATION
#include <vircon.h>

void allocation_free_from_other_source(void *memory);

/* Exercises the public page-arena helpers and the Wasm memory instructions
 * they intentionally compile to. The result keeps every operation live. */
int main(void) {
  unsigned before = vircon_memory_size_pages();
  volatile unsigned *first;
  volatile unsigned char *zeroed;
  volatile unsigned *grown;

  /* Configure a byte-addressed global heap before its first allocation. */
  malloc_start_address = (void *)(unsigned long)((before + 1u) << 16);
  malloc_end_address = (void *)(unsigned long)(((before + 4u) << 16) - 1u);
  first = (volatile unsigned *)malloc(12);
  zeroed = (volatile unsigned char *)calloc(4, 3);

  if (first == (void *)0 || zeroed == (void *)0)
    return -1;
  first[0] = 17;
  first[1] = 25;
  grown = (volatile unsigned *)realloc((void *)first, 70000);
  if (grown == (void *)0)
    return -2;
  allocation_free_from_other_source((void *)zeroed);
  zeroed = (volatile unsigned char *)calloc(4, 3);
  if (zeroed == (void *)0)
    return -3;
  free((void *)grown);
  return (int)(vircon_memory_size_pages() - before) + (int)grown[0] + (int)zeroed[0];
}
