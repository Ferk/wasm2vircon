(module
  (memory 1)
  (func $main (result i32)
    (drop
      (i32.rem_s
        (i32.const -42)
        (i32.const 6)))
    (drop
      (i32.rem_s
        (i32.const 0x80000000)
        (i32.const -1)))
    i32.const -42
    i32.const 6
    i32.div_s)
  (export "__original_main" (func $main)))
