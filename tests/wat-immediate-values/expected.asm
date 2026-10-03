# Integer arithmetic uses a target immediate rather than a temporary slot.
iadd R1, 0x00000007

# A scalar f32 constant may flow directly into the return register as raw bits.
mov R0, 0x3FC00000

# Constant import and defined-call arguments avoid compiler temporary slots.
mov R1, 0x00123456
out GPU_ClearColor, R1
mov R1, 0x00000005
mov [SP+0], R1
