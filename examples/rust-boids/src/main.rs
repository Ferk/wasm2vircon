//! Freestanding Rust cartridge entry for the benchmark suite.
#![no_std]
#![no_main]
mod alpha_swarm;
mod benchmark;
mod boids;
mod bunnies;
mod vircon;
use core::panic::PanicInfo;
/// No recoverable failures are supported in this freestanding program.
#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    loop {}
}
/// Enter the benchmark controller through the C ABI.
#[unsafe(no_mangle)]
pub extern "C" fn vircon_main() -> ! {
    benchmark::run()
}
