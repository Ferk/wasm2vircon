# Mixed parameters occupy eight target argument words.
mov [SP+7], R1
call __wasm_function_
# The high word of an i64 return travels in R1.
mov R1, [BP+
