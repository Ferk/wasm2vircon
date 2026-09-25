(module
  (type $drawing-point (func (param i32 i32)))
  (import "env" "vircon_gpu_set_drawing_point" (func $drawing-point (type $drawing-point)))
  (memory 17)
  (func $entry (result i32)
    (local $pair i64)
    ;; Source pair: { 1, 2 }.
    (i32.store offset=1048576 (i32.const 0) (i32.const 1))
    (i32.store offset=1048580 (i32.const 0) (i32.const 2))
    ;; This is the normalized Zig local.tee aggregate-copy shape.
    (i64.store offset=1048584
      (i32.const 0)
      (local.tee $pair
        (i64.load offset=1048576 (i32.const 0))))
    (call $drawing-point
      (i32.wrap_i64 (local.get $pair))
      (i32.wrap_i64 (i64.shr_u (local.get $pair) (i64.const 32))))
    (i32.load offset=1048584 (i32.const 0)))
  (export "__original_main" (func $entry)))
