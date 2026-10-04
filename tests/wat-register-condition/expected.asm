# The cached right square must move out of R1 before the left square reloads.
  fmul R1, R2
  mov R2, R1
  mov R1, [BP-3]
  fadd R1, R2
