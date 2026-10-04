(module
  (memory 1)
  (func $access (param $pointer i32)
    (local $discard i32)
    ;; A store is externally observable if the following load traps. Bounds
    ;; checks therefore remain in source evaluation order across the store.
    (local.set $discard (i32.load offset=0 (local.get $pointer)))
    (i32.store offset=4 (local.get $pointer) (i32.const 7))
    (local.set $discard (i32.load offset=12 (local.get $pointer)))
  )
  (func $entry
    (call $access (i32.const 32)))
  (export "main" (func $entry))
)
