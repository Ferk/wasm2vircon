(module
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $discard i32)
    (local.set $pointer (i32.const 32))

    ;; Once both ends have passed their normal checks, the middle word is
    ;; necessarily in bounds as well. The target address is also still cached.
    (local.set $discard (i32.load offset=0 (local.get $pointer)))
    (local.set $discard (i32.load offset=8 (local.get $pointer)))
    (local.set $discard (i32.load offset=4 (local.get $pointer)))
  )
  (export "main" (func $entry))
)
