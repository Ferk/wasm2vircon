call __wasm_memory_copy
call __wasm_memory_fill
__wasm_memory_copy_backward:
__wasm_memory_fill:
jt R1, __wasm_trap
