# TinyGo Boids

This is a self-contained TinyGo port of the C boids benchmark in
[`../c-boids`](../c-boids). It preserves the deliberately simple O(N²)
flocking algorithm, BIOS-font HUD, cyan BIOS-pixel rendering, and gamepad-0
up/down workload controls.

The program starts with 50 boids. Press **up** to add ten and **down** to
remove ten; the active range is 10–150. The HUD reports occupied simulation
frame slots and a `60 / update_frames` update-rate estimate.

## Requirements

- TinyGo and a host Go version supported by that TinyGo release;
- CMake;
- a prebuilt `wasm2vircon` executable on `PATH`; and
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

The benchmark has no external resources, so it does not need `png2vircon` or
`wav2vircon`. It uses TinyGo directly rather than Clang/`wasm-ld`; Binaryen
development files are required only to build wasm2vircon from source.

## Build

From this directory, run:

```sh
cmake -S . -B build
cmake --build build
```

The result is `build/TinyGoBoids.v32`. The default CMake target is `v32`, so
`cmake --build build --target v32` is equivalent. `CMakeLists.txt` is deliberately
reusable: change `project_name`, `tinygo_target`, and `wasm_entry` near its
top, then update `rom.xml.in` when copying this example into another cartridge.

The TinyGo input profile is intentionally narrow:

```text
tinygo build -target wasm-unknown -gc=leaking -scheduler=none \
  -panic=trap -no-debug -opt=1
```

`wasm-unknown` avoids WASI/JavaScript imports. The program uses direct
`//go:wasmimport env ...` declarations for only the Vircon32 hardware-like
operations it needs, while text and decimal rendering remain ordinary Go
functions. `//go:export vircon_main` is a direct entry export; it intentionally
does not use TinyGo's `//go:wasmexport` reactor wrapper, which would require a
separate `_initialize` call before the cartridge entry.

`-opt=1` is intentional. TinyGo's more aggressive `-opt=2` currently turns
part of the flock update into a value-carrying structured branch, which is
outside the deliberately resultless VirconWasm control-flow subset. The
`-opt=1` output retains the same application behavior and passes the supported
frontend profile.

TinyGo itself requires a Go toolchain version compatible with the TinyGo build.
If `tinygo build` reports a host Go version mismatch, install/select the Go
version that TinyGo documents before building; this is a TinyGo host-toolchain
requirement, not a VirconWasm compatibility failure.
