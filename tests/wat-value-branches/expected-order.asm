# A taken value branch stores 7 and jumps past the ordinary fallthrough copy.
__wasm_function_0:
mov R1, 0x00000007
mov [BP-1], R1
jmp __wasm_block_end_
mov R1, 0x00000009
mov [BP-1], R1
__wasm_block_end_
