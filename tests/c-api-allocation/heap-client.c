#include <vircon.h>

/* Confirms ordinary includes in other sources use the one heap emitted by the
 * VIRCON_IMPLEMENTATION source, rather than a private per-source allocator. */
void allocation_free_from_other_source(void *memory) { free(memory); }
