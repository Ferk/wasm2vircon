# Startup initializes the ordinary linear-memory word before the entry call.
mov R1, 0x12345678
mov [1000000], R1
call __wasm_function_2

# Every raw read uses the same explicit physical-RAM bounds and direct word load.
ilt R2, 0
igt R2, 0x003FFFFF
mov R0, [R1]
mov R0, 0

# Cover the mapped region and both valid/invalid physical boundaries.
mov R1, 0x000F4240
mov R1, 0x003FFFFF
mov R1, 0xFFFFFFFF
mov R1, 0x00400000
