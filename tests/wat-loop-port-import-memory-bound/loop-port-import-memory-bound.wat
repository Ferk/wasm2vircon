(module
  (import "env" "vircon_gpu_set_drawing_point" (func $set_point (param i32 i32)))
  (import "env" "vircon_gpu_draw_region" (func $draw))
  (memory 1)

  ;; The counter limit lives in ordinary writable Wasm memory. Both GPU
  ;; operations are explicitly audited as port-only imports: neither can
  ;; modify this word while the loop is running.
  (data (i32.const 100) "\04\00\00\00")

  (func $entry
    (local $pointer i32)
    (local $index i32)
    (local $discard i32)
    (local.set $pointer (i32.const 0))
    (local.set $index (i32.const 0))
    (loop $scan
      (local.set $discard (i32.load (local.get $pointer)))
      (local.set $discard (i32.load offset=4 (local.get $pointer)))
      (call $set_point (local.get $index) (i32.const 0))
      (call $draw)
      (local.set $pointer
        (i32.add (local.get $pointer) (i32.const 8)))
      (br_if $scan
        (i32.lt_s
          (local.tee $index
            (i32.add (local.get $index) (i32.const 1)))
          (i32.load (i32.const 100)))))
  )

  (export "main" (func $entry))
)
