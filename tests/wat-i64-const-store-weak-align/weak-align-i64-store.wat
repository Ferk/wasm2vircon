(module
  (memory 1)
  ;; The address is aligned, but the Wasm alignment declaration is too weak
  ;; for the deliberately strict two-word lowering.
  (func $entry (result i32)
    (i64.store align=1
      (i32.const 4)
      (i64.const 0x1122334455667788))
    (i32.const 0))
  (export "__original_main" (func $entry)))
