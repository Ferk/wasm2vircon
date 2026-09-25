(module
  (global $state (mut i32) (i32.const 7))
  (memory 1)
  (export "state" (global $state))
  (func $main (result i32)
    (i32.const 0))
  (export "main" (func $main)))
