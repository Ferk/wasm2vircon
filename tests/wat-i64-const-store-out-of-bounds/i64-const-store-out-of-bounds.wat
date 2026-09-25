(module
  (memory 1)
  (func $entry (result i32)
    ;; The final aligned four-byte word cannot hold an eight-byte i64 store.
    (i64.store offset=65532 align=4
      (i32.const 0)
      (i64.const 0))
    (i32.const 0))
  (export "__original_main" (func $entry)))
