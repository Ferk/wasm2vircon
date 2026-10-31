(module
  (import "env" "vircon_gpu_set_drawing_point" (func $set_point (param i32 i32)))
  (memory 1)
  (data (i32.const 0) "\00\00\00\00\00\00\00\00")

  ;; A direct conditional exit followed by an unconditional backedge is the
  ;; structured form emitted for the inlined Zig sprite-update loop.
  (func $run (param $remaining i32) (param $start i32)
    (local $pointer i32)
    (local.set $pointer (local.get $start))
    (block $done
      (loop $again
        (i32.store (local.get $pointer)
          (i32.add (i32.load (local.get $pointer)) (i32.const 1)))
        (i32.store offset=4 (local.get $pointer)
          (i32.add (i32.load offset=4 (local.get $pointer)) (i32.const 2)))
        (local.set $pointer (i32.add (local.get $pointer) (i32.const 8)))
        (br_if $done
          (i32.eqz (local.tee $remaining
            (i32.add (local.get $remaining) (i32.const -1)))))
        (br $again)))
    (call $set_point (local.get $pointer) (local.get $remaining)))

  (func (export "main")
    (call $run (i32.const 4) (i32.const 0))))
