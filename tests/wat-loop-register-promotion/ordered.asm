mov R9, R1
mov R11, R2
__wasm_loop_fast_
iadd R11, 0x00000008
iadd R13, 2
iadd R9, 0x00000001
mov R1, R9
ilt R1, R10
jt R1, __wasm_loop_fast_
mov [BP-1], R11
mov [BP-2], R9
jmp __wasm_loop_join_
__wasm_loop_checked_
__wasm_loop_join_
