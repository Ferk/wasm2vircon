(module
  (memory 1)
  ;; The eight bytes begin at an unaligned Wasm byte address.
  (data (i32.const 1) "\11\22\33\44\55\66\77\88")
  (func $entry (result i32)
    (block (result i32)
      ;; This is the exact high-word extraction shape emitted by the probe.
      (drop
        (i32.wrap_i64
          (i64.shr_u
            (i64.load offset=1 align=1 (i32.const 0))
            (i64.const 32))))
      ;; Also exercise a non-word-aligned constant right shift.
      (i32.wrap_i64
        (i64.shr_u
          (i64.load offset=1 align=1 (i32.const 0))
          (i64.const 4)))))
  (export "__original_main" (func $entry)))
