# Both rotate directions survive frontend compilation and lower to 32-bit
# pair operations. The result is checked by the program itself at runtime.
__wasm_entry:
shl R4, R2
shl R4, R6
or R4, R5
