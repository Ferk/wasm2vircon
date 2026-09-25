# The signed division lowering checks both Wasm trap cases before Vircon IDIV.
ieq R3, 0
jt R3, __wasm_trap
ieq R3, 0x80000000
ieq R3, -1
idiv R1, R2
# i32.rem_s checks a zero divisor and uses Vircon's signed IMOD otherwise.
imod R1, R2
