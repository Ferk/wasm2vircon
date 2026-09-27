(module
  (memory 1)

  ;; This models a frontend runtime helper that is exported for its own ABI,
  ;; but is unreachable from the selected cartridge entry.
  (func $foreign_f64 (export "foreign_f64") (param f64) (result f64)
    local.get 0
  )

  (func $main (export "main")
    nop
    (loop $forever
      (br $forever)
    )
  )
)
