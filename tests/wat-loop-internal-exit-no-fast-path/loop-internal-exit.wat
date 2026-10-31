(module
  (memory 1)
  (func (export "main")
    (local $pointer i32)
    (local $remaining i32)
    (local.set $pointer (i32.const 0))
    (local.set $remaining (i32.const 4))
    (loop $again
      (block $inside
        (i32.store (local.get $pointer)
          (i32.load (local.get $pointer)))
        (i32.store offset=4 (local.get $pointer)
          (i32.load offset=4 (local.get $pointer)))
        (local.set $pointer (i32.add (local.get $pointer) (i32.const 8)))
        ;; This target belongs to the loop body, not an enclosing block.
        (br_if $inside
          (i32.eqz (local.tee $remaining
            (i32.add (local.get $remaining) (i32.const -1)))))
        (br $again)))))
