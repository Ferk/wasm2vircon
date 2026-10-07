# Startup records the initial Wasm page count outside linear memory.
mov [999998], R1
# memory.size reads that dynamic state; memory.grow applies the declared cap.
__wasm_memory_grow_clear
__wasm_memory_grow_failed
# Vircon comparisons overwrite their first operand, so preserve the requested
# page count while checking it against the physical maximum.
mov R3, R2
igt R3, R1
mov R1, 0x00000002
mov [999998], R5
# The grown page is addressed through normal packed Wasm memory lowering.
iadd R3, 1000000
