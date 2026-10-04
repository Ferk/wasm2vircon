(module
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $sum i32)
    (local.set $pointer (i32.const 32))

    ;; Both loads are guaranteed to run before local.set completes, so the
    ;; first access may prove their complete 16-byte span.
    (local.set $sum
      (i32.add
        (i32.load offset=0 (local.get $pointer))
        (i32.load offset=12 (local.get $pointer))))
  )
  (export "main" (func $entry))
)
