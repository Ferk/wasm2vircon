# A two-byte access accepts only pointers through the final two valid bytes.
igt R1, 0x8000FFFE

# store16 writes two individual preserving lanes, shifting the second byte.
shl R7, -8
mov [R4], R5

# load16 reconstructs the little-endian value from two bytes.
shl R5, 8
or R1, R5

# Signed byte and halfword loads extend their respective sign bits.
and R2, 0x00008000
or R1, 0xFFFF0000
and R2, 0x00000080
or R1, 0xFFFFFF00
