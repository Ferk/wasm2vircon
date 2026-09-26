# Clang's builtins must reach the real dynamic Wasm page mechanism.
__wasm_memory_grow_clear
__wasm_memory_grow_failed
mov R1, [999998]
# Allocator payloads and their size headers use ordinary Wasm byte memory.
mov [R4], R5
