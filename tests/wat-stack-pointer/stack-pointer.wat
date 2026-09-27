(module
  (global $__stack_pointer (mut i32) (i32.const 65536))
  (memory 1)
  (func $entry (result i32)
    (global.set $__stack_pointer
      (i32.sub
        (global.get $__stack_pointer)
        (i32.const 4)))
    (global.get $__stack_pointer))
  (export "entry" (func $entry)))
