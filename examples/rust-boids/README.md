# Rust Boids benchmarks

This freestanding Rust example implements the Boids, BunnyMark, and Alpha
Swarm workloads from the [C benchmark](../c-boids). Workload state, rendering,
and the benchmark controller are ordinary Rust code; no textures or sounds
are required. The source files keep the workloads separate from the controller.

Use **L/R** to switch workloads, **up/down** to change the count in batches,
and **left/right** to change it by one. Holding a direction repeats using the
frame counter. **Start** runs the same six one-second cases as the C ROM and
then returns to interactive mode. The HUD shows the completed-frame FPS and
CPU/GPU usage. The optional [benchmark test](../test/README.md) collects and
compares the language tours with v32sim.

## Requirements

- a Rust toolchain with Cargo;
- Rust's `wasm32-unknown-unknown` target:

  ```sh
  rustup target add wasm32-unknown-unknown
  ```

- a prebuilt `wasm2vircon` executable on `PATH`; and
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

It has no Cargo dependencies or external assets. It does not require Clang,
`wasm-ld`, Binaryen development files, `png2vircon`, or `wav2vircon` merely to
build the ROM.

## Build

From this directory, run:

```sh
cargo v32
```

The resulting cartridge is `target/RustBoids.v32`. This Cargo alias invokes
the small `xtask` build driver, which compiles the Wasm input and runs
`wasm2vircon`, `assemble`, and `packrom` from `PATH`. It generates
`target/RustBoids.rom.xml` and the intermediate assembly and binary there.

To build only the Wasm input, use `cargo v32 --wasm-only`. Set
`WASM2VIRCON_EXECUTABLE` to select a particular translator and
`CARGO_TARGET_DIR` to choose a different output directory. Both the Wasm and
ROM artifacts follow that target directory. The project name, Wasm filename,
and entry export are constants near the top of `xtask/src/main.rs` when
adapting the build for another cartridge.
