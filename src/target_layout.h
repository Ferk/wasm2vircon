/* Centralized Vircon32 RAM reservations used by the Wasm frontend. */

#ifndef WASM2VIRCON_TARGET_LAYOUT_H
#define WASM2VIRCON_TARGET_LAYOUT_H

#include <stdint.h>

/* Vircon32 has 4 MiWords of RAM. The frontend retains the established policy
 * of leaving the first and last one million words outside linear memory. */
#define VIRCON_RAM_WORDS 4194304u
#define VIRCON_RESERVED_LOW_WORDS 1000000u
#define VIRCON_RESERVED_HIGH_WORDS 1000000u
#define VIRCON_WORD_BYTES 4u
#define VIRCON_WASM_PAGE_BYTES 65536u
#define VIRCON_WORDS_PER_WASM_PAGE (VIRCON_WASM_PAGE_BYTES / VIRCON_WORD_BYTES)
/* Holds a Wasm byte-offset stack pointer; it is outside linear memory and the
 * native stack. */
#define VIRCON_WASM_STACK_POINTER_WORD (VIRCON_RESERVED_LOW_WORDS - 1u)
/* Tracks the current Wasm linear-memory length in 64 KiB pages. It is
 * compiler-owned state, deliberately outside the byte-addressed memory. */
#define VIRCON_WASM_MEMORY_PAGES_WORD (VIRCON_RESERVED_LOW_WORDS - 2u)
#define VIRCON_LINEAR_MEMORY_BASE VIRCON_RESERVED_LOW_WORDS
#define VIRCON_LINEAR_MEMORY_LIMIT (VIRCON_RAM_WORDS - VIRCON_RESERVED_HIGH_WORDS)
#define VIRCON_LINEAR_MEMORY_WORDS (VIRCON_LINEAR_MEMORY_LIMIT - VIRCON_LINEAR_MEMORY_BASE)
#define VIRCON_LINEAR_MEMORY_BYTES ((uint64_t)VIRCON_LINEAR_MEMORY_WORDS * VIRCON_WORD_BYTES)
#define VIRCON_LINEAR_MEMORY_MAX_PAGES (VIRCON_LINEAR_MEMORY_BYTES / VIRCON_WASM_PAGE_BYTES)

#endif
