;; The expected low-64 product is 0x2236D88FE5618CF0. Both words are made
;; observable through the normal i32 platform ABI before the low word returns.
(module
  (type $point (func (param i32 i32)))
  (type $entry (func (result i32)))
  (import "env" "vircon_gpu_set_drawing_point" (func $point (type $point)))
  (memory 17)
  (export "memory" (memory 0))
  (func $main (type $entry)
    (local $product i64)
    (local.set $product
      (i64.mul (i64.const 0x123456789ABCDEF0) (i64.const 0x0FEDCBA987654321)))
    (call $point
      (i32.wrap_i64 (local.get $product))
      (i32.wrap_i64 (i64.shr_u (local.get $product) (i64.const 32))))
    (i64.eq (local.get $product) (i64.const 0x2236D88FE5618CF0)))
  (export "main" (func $main)))
