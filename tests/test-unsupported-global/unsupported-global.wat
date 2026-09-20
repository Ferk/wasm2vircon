(module
  (global $state (mut i32) (i32.const 7))
  (func $entry (result i32)
    global.get $state)
  (export "entry" (func $entry)))
