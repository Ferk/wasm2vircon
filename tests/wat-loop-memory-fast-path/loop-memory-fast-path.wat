(module
  (memory 1)
  ;; Four iterations. Keeping the bound in writable memory verifies that the
  ;; versioner accepts an aligned invariant load without treating it as a
  ;; source-level constant.
  (data (i32.const 100) "\04\00\00\00")

  ;; A separate countdown loop covers the store-capable form. The runtime
  ;; guard accepts positive counts; zero/negative values retain the checked
  ;; loop's original wrapping behavior.
  (func $store_loop (param $remaining i32)
    (local $pointer i32)
    (local.set $pointer (i32.const 32))
    (loop $write
      (i32.store
        (local.get $pointer)
        (i32.load (local.get $pointer)))
      (i32.store offset=4
        (local.get $pointer)
        (i32.load offset=4 (local.get $pointer)))
      (local.set $pointer
        (i32.add (local.get $pointer) (i32.const 8)))
      (br_if $write
        (local.tee $remaining
          (i32.add (local.get $remaining) (i32.const -1)))))
  )

  (func $entry
    (local $pointer i32)
    (local $index i32)
    (local $discard i32)

    (local.set $pointer (i32.const 0))
    (local.set $index (i32.const 0))
    (call $store_loop (i32.const 4))
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
  )

  (export "main" (func $entry))
)
