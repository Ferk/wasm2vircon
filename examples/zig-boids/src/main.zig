//! Entry point for the Zig benchmark cartridge.
const benchmark = @import("benchmark.zig");
/// Satisfies Zig's root-module entry check.
pub fn main() void {}
/// Runs the Vircon32 benchmark controller.
pub export fn vircon_main() callconv(.c) noreturn {
    benchmark.run();
}
