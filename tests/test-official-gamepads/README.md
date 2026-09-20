# Official Gamepads port

Upstream reference: `reference/ConsoleSoftware/TestPrograms/Test-Gamepads/`.

This directory owns its normal-C source and texture. The port preserves the
official four-gamepad screen and uses the shared ROM builder; no source or
runtime build dependency points into `reference/`.

The only meaningful adaptation is that the two fixed zoom scales use their
IEEE-754 `float` bit patterns through `set_drawing_scale_bits`. This preserves
the Vircon GPU's exact `320.0f`, `180.0f`, and `1.0f` values while avoiding a
speculative general float extension in the integer-only VirconWasm profile.
