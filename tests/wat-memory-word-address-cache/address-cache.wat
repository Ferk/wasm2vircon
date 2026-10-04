(module
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $discard i32)
    (local $float f32)
    (local.set $pointer (i32.const 16))

    ;; These aligned accesses share one byte-pointer base. The target backend
    ;; may derive its Vircon word address once, then use word displacements.
    (local.set $discard (i32.load (local.get $pointer)))
    (i32.store offset=4 (local.get $pointer) (i32.const 7))
    (local.set $discard (i32.load offset=8 (local.get $pointer)))

    ;; Compiler-private branches in a scalar conversion do not modify the
    ;; cached address, so a following neighboring access can still reuse it.
    (local.set $float (f32.load offset=12 (local.get $pointer)))
    (local.set $discard (i32.trunc_sat_f32_s (local.get $float)))
    (local.set $float (f32.load offset=16 (local.get $pointer)))
  )
  (export "main" (func $entry))
)
