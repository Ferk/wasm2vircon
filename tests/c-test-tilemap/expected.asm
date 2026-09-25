out GPU_MultiplyColor, R1
out INP_SelectedGamepad, R1
in R0, INP_GamepadLeft
in R0, INP_GamepadRight
in R0, INP_GamepadUp
in R0, INP_GamepadDown
in R0, TIM_FrameCounter
imul R1, R2
imod R1, R2
__wasm_i32_div_u:
out GPU_Command, GPUCommand_DrawRegion
