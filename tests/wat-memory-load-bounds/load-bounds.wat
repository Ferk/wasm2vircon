(module
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $sum i32)
    (local.set $pointer (i32.const 32))

    ;; Each load must trap at its own evaluation point. The first check must
    ;; not be widened to include the later load merely because both occur in
    ;; one expression.
    (local.set $sum
      (i32.add
        (i32.load offset=0 (local.get $pointer))
        (i32.load offset=12 (local.get $pointer))))
  )
  (export "main" (func $entry))
)
