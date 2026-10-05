# The normalized Clang module uses preserving halfword stores and loads.
shl R7, -8
shl R5, 8
or R1, R5
; Wasm memory/unreachable trap
