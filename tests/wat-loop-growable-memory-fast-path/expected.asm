# A growable module reads the live page count once in the loop guard. Dynamic
# pointer alignment is checked once before R13 carries the native word address.
__wasm_loop_fast_
__wasm_loop_checked_
__wasm_loop_join_
mov R4, [999998]
imul R4, 65536
and R1, 3
idiv R4, 16
mov R1, [R13]
mov R1, [R13+2]
mov [R13], R1
mov R1, [R13+1]
mov R1, [R13+3]
mov [R13+1], R1
iadd R13, 4
