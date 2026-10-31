//! Translucent rotating BIOS glyphs for GPU load measurement.
const v = @import("vircon.zig");
const Particle = struct { x: i32, y: i32, dx: i32, dy: i32, angle: f32 };
var particles: [96]Particle = undefined;
var active: [1]i32 = .{4};
// Align the glyph with its movement direction.
fn angle(p: *const Particle) f32 {
    return v.atan2(@floatFromInt(p.dy), @floatFromInt(p.dx)) - 3.1415926 / 2.0;
}
/// Initialize particles and copy the current BIOS glyph region.
pub fn reset() void {
    active[0] = 4;
    for (&particles, 0..) |*p, u| {
        const i: i32 = @intCast(u);
        p.* = .{ .x = 114 + @mod(i * 83, 413), .y = 194 + @mod(i * 29, 53), .dx = if (i & 1 != 0) 3 else -3, .dy = if (i & 2 != 0) 2 else -2, .angle = 0 };
        p.angle = angle(p);
    }
    v.selectTexture(-1);
    v.selectRegion('v');
    const point = v.getRegionMinimum();
    v.selectRegion(257);
    v.defineRegion(point.x, point.y, point.x + 9, point.y + 19, point.x + 5, point.y + 10);
}
/// Select a count within the fixed array.
pub fn setCount(n: i32) void {
    active[0] = @max(0, @min(96, n));
}
/// Return the number of active layers.
pub fn count() i32 {
    return active[0];
}
/// Move layers and turn at the screen edges.
pub fn update() void {
    var i: usize = 0;
    while (i < @as(usize, @intCast(active[0]))) : (i += 1) {
        const p = &particles[i];
        var bounced = false;
        p.x += p.dx;
        p.y += p.dy;
        if (p.x < 114 or p.x > 526) {
            p.x = @max(114, @min(526, p.x));
            p.dx = -p.dx;
            bounced = true;
        }
        if (p.y < 194 or p.y > 246) {
            p.y = @max(194, @min(246, p.y));
            p.dy = -p.dy;
            bounced = true;
        }
        if (bounced) p.angle = angle(p);
    }
}
/// Draw and restore the shared GPU state.
pub fn draw() void {
    const colors = [_]u32{ 0x70ff4030, 0x7040d060, 0x703060ff, 0x70e0c040 };
    v.selectTexture(-1);
    v.selectRegion(257);
    v.setBlendingMode(0x20);
    v.setDrawingScale(10, 10);
    var i: usize = 0;
    while (i < @as(usize, @intCast(active[0]))) : (i += 1) {
        const p = &particles[i];
        v.setMultiplyColor(@bitCast(colors[i & 3]));
        v.setDrawingAngle(p.angle);
        v.drawRegionRotozoomedAt(p.x, p.y);
    }
    v.setDrawingScale(1, 1);
    v.setDrawingAngle(0);
    v.setMultiplyColor(v.color_white);
}
