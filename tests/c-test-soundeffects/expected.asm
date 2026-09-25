# Existing region/input/draw/math paths and the narrow SoundEffects SPU delta.
out SPU_SelectedSound, R1
out SPU_SoundPlayWithLoop, R1
out SPU_SoundLoopStart, R1
out SPU_SoundLoopEnd, R1
out SPU_SelectedChannel, R1
out SPU_ChannelVolume, R1
out SPU_GlobalVolume, R1
out SPU_ChannelAssignedSound, R1
out SPU_Command, SPUCommand_PlaySelectedChannel
out SPU_ChannelSpeed, R1
out SPU_ChannelLoopEnabled, R1
out SPU_Command, SPUCommand_PauseSelectedChannel
in R0, SPU_ChannelState
out GPU_Command, GPUCommand_DrawRegion
pow R0, R1
wait
