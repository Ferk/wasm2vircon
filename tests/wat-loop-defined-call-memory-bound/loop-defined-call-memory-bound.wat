(module
  (memory 1)
  (data (i32.const 100) "\04\00\00\00")

  ;; A normal defined call has unrestricted linear-memory effects. It blocks
  ;; retention of the loop's memory-backed limit even though this particular
  ;; callee body is empty.
  (func $touch)

  (func $entry
    (local $pointer i32)
    (local $index i32)
    (local.set $pointer (i32.const 0))
    (local.set $index (i32.const 0))
    (loop $scan
      (drop (i32.load (local.get $pointer)))
      (drop (i32.load offset=4 (local.get $pointer)))
      (call $touch)
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
