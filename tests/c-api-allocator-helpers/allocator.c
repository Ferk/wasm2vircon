/* Exercise the official block helpers against the real byte-addressed heap. */
#define VIRCON_IMPLEMENTATION
#include <vircon.h>

int main(void) {
  void *p = malloc(128);
  malloc_block *block;
  if (!p) return 1;
  block = (malloc_block *)p - 1;
  reduce_malloc_block(block, 32);
  if (block->size != 32 || !block->next || !block->next->free) return 2;
  if (!expand_malloc_block(block, 64) || block->size < 64) return 3;
  if (expand_malloc_block(block, 10000)) return 4;
  block->free = true;
  merge_free_malloc_blocks(block);
  if (block->next || block->size < 128) return 5;
  return 0;
}
