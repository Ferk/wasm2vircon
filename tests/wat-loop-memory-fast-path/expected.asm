# A non-trapping span guard selects between a check-free fast copy and the
# original checked fallback. The fallback's trap branch is intentionally kept.
__wasm_loop_fast_
__wasm_loop_checked_
__wasm_loop_join_
idiv R4, 8
igt R1, 0
# The forward loop retains its stable memory-backed limit, and an aligned
# single-base loop advances the derived Vircon word pointer at the backedge.
mov R10, R3
iadd R13, 2
jt R1, __wasm_trap
