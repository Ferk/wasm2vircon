# A selector reaches a ROM-resident address table through a register jump.
__wasm_br_table_0_3:
pointer __wasm_block_end_0_2
pointer __wasm_loop_0_1
pointer __wasm_block_end_0_0
mov R4, [R3]
jmp R4

# The explicit table cases and default resolve through the active scopes.
jf R1, __wasm_block_end_0_0
