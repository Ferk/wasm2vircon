feq R2, R1
jf R2, __wasm_trap
flt R2, 0xCF000000
jt R2, __wasm_trap
fge R2, 0x4F000000
jt R2, __wasm_trap
cfi R1
