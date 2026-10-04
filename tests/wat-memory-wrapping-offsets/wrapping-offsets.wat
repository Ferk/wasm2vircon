(module
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $sum i32)
    (local.set $pointer (i32.const 64))

    ;; Negative affine displacements use wrapping i32 source arithmetic. They
    ;; must not be reconstructed as a large positive Wasm memarg span.
    (local.set $sum
      (i32.add
        (i32.load (i32.add (local.get $pointer) (i32.const -8)))
        (i32.load (i32.add (local.get $pointer) (i32.const -4)))))
  )
  (export "main" (func $entry))
)
