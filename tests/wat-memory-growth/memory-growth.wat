;; Exercises current-page reporting, successful growth, failure at the
;; declared maximum, and access to a newly allocated zero-filled page.
(module
  (memory 1 2)
  (func $main (result i32)
    (drop (memory.size))
    (drop (memory.grow (i32.const 1)))
    (i32.store (i32.const 65536) (i32.const 305419896))
    (drop (memory.grow (i32.const 1)))
    (i32.load (i32.const 65536)))
  (export "main" (func $main)))
