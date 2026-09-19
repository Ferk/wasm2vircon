(module
  (memory 1)
  (func $main (result i32)
    (block $outer
      (block $inner
        (br_if $inner (i32.const 1))
        unreachable)
      (br_if $outer (i32.const 1))
      unreachable)
    (i32.const 7))
  (export "__original_main" (func $main)))
