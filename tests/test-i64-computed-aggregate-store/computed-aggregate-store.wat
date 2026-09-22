;; Restricted Zig-style computed Pair construction: high and low i32 words are
;; packed only as the immediate value of i64.store, then observed separately.
(module
  (type $drawing-point (func (param i32 i32)))
  (import "env" "vircon_gpu_set_drawing_point"
    (func $drawing-point (type $drawing-point)))
  (memory 17)
  ;; Source Pair { 40, 80 }, initialized through normal active data.
  (data (i32.const 1048576) "\28\00\00\00\50\00\00\00")
  (func $entry (result i32)
    (local $pair i64)
    (local $high i32)
    (local $low i32)
    ;; Pair { low + 1, high + 2 }, using Zig's exact packing and the initial
    ;; i64.load/local.tee high-word extraction shape.
    (i64.store offset=1048576
      (i32.const 0)
      (i64.or
        (i64.shl
          (i64.extend_i32_u
            (local.tee $high
              (i32.add
                (i32.wrap_i64
                  (i64.shr_u
                    (local.tee $pair
                      (i64.load offset=1048576 (i32.const 0)))
                    (i64.const 32)))
                (i32.const 2))))
          (i64.const 32))
        (i64.extend_i32_u
          (local.tee $low
            (i32.add
              (i32.wrap_i64 (local.get $pair))
              (i32.const 1))))))
    (call $drawing-point (local.get $low) (local.get $high))
    (i32.load offset=1048576 (i32.const 0)))
  (export "__original_main" (func $entry)))
