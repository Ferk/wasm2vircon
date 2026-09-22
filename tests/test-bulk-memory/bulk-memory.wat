(module
  (memory 1)
  (func $entry (result i32)
    (i32.store
      (i32.const 0)
      (i32.const 1144201745))
    ;; Overlap requires backwards memmove-style copying.
    (memory.copy
      (i32.const 1)
      (i32.const 0)
      (i32.const 4))
    ;; Fill two middle lanes without changing their neighbours.
    (memory.fill
      (i32.const 2)
      (i32.const 170)
      (i32.const 2))
    (i32.load
      (i32.const 1)))
  (export "entry" (func $entry)))
