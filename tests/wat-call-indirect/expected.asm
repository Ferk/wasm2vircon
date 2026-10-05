# Unsigned range checking occurs before typed direct-call dispatch.
xor R1, 0x80000000
ilt R1, 0x80000005
jf R1, __wasm_trap

# Only signature-compatible slots become callable cases.
call __wasm_function_0
call __wasm_function_2
call __wasm_function_3

# Five-argument indirect calls use the ordinary outgoing argument area.
mov [SP+4], R1

# Null, mismatched, and unmatched selectors reach the shared trap.
jmp __wasm_trap
