mov R1, 0x44332211
mov [1000000], R1
; Wasm memory/unreachable trap
imul R3, -8
mov R3, -8
__wasm_load_aligned_
__wasm_store_aligned_
__wasm_if_end_
