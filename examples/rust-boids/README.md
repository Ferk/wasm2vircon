# Rust Boids

A self-contained freestanding Rust port of the boids benchmark. It has no
Cargo dependencies and uses `#![no_std]` plus `#![no_main]`; `vircon_main` is
the explicit C-ABI Wasm export selected by `wasm2vircon`.

`src/main.rs` owns entry, input, timing and the HUD. `src/boids.rs` owns
flocking state, workload changes, simulation and drawing. `src/vircon.rs` is a
small Rust-facing wrapper around the same hardware-like `env.*` imports used
by the other examples; its text and decimal helpers are ordinary Rust code.

Gamepad 0 **up** and **down** add or remove ten boids within a 10–150 range.

## Requirements

- a Rust toolchain with Cargo;
- Rust's `wasm32-unknown-unknown` target:

  ```sh
  rustup target add wasm32-unknown-unknown
  ```

- CMake;
- a prebuilt `wasm2vircon` executable on `PATH`; and
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

It has no Cargo dependencies or external assets. It does not require Clang,
`wasm-ld`, Binaryen development files, `png2vircon`, or `wav2vircon` merely to
build the ROM.

## Build

From this directory, run:

```sh
cmake -S . -B build
cmake --build build
```

The resulting cartridge is `build/RustBoids.v32`.
