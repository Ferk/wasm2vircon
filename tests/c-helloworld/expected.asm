# Application loop and existing v0 frame operations.
__wasm_loop_
out GPU_ClearColor, R1
wait

# print_at remains ordinary compiled code and uses low-level GPU imports.
in R0, GPU_SelectedTexture
out GPU_SelectedTexture, R1
out GPU_SelectedRegion, R1
out GPU_DrawingPointX, R1
out GPU_DrawingPointY, R2
out GPU_Command, GPUCommand_DrawRegion

# The application's resultless conditional is lowered as structured br_if.
jt R1, __wasm_block_end_
