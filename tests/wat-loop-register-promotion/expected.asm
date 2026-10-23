# The checked copy must remain available for guard failures.
__wasm_loop_checked_

# R11 carries the Wasm byte pointer while R13 carries its native word address.
iadd R11, 0x00000008
iadd R13, 2

# R9 carries the loop counter across the fast back-edge.
iadd R9, 0x00000001
mov R1, R9
ilt R1, R10

# The final promoted values are made visible to code after the loop.
mov [BP-1], R11
mov [BP-2], R9
