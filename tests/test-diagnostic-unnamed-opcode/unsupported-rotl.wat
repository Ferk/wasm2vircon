(module
  (memory 1)
  (func (result i32)
    (i32.rotl (i32.const 1) (i32.const 2)))
  (func $world_initialize (result i32)
    (call 0))
  (export "__original_main" (func $world_initialize)))
