# Wasm evaluates arguments before the table selector, each exactly once.
call __wasm_function_4
call __wasm_function_5
xor R1, 0x80000000
