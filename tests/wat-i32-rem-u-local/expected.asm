# Save the dividend before __wasm_i32_div_u mutates R1, then restore it to
# calculate dividend - quotient * divisor.
mov [BP-2], R1
call __wasm_i32_div_u
mov R1, [BP-2]
imul R0, R2
isub R1, R0
