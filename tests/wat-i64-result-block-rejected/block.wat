;; i64 structured branch results remain outside the function-call ABI change.
(module
  (memory 1)
  (func (export "main") (result i64)
    (block $result (result i64)
      (br $result (i64.const 42)))))
