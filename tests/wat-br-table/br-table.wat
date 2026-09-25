(module
  (memory 1)

  ;; Cases target three different active structured scopes. Case 0 exits the
  ;; innermost block, case 1 restarts the loop, and case 2/default exit outer.
  (func $dispatch (param $selector i32)
    (block $exit
      (loop $retry
        (block $inner
          (br_table $inner $retry $exit $exit (local.get $selector))
        )
      )
    )
  )

  (func $main
    (call $dispatch (i32.const 0))
  )
  (export "main" (func $main))
)
