(module
  (memory 1)
  (func $entry (result i32)
    (i64.store align=4
      (i32.const 1)
      (i64.const 0x1122334455667788))
    (i32.const 0))
  (export "__original_main" (func $entry)))
