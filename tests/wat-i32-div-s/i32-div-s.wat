(module
  (memory 1)
  (func $operations (param $dividend i32) (param $divisor i32) (result i32)
    (drop
      (i32.rem_s
        (local.get $dividend)
        (local.get $divisor)))
    (drop
      (i32.rem_s
        (i32.const 0x80000000)
        (local.get $divisor)))
    local.get $dividend
    local.get $divisor
    i32.div_s)
  (func $main (result i32)
    (call $operations (i32.const -42) (i32.const 6)))
  (export "__original_main" (func $main)))
