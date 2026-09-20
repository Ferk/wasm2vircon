# Official MathFunctions port

Upstream: `ConsoleSoftware/TestPrograms/Test-MathFunctions/`.

This project-owned port retains the eight graph functions, texture-region
layout, and edge-triggered gamepad tab selection. The only intentional source
adaptation is storing the selected tab in `main` rather than using the
official compiler's global: this preserves program behavior while avoiding an
otherwise avoidable linker-generated Wasm stack-pointer global.

The math functions are ordinary C runtime definitions in
`runtime/vircon_math.c`; they are not compiler intrinsics. The source expects
the self-contained `Texture-MathFunctions.png` asset to be packaged as the
first cartridge texture.
