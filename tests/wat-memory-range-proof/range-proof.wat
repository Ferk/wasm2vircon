(module
  (memory 1)
  (func $loads (param $pointer i32)
    (local $base i32)
    (local $discard i32)
    (local.set $base (i32.mul (local.get $pointer) (i32.const 4)))
    ;; Once both ends have passed their normal checks, the middle word is
    ;; necessarily in bounds as well. The target address is also still cached.
    (local.set $discard (i32.load offset=0 (local.get $base)))
    (local.set $discard (i32.load offset=8 (local.get $base)))
    (local.set $discard (i32.load offset=4 (local.get $base)))
  )
  (func $entry
    (call $loads (i32.const 8)))
  (export "main" (func $entry))
)
