;; ReleaseSmall-style structured nesting must not depend on a fixed backend
;; target-stack limit. Every block is referenced so wasm-as retains its label.
(module
  (memory 1)
  (func $main
    (block $b00
      (br_if $b00 (i32.const 0))
      (block $b01
        (br_if $b01 (i32.const 0))
        (block $b02
          (br_if $b02 (i32.const 0))
          (block $b03
            (br_if $b03 (i32.const 0))
            (block $b04
              (br_if $b04 (i32.const 0))
              (block $b05
                (br_if $b05 (i32.const 0))
                (block $b06
                  (br_if $b06 (i32.const 0))
                  (block $b07
                    (br_if $b07 (i32.const 0))
                    (block $b08
                      (br_if $b08 (i32.const 0))
                      (block $b09
                        (br_if $b09 (i32.const 0))
                        (block $b10
                          (br_if $b10 (i32.const 0))
                          (block $b11
                            (br_if $b11 (i32.const 0))
                            (block $b12
                              (br_if $b12 (i32.const 0))
                              (block $b13
                                (br_if $b13 (i32.const 0))
                                (block $b14
                                  (br_if $b14 (i32.const 0))
                                  (block $b15
                                    (br_if $b15 (i32.const 0))
                                    (block $b16
                                      (br_if $b16 (i32.const 0))
                                      (block $b17
                                        (br_if $b17 (i32.const 0))
                                        (block $b18
                                          (br_if $b18 (i32.const 0))
                                          (block $b19
                                            (br_if $b19 (i32.const 0))
                                            (block $b20
                                              (br_if $b20 (i32.const 0))
                                              (block $b21
                                                (br_if $b21 (i32.const 0))
                                                (block $b22
                                                  (br_if $b22 (i32.const 0))
                                                  (block $b23
                                                    (br_if $b23 (i32.const 0))
                                                    (block $b24
                                                      (br_if $b24 (i32.const 0))
                                                      (block $b25
                                                        (br_if $b25 (i32.const 0))
                                                        (block $b26
                                                          (br_if $b26 (i32.const 0))
                                                          (block $b27
                                                            (br_if $b27 (i32.const 0))
                                                            (block $b28
                                                              (br_if $b28 (i32.const 0))
                                                              (block $b29
                                                                (br_if $b29 (i32.const 0))
                                                                (block $b30
                                                                  (br_if $b30 (i32.const 0))
                                                                  (block $b31
                                                                    (br_if $b31 (i32.const 0))
                                                                    (block $b32
                                                                      (br_if $b32 (i32.const 0))
                                                                      (block $b33
                                                                        (br_if $b33 (i32.const 0))
                                                                        (block $b34
                                                                          (br_if $b34 (i32.const 0))
                                                                          (block $b35
                                                                            (br_if $b35 (i32.const 0))
                                                                            (block $b36
                                                                              (br_if $b36 (i32.const 0))
                                                                              (block $b37
                                                                                (br_if $b37 (i32.const 0))
                                                                                (block $b38
                                                                                  (br_if $b38 (i32.const 0))
                                                                                  (block $b39
                                                                                    (br_if $b39 (i32.const 0))
                                                                                    (nop))))))))))))))))))))))))))))))))))))))))))
  (export "main" (func $main)))
