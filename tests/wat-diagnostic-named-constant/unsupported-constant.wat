(module
  (memory 1)
  (func $world_initialize (result i32)
    (block (result i32)
      (drop (i64.const 1))
      (i32.const 0)))
  (export "world_initialize" (func $world_initialize)))
