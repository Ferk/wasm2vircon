# The invariant pointer word is loaded once before the fast loop. Its runtime
# value plus the initial offset forms the checked span and native word pointer.
mov R12, [1000025]
__wasm_loop_fast_
__wasm_loop_checked_
__wasm_loop_join_
and R1, 3
idiv R4, 16
iadd R1, R12
mov R1, [R13]
mov R1, [R13+1]
iadd R13, 4
out GPU_Command, GPUCommand_DrawRegion
