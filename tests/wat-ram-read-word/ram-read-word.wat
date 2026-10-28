(module
  (import "env" "vircon_ram_read_word" (func $ram_read_word (param i32) (result i32)))
  (memory 1)
  (data (i32.const 0) "\78\56\34\12")

  (func $read_dynamic (param $address i32) (result i32)
    (call $ram_read_word (local.get $address)))

  (func $entry
    (drop (call $read_dynamic (i32.const 0)))
    (drop (call $ram_read_word (i32.const 1000000)))
    (drop (call $ram_read_word (i32.const 4194303)))
    (drop (call $ram_read_word (i32.const -1)))
    (drop (call $ram_read_word (i32.const 4194304))))

  (export "main" (func $entry)))
