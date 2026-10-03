(module
  (memory 1)
  (func $main
    (drop
      (i32.clz
        (i32.const 1))))
  (export "main" (func $main)))
