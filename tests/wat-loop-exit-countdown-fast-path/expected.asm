# An exit-on-zero countdown gets the same guarded native-word fast path as a
# conditional-backedge loop. The checked copy and original exit remain intact.
__wasm_loop_fast_
__wasm_loop_checked_
__wasm_loop_join_
idiv R4, 8
iadd R13, 2
jt R1, __wasm_trap
