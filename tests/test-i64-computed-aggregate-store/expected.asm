# The computed i32 words remain normal locals and are observable through the
# platform ABI after the exact i64 packing-to-store frontend legalization.
mov [BP-3], R1
mov [BP-4], R1
out GPU_DrawingPointX, R1
out GPU_DrawingPointY, R2
