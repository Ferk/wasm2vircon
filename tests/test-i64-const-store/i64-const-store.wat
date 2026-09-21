(module
  (memory 2)
  ;; Clang may use this for adjacent i32 initialization. The source-level
  ;; values are two words, despite the transient Wasm i64 transport value.
  (func $entry (result i32)
    ;; This is the normalized shape emitted by the escaping static-pair probe:
    ;; base zero plus the static-data memarg offset.
    (i64.store offset=65536 align=4
      (i32.const 0)
      (i64.const 0x1122334455667788))
    (i32.const 0))
  (export "__original_main" (func $entry)))
