(module
  (memory 1)
  (func $add_one (param $value i32) (result i32)
    (i32.add (local.get $value) (i32.const 1)))
  (func $entry (result i32)
    (call $add_one (i32.const 41)))
  (export "__original_main" (func $entry))
)
