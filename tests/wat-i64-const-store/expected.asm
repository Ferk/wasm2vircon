# Clang's adjacent i32 initialization becomes two ordered native word stores.
mov R1, 0x55667788
mov [1016384], R1
mov R1, 0x11223344
mov [1016385], R1
