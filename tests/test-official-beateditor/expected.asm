# Existing GPU/input/audio operations, including the byte-cell matrix path.
__wasm_loop_
in R0, INP_GamepadButtonA
in R0, INP_GamepadLeft
in R0, INP_GamepadRight
in R0, INP_GamepadUp
in R0, INP_GamepadDown
out GPU_SelectedTexture, R1
out GPU_RegionMinX, R1
out GPU_RegionHotSpotY, R2
out GPU_Command, GPUCommand_DrawRegion
in R0, SPU_ChannelState
out SPU_SelectedChannel, R1
out SPU_ChannelAssignedSound, R1
out SPU_Command, SPUCommand_PlaySelectedChannel
wait
