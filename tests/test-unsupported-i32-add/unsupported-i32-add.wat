(module
  (type $entry_type (func (result i32)))
  (memory 1)
  (func $entry (type $entry_type)
    (i32.add (i32.const 1) (i32.const 2))
  )
  (export "__original_main" (func $entry))
)
