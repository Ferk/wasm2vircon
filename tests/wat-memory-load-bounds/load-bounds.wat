(module
  (memory 1)
  (func $loads (param $pointer i32)
    (local $base i32)
    (local $sum i32)
    (local.set $base (i32.mul (local.get $pointer) (i32.const 4)))
    ;; Each load must trap at its own evaluation point. The first check must
    ;; not be widened to include the later load merely because both occur in
    ;; one expression.
    (local.set $sum
      (i32.add
        (i32.load offset=0 (local.get $base))
        (i32.load offset=12 (local.get $base))))
  )
  (func $entry
    (call $loads (i32.const 8)))
  (export "main" (func $entry))
)
