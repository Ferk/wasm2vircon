# Rust Boids

A self-contained freestanding Rust port of the boids benchmark. It has no
Cargo dependencies and uses `#![no_std]` plus `#![no_main]`; `vircon_main` is
the explicit C-ABI Wasm export selected by `wasm2vircon`.

`src/main.rs` owns entry, input, timing and the HUD. `src/boids.rs` owns
flocking state, workload changes, simulation and drawing. `src/vircon.rs` is a
small Rust-facing wrapper around the same hardware-like `env.*` imports used
by the other examples; its text and decimal helpers are ordinary Rust code.

Gamepad 0 **up** and **down** add or remove ten boids within a 10–150 range.

## Build

Install Rust with the `wasm32-unknown-unknown` target, plus `wasm2vircon`,
`assemble`, and `packrom` on `PATH`:

```sh
cmake -S . -B build
cmake --build build
```

The resulting cartridge is `build/RustBoids.v32`.
