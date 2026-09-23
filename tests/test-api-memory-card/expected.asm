# The public card helpers use only the existing word-addressed device bridge.
in R0, MEM_Connected
iadd R1, 0x30000000
mov R0, [R1]
mov [R1], R2
