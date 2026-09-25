# Leaf functions reserve no unused outgoing area; entry and caller reserve six words.
isub SP, 48
isub SP, 54
isub SP, 55
mov [SP+5], R1
mov R1, [BP+7]
call __wasm_function_0
call __wasm_function_1
call __wasm_function_2
