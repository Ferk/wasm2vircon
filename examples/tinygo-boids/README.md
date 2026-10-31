# TinyGo Boids benchmarks

This freestanding TinyGo example implements the Boids, BunnyMark, and Alpha
Swarm workloads from the [C benchmark](../c-boids). Workload state, rendering,
and the benchmark controller are ordinary TinyGo code; no textures or sounds
are required. The source files keep the workloads separate from the controller.

Use **L/R** to switch workloads, **up/down** to change the count in batches,
and **left/right** to change it by one. Holding a direction repeats using the
frame counter. **Start** runs the same six one-second cases as the C ROM and
then returns to interactive mode. The HUD shows the completed-frame FPS and
CPU/GPU usage. The optional [benchmark test](../test/README.md) collects and
compares the language tours with v32sim.

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
`cmake --build build --target v32` is equivalent. The cartridge configuration
at the top of `CMakeLists.txt` defines the project name, entry export, TinyGo
options, source package, and optional texture and sound lists. Go files below
the source directory are tracked as dependencies; add non-Go inputs to
`project_extra_dependencies` when needed.

The build generates `TinyGoBoids.rom.xml` and, when resources are listed,
uses the official converters to create numbered textures and sounds. Set
`WASM2VIRCON_EXECUTABLE` to select a particular translator. The intermediate
Wasm, assembly, and program binary have separate build steps.

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

`-opt=1` is the profile currently used for this example and its benchmark
results. Other optimization levels may produce different Wasm and performance.

TinyGo itself requires a Go toolchain version compatible with the TinyGo build.
If `tinygo build` reports a host Go version mismatch, install/select the Go
version that TinyGo documents before building; this is a TinyGo host-toolchain
requirement, not a VirconWasm compatibility failure.
