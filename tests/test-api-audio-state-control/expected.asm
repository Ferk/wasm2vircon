# SPU control and readback must use their direct hardware ports and commands.
in R0, SPU_SelectedSound
in R0, SPU_SelectedChannel
out SPU_ChannelPosition, R1
in R0, SPU_ChannelPosition
in R0, SPU_ChannelSpeed
in R0, SPU_GlobalVolume
out SPU_Command, SPUCommand_StopSelectedChannel
out SPU_Command, SPUCommand_PauseAllChannels
out SPU_Command, SPUCommand_StopAllChannels
out SPU_Command, SPUCommand_ResumeAllChannels
