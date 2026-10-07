(module
  (memory 1 2)

  ;; The pointer is deliberately a parameter so its alignment is not known
  ;; statically. The fast copy must be selected only after a runtime alignment
  ;; and current-memory-span guard; unusual inputs retain the checked copy.
  (func $update (param $remaining i32) (param $pointer i32)
    (loop $items
      (f32.store
        (local.get $pointer)
        (f32.add
          (f32.load (local.get $pointer))
          (f32.load offset=8 (local.get $pointer))))
      (f32.store offset=4
        (local.get $pointer)
        (f32.add
          (f32.load offset=4 (local.get $pointer))
          (f32.load offset=12 (local.get $pointer))))
      (local.set $pointer
        (i32.add (local.get $pointer) (i32.const 16)))
      (br_if $items
        (local.tee $remaining
          (i32.add (local.get $remaining) (i32.const -1)))))
  )

  (func $entry
    ;; Keeping memory.grow reachable verifies that loop optimization uses the
    ;; runtime page count instead of disabling all fast paths in the module.
    (drop (memory.grow (i32.const 1)))
    (call $update (i32.const 4) (i32.const 32))
  )

  (export "main" (func $entry))
)
