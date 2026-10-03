# A value-returning platform import can feed nested ordinary arithmetic.
iabs R0
imul R1, 0x00000064
idiv R1, R2
iadd R1, 0x00000064
