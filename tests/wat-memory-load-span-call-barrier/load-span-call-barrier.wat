(module
  (import "env" "vircon_timer_get_current_time" (func $current_time (result i32)))
  (memory 1)
  (func $entry
    (local $pointer i32)
    (local $sum i32)
    (local.set $pointer (i32.const 32))

    ;; The hardware read between these loads is observable, so a failure of
    ;; the second load must not be moved ahead of that import call.
    (local.set $sum
      (i32.add
        (i32.load offset=0 (local.get $pointer))
        (i32.add
          (call $current_time)
          (i32.load offset=12 (local.get $pointer)))))
  )
  (export "main" (func $entry))
)
