# Narrow memory-card boundary plus ordinary GPU/input/runtime code.
in R0, MEM_Connected
iadd R1, 0x30000000
mov R0, [R1]
mov [R1], R2
out GPU_SelectedTexture, R1
out GPU_Command, GPUCommand_DrawRegion
in R0, INP_GamepadButtonA
in R0, INP_GamepadButtonB
wait
