(module
  (memory 1)
  (table $dispatch 1 funcref)
  (export "dispatch" (table $dispatch))
  (func $main (result i32)
    (i32.const 0))
  (export "main" (func $main)))
