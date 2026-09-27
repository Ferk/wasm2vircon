// Reusable Zig build driver for a freestanding Vircon32 cartridge.
//
// Copy this directory to start another Zig project, then change the project
// constants below. Required external tools are resolved through PATH.

const std = @import("std");

const project_name = "zig-boids";
const entry_name = "vircon_main";
const source_path = "src/main.zig";

/// Builds the freestanding Wasm input, V32 program binary, and cartridge ROM.
pub fn build(b: *std.Build) void {
    const v32 = b.step("v32", "Build the complete Vircon32 ROM");
    const wasm = b.step("wasm", "Build the freestanding Wasm input");

    std.Io.Dir.cwd().createDirPath(b.graph.io, "build") catch |err| {
        std.process.fatal("cannot create build directory: {s}", .{@errorName(err)});
    };

    const wasm_path = b.fmt("build/{s}.wasm", .{project_name});
    const asm_path = b.fmt("build/{s}.asm", .{project_name});
    const vbin_path = b.fmt("build/{s}.vbin", .{project_name});
    const rom_path = b.fmt("build/{s}.v32", .{project_name});
    const entry_export = b.fmt("--export={s}", .{entry_name});

    const compile = b.addSystemCommand(&.{
        b.graph.zig_exe, "build-exe", source_path, "-target", "wasm32-freestanding",
        "-O", "ReleaseFast", "-fno-entry", entry_export, "-fstrip",
        b.fmt("-femit-bin={s}", .{wasm_path}),
    });
    wasm.dependOn(&compile.step);

    const translate = b.addSystemCommand(&.{ "wasm2vircon", wasm_path, "--entry", entry_name, "-o", asm_path });
    translate.step.dependOn(&compile.step);
    const assemble = b.addSystemCommand(&.{ "assemble", "-o", vbin_path, asm_path });
    assemble.step.dependOn(&translate.step);
    const pack = b.addSystemCommand(&.{ "packrom", "-o", rom_path, "rom.xml" });
    pack.step.dependOn(&assemble.step);
    v32.dependOn(&pack.step);
    b.default_step.dependOn(v32);
}
