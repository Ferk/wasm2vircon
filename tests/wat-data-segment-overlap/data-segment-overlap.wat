(module
  (memory 1)
  ;; Later active segments overwrite only their own Wasm bytes.
  (data (i32.const 0) "\11\22")
  (data (i32.const 1) "\AA\BB")
  (data (i32.const 3) "\DD")
  (data (i32.const 4) "\55")
  (func $entry (result i32) (i32.const 0))
  (export "__original_main" (func $entry))
)
