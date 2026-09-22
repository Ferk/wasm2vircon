# Direct operations.
xor R1, R2
ile R1, R2

# Signed-shift count masking, sign test, and sign-bit fill.
and R2, 31
ilt R3, 0
mov R5, 0xFFFFFFFF
or R1, R5

# The high unsigned conversion path uses a sticky low bit, CIF, and an exact
# power-of-two scale through FADD.
cif R3
fadd R3, R3
