(module
  (type $void (func))
  (type $entry_type (func (result i32)))
  (import "env" "unknown" (func $unknown (type $void)))
  (memory 1)
  (func $entry (type $entry_type)
    (loop $loop
      (call $unknown)
      (br $loop)
    )
    unreachable
  )
  (export "__original_main" (func $entry))
)
