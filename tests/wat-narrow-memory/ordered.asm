# Wasm store operands are evaluated before the preserving two-byte write.
mov R1, [BP+3]
mov R7, R1
shl R7, -8

# The first reconstructed load is unsigned; signed extension occurs later.
shl R5, 8
or R1, R5
shl R5, 8
or R1, R5
and R1, 0x0000FFFF
