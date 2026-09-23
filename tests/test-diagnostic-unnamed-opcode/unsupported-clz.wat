(module
  (memory 1)
  (func (result i32)
    (i32.clz (i32.const 1)))
  (func $world_initialize (result i32)
    (call 0))
  (export "__original_main" (func $world_initialize)))
