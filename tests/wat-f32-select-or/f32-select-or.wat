(module
  (memory 1)
  (func $operations (param $left i32) (param $condition i32) (result i32)
    (drop
      (select
        (f32.const 1.0)
        (f32.const 2.0)
        (local.get $condition)))
    (i32.or (local.get $left) (i32.const 2)))
  (func (export "__original_main") (result i32)
    (call $operations (i32.const 16) (i32.const 1))))
