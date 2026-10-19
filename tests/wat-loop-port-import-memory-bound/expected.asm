# The non-trapping version guard loads the bound once into R10 before either
# loop copy. Both the fast and checked backedges must retain that exact value
# across the two audited GPU port operations.
mov R10, R3
__wasm_loop_fast_
out GPU_DrawingPointX, R8
out GPU_Command, GPUCommand_DrawRegion
ilt R1, R10
__wasm_loop_checked_
out GPU_DrawingPointX, R8
out GPU_Command, GPUCommand_DrawRegion
ilt R1, R10
