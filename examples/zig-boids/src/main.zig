//! Cartridge entry and frame presentation for the Zig boids benchmark.
//!
//! Simulation state and flocking work live in boids.zig, mirroring the C and
//! TinyGo examples' separation between application flow and benchmark logic.

const boids = @import("boids.zig");
const vircon = @import("vircon.zig");

/// Satisfies Zig's root-module entry check; the cartridge entry is below.
pub fn main() void {}

/// Runs the never-ending frame loop through the explicit C-ABI Wasm export.
pub export fn vircon_main() callconv(.c) void {
    var previous_up_pressed = false;
    var previous_down_pressed = false;
    boids.initialize();

    while (true) {
        vircon.selectGamepad(0);
        const up_pressed = vircon.gamepadUp();
        const down_pressed = vircon.gamepadDown();
        if (up_pressed and !previous_up_pressed)
            boids.increase()
        else if (down_pressed and !previous_down_pressed)
            boids.decrease();
        previous_up_pressed = up_pressed;
        previous_down_pressed = down_pressed;

        const update_start_frame = vircon.frameCounter();
        boids.update();
        const update_frames = vircon.frameCounter() -% update_start_frame +% 1;
        const fps = @as(f32, @floatFromInt(vircon.frames_per_second)) /
            @as(f32, @floatFromInt(update_frames));

        vircon.clearScreen(vircon.color_black);
        boids.draw();
        vircon.printAt(10, 10, "Boids:");
        vircon.printUIntAt(80, 10, @intCast(boids.count()));
        vircon.printAt(120, 10, "(up/down +/-10)");
        vircon.printAt(10, 30, "Update frames:");
        vircon.printUIntAt(160, 30, @intCast(update_frames));
        vircon.printAt(10, 50, "FPS:");
        vircon.printFixed2At(60, 50, fps);
        vircon.endFrame();
    }
}
