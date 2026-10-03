# The software divider must reduce the arithmetic sign extension to one bit.
__wasm_i32_div_u:
shl R6, R7
and R6, 1

# Both unsigned quotient and remainder paths use the shared helper.
call __wasm_i32_div_u
imul R0, R2
