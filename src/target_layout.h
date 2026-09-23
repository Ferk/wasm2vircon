/* Centralized Vircon32 RAM reservations used by the Wasm frontend. */

#ifndef WASM2VIRCON_TARGET_LAYOUT_H
#define WASM2VIRCON_TARGET_LAYOUT_H

#include <stdint.h>

/* The investigated Vircon32 allocator leaves the first and last one million
 * words alone. Keep the Wasm frontend in that middle region. */
#define VIRCON_RAM_WORDS 4000000u
#define VIRCON_RESERVED_LOW_WORDS 1000000u
#define VIRCON_RESERVED_HIGH_WORDS 1000000u
/* Holds a Wasm byte-offset stack pointer; it is outside linear memory and the
 * native stack. */
#define VIRCON_WASM_STACK_POINTER_WORD (VIRCON_RESERVED_LOW_WORDS - 1u)
#define VIRCON_LINEAR_MEMORY_BASE VIRCON_RESERVED_LOW_WORDS
#define VIRCON_LINEAR_MEMORY_LIMIT                                             \
  (VIRCON_RAM_WORDS - VIRCON_RESERVED_HIGH_WORDS)
#define VIRCON_LINEAR_MEMORY_WORDS                                             \
  (VIRCON_LINEAR_MEMORY_LIMIT - VIRCON_LINEAR_MEMORY_BASE)
#define VIRCON_LINEAR_MEMORY_BYTES ((uint64_t)VIRCON_LINEAR_MEMORY_WORDS * 4u)
#define VIRCON_LINEAR_MEMORY_MAX_PAGES (VIRCON_LINEAR_MEMORY_BYTES / 65536u)

#endif
