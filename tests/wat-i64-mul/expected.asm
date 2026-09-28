# i64.mul lowers through 16-bit partial products and remains observable as
# ordinary low/high i32 words through the platform ABI.
imul R1, R2
out GPU_DrawingPointX, R1
out GPU_DrawingPointY, R2
