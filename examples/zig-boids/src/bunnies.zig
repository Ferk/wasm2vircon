//! Deterministic moving BIOS glyphs for the BunnyMark workload.
const v = @import("vircon.zig");
const Bunny = struct { x: f32, y: f32, dx: f32, dy: f32 };
var bunnies: [9999]Bunny = undefined;
var state: [2]i32 = .{ 0, 0 }; // active, initialized

// Initialize one deterministic sprite.
fn initialize(i: i32) void {
    const b = &bunnies[@intCast(i)];
    b.x = @floatFromInt(10 + @mod(i * 13, 600));
    b.y = @floatFromInt(20 + @mod(i * 17, 300));
    b.dx = @as(f32, @floatFromInt((i & 7) - 3)) * 0.5;
    b.dy = @as(f32, @floatFromInt((i & 3) - 2)) * 0.5;
}
/// Restore the reproducible starting state.
pub fn reset() void {
    for (0..500) |i| initialize(@intCast(i));
    state = .{ 100, 500 };
}
/// Change the workload within the supported capacity.
pub fn setCount(value: i32) void {
    const target = @max(0, @min(9999, value));
    while (state[1] < target) {
        initialize(state[1]);
        state[1] += 1;
    }
    state[0] = target;
}
/// Return the active count.
pub fn count() i32 {
    return state[0];
}
/// Advance and bounce all active sprites.
pub fn update() void {
    const active: usize = @intCast(state[0]);
    var i: usize = 0;
    while (i < active) : (i += 1) {
        const b = &bunnies[i];
        b.x += b.dx;
        b.y += b.dy;
        if (b.x < 0 or b.x > 629) b.dx = -b.dx;
        if (b.y < 0 or b.y > 339) b.dy = -b.dy;
    }
}
/// Draw gray BIOS-font glyphs.
pub fn draw() void {
    v.selectTexture(-1);
    v.selectRegion('b');
    v.setMultiplyColor(v.color_gray);
    const active: usize = @intCast(state[0]);
    var i: usize = 0;
    while (i < active) : (i += 1)
        v.drawRegionAt(@intFromFloat(bunnies[i].x), @intFromFloat(bunnies[i].y));
    v.setMultiplyColor(v.color_white);
}
