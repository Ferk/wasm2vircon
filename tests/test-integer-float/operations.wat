(module
  (memory 1)
  (func $main (result i32)
    ;; Generic XOR and signed less-than-or-equal comparison.
    (drop (i32.xor (i32.const 0x55aa55aa) (i32.const 0xff00ff00)))
    (drop (i32.le_s (i32.const -12) (i32.const -12)))

    ;; Test arithmetic shifting of a negative word, including a count that
    ;; masks to zero under Wasm's i32 shift-count semantics.
    (drop (i32.shr_s (i32.const -1) (i32.const 1)))
    (drop (i32.shr_s (i32.const 0x80000000) (i32.const 32)))

    ;; This lies immediately above the first high-half f32 rounding boundary.
    (drop (f32.convert_i32_u (i32.const 0x80000081)))
    (i32.const 0))
  (export "__original_main" (func $main)))
