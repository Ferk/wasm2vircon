(module
  (memory 1)
  (func $main (result i32)
    i32.const -42
    i32.const 6
    i32.div_s)
  (export "__original_main" (func $main)))
