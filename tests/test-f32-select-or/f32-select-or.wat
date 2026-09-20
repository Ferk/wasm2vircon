(module
  (memory 1)
  (func (export "__original_main") (result i32)
    (drop
      (select
        (f32.const 1.0)
        (f32.const 2.0)
        (i32.const 1)))
    (i32.or (i32.const 16) (i32.const 2))))
