# Time/date reads, region setup/drawing, alternating channel playback, and frame wait.
in R0, TIM_CurrentTime
in R0, TIM_CurrentDate
in R0, TIM_FrameCounter
out GPU_DrawingScaleX, R1
out GPU_DrawingScaleY, R2
out GPU_Command, GPUCommand_DrawRegionZoomed
out GPU_Command, GPUCommand_DrawRegion
out SPU_ChannelAssignedSound, R1
out SPU_ChannelVolume, R1
out SPU_ChannelSpeed, R1
out SPU_Command, SPUCommand_PlaySelectedChannel
wait
