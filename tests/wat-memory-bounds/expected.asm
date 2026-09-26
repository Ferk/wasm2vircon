mov R1, [999998]
imul R1, 65536
mov R3, 0x00000001
mov R3, 0x00000004
mov R1, 0xFFFFFFFF
mov R1, 0x80000000
ilt R1, 0
jmp __wasm_trap
