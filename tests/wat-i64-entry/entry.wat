;; An i64 entry result is returned as two words before startup halts.
(module
  (memory 1)
  (func (export "main") (result i64)
    (i64.const 0x123456789abcdef0)))
