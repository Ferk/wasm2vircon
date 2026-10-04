(module
  (memory 1)
  (func $loads (param $pointer i32)
    (local $sum i32)
    ;; Negative affine displacements use wrapping i32 source arithmetic. They
    ;; must not be reconstructed as a large positive Wasm memarg span.
    (local.set $sum
      (i32.add
        (i32.load (i32.add (local.get $pointer) (i32.const -8)))
        (i32.load (i32.add (local.get $pointer) (i32.const -4)))))
  )
  (func $entry
    (call $loads (i32.const 64)))
  (export "main" (func $entry))
)
