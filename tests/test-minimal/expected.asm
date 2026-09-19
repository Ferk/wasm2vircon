# Literal assembly fragments that characterize this test's supported lowering.
mov R1, 0xFFFF40FF
__wasm_loop_
out GPU_ClearColor, R1
out GPU_Command, GPUCommand_ClearScreen
wait
jmp __wasm_loop_
