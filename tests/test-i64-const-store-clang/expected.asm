# The static object begins at Wasm byte 65536, hence Vircon word 1016384.
mov R1, 0x55667788
mov [1016384], R1
mov R1, 0x11223344
mov [1016385], R1
