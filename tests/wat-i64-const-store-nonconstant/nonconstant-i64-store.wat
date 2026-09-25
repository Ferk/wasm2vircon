(module
  (memory 1)
  (func $entry (result i32)
    (i64.store align=4
      (i32.const 4)
      (i64.add (i64.const 1) (i64.const 2)))
    (i32.const 0))
  (export "__original_main" (func $entry)))
