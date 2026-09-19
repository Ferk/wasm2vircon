(module
  (memory 1)
  (func $main (result i32)
    (local $value i32)
    (local.set $value
      (i32.eqz (i32.const 0)))
    (local.set $value
      (select
        (i32.const 17)
        (i32.const 23)
        (i32.eq (local.get $value) (i32.const 1))))
    (local.get $value))
  (export "__original_main" (func $main)))
