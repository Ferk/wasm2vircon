(module
  (import "env" "vircon_timer_get_frame_counter" (func $frame_counter (result i32)))
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $discard i32)
    (local.set $pointer (i32.const 4))
    (block $skip_unaligned_assignment
      (br_if $skip_unaligned_assignment (call $frame_counter))
      (local.set $pointer (i32.const 1))
    )
    ;; Both paths reach this access, so the compiler must retain the unaligned
    ;; fallback even though one path keeps the aligned value 4.
    (local.set $discard (i32.load (local.get $pointer)))
    ;; A separately proven constant address still uses the direct word path.
    (local.set $discard (i32.load (i32.const 8)))
  )
  (export "main" (func $entry))
)
