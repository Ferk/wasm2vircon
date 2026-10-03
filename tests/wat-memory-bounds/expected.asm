mov R1, [999998]
imul R1, 65536
mov R3, 0x00000001
mov R3, 0x00000004
mov R2, 0xFFFFFFFF
mov R2, 0x80000000
ilt R1, 0
jmp __wasm_trap
