# Existing input, f32 transform, BIOS line primitive, byte-text and rotozoom paths.
out GPU_DrawingScaleX, R1
out GPU_DrawingScaleY, R2
out GPU_DrawingAngle, R1
out GPU_Command, GPUCommand_DrawRegionZoomed
out GPU_Command, GPUCommand_DrawRegionRotozoomed
out INP_SelectedGamepad, R1
in R0, INP_GamepadLeft
in R0, INP_GamepadButtonA
