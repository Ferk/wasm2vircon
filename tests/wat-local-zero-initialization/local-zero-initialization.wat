(module
  (memory 1)
  ;; Returns the values observed before poisoning every local. Calling this
  ;; twice proves initialization belongs to each invocation, not cartridge
  ;; startup. The nonzero parameter also proves parameters are not cleared.
  (func $observe (param $poison i32) (result i32)
    (local $integer i32)
    (local $real f32)
    (local $wide i64)
    (local $observed i32)
    (local.set $observed
      (i32.or
        (i32.or
          (local.get $integer)
          (i32.reinterpret_f32 (local.get $real)))
        (i32.or
          (i32.eqz (i64.eqz (local.get $wide)))
          (i32.eqz (local.get $poison)))))
    (local.set $integer (local.get $poison))
    (local.set $real (f32.reinterpret_i32 (local.get $poison)))
    (local.set $wide (i64.extend_i32_u (local.get $poison)))
    (local.get $observed)
  )

  ;; Also exercises the direct-store path used for very small local frames.
  (func $small (result i32)
    (local $value i32)
    (local.get $value)
  )

  (func $entry (result i32)
    (i32.or
      (i32.or
        (call $observe (i32.const 305419896))
        (call $observe (i32.const 1985229328)))
      (call $small))
  )
  (export "main" (func $entry))
)
