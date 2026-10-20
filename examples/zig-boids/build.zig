// Reusable Zig build driver for a freestanding Vircon32 cartridge.
//
// Copy this directory to start another Zig project, then change the project
// constants below. Required external tools are resolved through PATH.

const std = @import("std");

const project_name = "zig-boids";
const rom_title = "Zig Boids";
const rom_version = "1.0";
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
    const xml_path = b.fmt("build/{s}.rom.xml", .{project_name});
    const rom_path = b.fmt("build/{s}.v32", .{project_name});
    const entry_export = b.fmt("--export={s}", .{entry_name});

    writeRomDefinition(b, xml_path);

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
    const pack = b.addSystemCommand(&.{ "packrom", "-o", rom_path, xml_path });
    pack.step.dependOn(&assemble.step);
    v32.dependOn(&pack.step);
    b.default_step.dependOn(v32);
}

/// Writes the generated ROM definition beside the program binary.
fn writeRomDefinition(b: *std.Build, xml_path: []const u8) void {
    const xml = b.fmt(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n" ++
            "<rom-definition version=\"1.0\">\n" ++
            "  <rom type=\"cartridge\" title=\"{s}\" version=\"{s}\" />\n" ++
            "  <binary path=\"{s}.vbin\" />\n" ++
            "  <textures />\n" ++
            "  <sounds />\n" ++
            "</rom-definition>\n",
        .{ rom_title, rom_version, project_name },
    );
    std.Io.Dir.cwd().writeFile(b.graph.io, .{ .sub_path = xml_path, .data = xml }) catch |err| {
        std.process.fatal("cannot write ROM definition: {s}", .{@errorName(err)});
    };
}
