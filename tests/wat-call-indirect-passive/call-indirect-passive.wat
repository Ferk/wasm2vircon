(module
  (type $signature (func))
  (memory 1)
  (table $functions 1 1 funcref)
  (elem $passive func $target)
  (func $target (type $signature))
  (func $main
    (call_indirect $functions (type $signature) (i32.const 0)))
  (export "main" (func $main)))
