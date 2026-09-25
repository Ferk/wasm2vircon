out GPU_MultiplyColor, R1
out INP_SelectedGamepad, R1
in R0, INP_GamepadLeft
in R0, INP_GamepadRight
in R0, INP_GamepadUp
in R0, INP_GamepadDown
in R0, TIM_FrameCounter
isub R1, R2
imul R1, R2
imod R1, R2
and R2, 31
ine R1, R2
ilt R1, R2
igt R1, R2
ige R1, R2
__wasm_i32_div_u:
