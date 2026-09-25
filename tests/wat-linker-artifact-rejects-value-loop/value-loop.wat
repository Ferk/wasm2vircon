(module
  (memory 1)
  (func $main (result i32)
    (loop $not_a_frame_loop (result i32)
      (br_if $not_a_frame_loop (i32.const 0))
      (i32.const 1)))
  (export "main" (func $main)))
