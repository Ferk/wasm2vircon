(module
  (memory 1)
  (data (i32.const 100) "\04\00\00\00")

  ;; The store can alias the memory-backed loop limit. The versioner must not
  ;; retain that limit in R10 or create a fast copy around this loop.
  (func $entry
    (local $pointer i32)
    (local $index i32)
    (local.set $pointer (i32.const 0))
    (local.set $index (i32.const 0))
    (loop $scan
      (i32.store offset=8 (local.get $pointer) (i32.const 0))
      (drop (i32.load (local.get $pointer)))
      (drop (i32.load offset=4 (local.get $pointer)))
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
