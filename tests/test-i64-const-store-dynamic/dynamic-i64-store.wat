(module
  (memory 1)
  (func $entry (result i32)
    (local $address i32)
    (local.set $address (i32.const 4))
    (i64.store align=4
      (local.get $address)
      (i64.const 0x1122334455667788))
    (i32.const 0))
  (export "__original_main" (func $entry)))
