(module
  (import "env" "vircon_gpu_set_drawing_point" (func $set_point (param i32 i32)))
  (memory 1)
  (data (i32.const 100) "\04\00\00\00")

  ;; Both locals are live after the loop. The guarded fast copy must keep them
  ;; in registers across its back-edge, then flush their final values once at
  ;; the join; the checked fallback remains conventionally frame-slot based.
  (func $entry
    (local $pointer i32)
    (local $index i32)
    (local $discard i32)
    (local.set $pointer (i32.const 0))
    (local.set $index (i32.const 0))
    (loop $scan
      (local.set $discard (i32.load (local.get $pointer)))
      (local.set $discard (i32.load offset=4 (local.get $pointer)))
      (local.set $pointer
        (i32.add (local.get $pointer) (i32.const 8)))
      (br_if $scan
        (i32.lt_s
          (local.tee $index
            (i32.add (local.get $index) (i32.const 1)))
          (i32.load (i32.const 100)))))
    (call $set_point (local.get $pointer) (local.get $index)))

  (export "main" (func $entry)))
