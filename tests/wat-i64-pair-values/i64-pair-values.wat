;; General pair-valued i64 regression: local storage, arithmetic, shift,
;; equality, load/store, extension, and i32 extraction all remain ordinary
;; Wasm expressions rather than one of the former aggregate-only shapes.
(module
  (type $point (func (param i32 i32)))
  (type $entry (func (result i32)))
  (import "env" "vircon_gpu_set_drawing_point" (func $point (type $point)))
  (memory 17)
  (export "memory" (memory 0))
  (func $main (type $entry)
    (local $value i64)
    (local $other i64)
    (local $shifted i64)
    (local.set $value (i64.const 0x1122334455667788))
    (local.set $other
      (i64.xor
        (i64.add (local.get $value) (i64.extend_i32_u (i32.const 2)))
        (i64.const 0x00FF00FF00FF00FF)))
    (local.set $shifted
      (i64.shr_u
        (i64.shl (local.get $other) (i64.const 4))
        (i64.const 4)))
    (drop (i64.shr_s (i64.const 0x8000000000000000) (i64.const 4)))
    (drop (i64.eq (local.get $shifted) (local.get $other)))
    (drop (i64.ne (local.get $shifted) (i64.const 0)))
    (drop (select (local.get $shifted) (local.get $other) (i32.const 0)))
    (i64.store offset=64 (i32.const 0) (local.get $shifted))
    (call $point
      (i32.wrap_i64 (i64.load offset=64 (i32.const 0)))
      (i32.wrap_i64
        (i64.shr_u (i64.load offset=64 (i32.const 0)) (i64.const 32))))
    (drop (i64.eqz (local.get $shifted)))
    (i32.wrap_i64 (i64.sub (local.get $shifted) (i64.const 1))))
  (export "main" (func $main)))
