(module
  (memory 1)

  ;; A local dividend needs to remain available after the software unsigned
  ;; divider returns its quotient in R0. The final byte must be ASCII '0'.
  (func $entry
    (local $value i32)
    (local.set $value (i32.const 600))
    (i32.store8 (i32.const 0)
      (i32.rem_u (local.get $value) (i32.const 10)))
  )

  (export "main" (func $entry))
)
