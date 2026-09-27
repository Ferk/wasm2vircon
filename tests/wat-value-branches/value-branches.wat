(module
  (memory 1)
  (func (export "__original_main") (result i32)
    (block $outer (result i32)
      (drop
        (br_if $outer
          (i32.const 7)
          (i32.const 1)))
      (block $inner (result i32)
        (i32.const 9)
        (br $inner)))))
