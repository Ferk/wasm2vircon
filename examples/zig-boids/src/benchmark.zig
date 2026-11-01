//! Interactive and deterministic benchmark controller.
const v = @import("vircon.zig");
const boids = @import("boids.zig");
const bunnies = @import("bunnies.zig");
const alpha = @import("alpha_swarm.zig");
const Mode = enum { boids, bunnies, alpha };
const Case = struct { mode: Mode, count: i32 };
const cases = [_]Case{ .{ .mode = .boids, .count = 10 }, .{ .mode = .boids, .count = 50 }, .{ .mode = .bunnies, .count = 500 }, .{ .mode = .bunnies, .count = 2500 }, .{ .mode = .alpha, .count = 4 }, .{ .mode = .alpha, .count = 64 } };
const Repeat = struct { direction: i32 = 0, start: u32 = 0, emitted: u32 = 0 };
const Fps = struct { start: u32, completed: u32 = 0, hundredths: u32 = 6000 };
// Read the wrapping display-frame counter.
fn now() u32 {
    return @bitCast(v.frameCounter());
}
// Reset the selected workload.
fn reset(m: Mode) void {
    switch (m) {
        .boids => boids.initialize(),
        .bunnies => bunnies.reset(),
        .alpha => alpha.reset(),
    }
}
// Return the selected workload count.
fn count(m: Mode) i32 {
    return switch (m) {
        .boids => boids.count(),
        .bunnies => bunnies.count(),
        .alpha => alpha.count(),
    };
}
// Select and clamp the workload count.
fn setCount(m: Mode, n: i32) void {
    switch (m) {
        .boids => boids.setCount(n),
        .bunnies => bunnies.setCount(n),
        .alpha => alpha.setCount(n),
    }
}
// Convert a held direction into frame-timed count changes.
fn repeat(r: *Repeat, d: i32, t: u32) i32 {
    if (d == 0) {
        r.direction = 0;
        return 0;
    }
    if (d != r.direction) {
        r.* = .{ .direction = d, .start = t };
        return d;
    }
    const elapsed = t -% r.start;
    const n = if (elapsed < 18) 0 else 1 + (elapsed - 18) / 6;
    const steps = @min(100, n -% r.emitted);
    r.emitted = n;
    return d * @as(i32, @intCast(steps));
}
// Draw the common benchmark status overlay.
fn hud(m: Mode, cpu: i32, gpu: i32, fps: u32, idx: usize) void {
    v.selectTexture(-1);
    v.selectRegion(256);
    v.setBlendingMode(0x20);
    v.setMultiplyColor(@bitCast(@as(u32, 0xb0000000)));
    v.setDrawingScale(380, 70);
    v.drawRegionZoomedAt(5, 5);
    v.setDrawingScale(1, 1);
    v.setMultiplyColor(v.color_white);
    v.printAt(10, 10, switch (m) {
        .boids => "Boids",
        .bunnies => "BunnyMark",
        .alpha => "Alpha Swarm",
    });
    v.printAt(230, 10, "Count:");
    v.printUIntAt(295, 10, @intCast(count(m)));
    v.printAt(10, 30, "FPS:");
    v.printFixed2At(55, 30, @as(f32, @floatFromInt(fps)) / 100);
    v.printAt(120, 30, "GPU%:");
    v.printFixed2At(175, 30, @as(f32, @floatFromInt(gpu)) / 100);
    v.printAt(245, 30, "CPU%:");
    v.printFixed2At(300, 30, @as(f32, @floatFromInt(cpu)) / 100);
    if (idx != 0) {
        v.printAt(10, 50, "Benchmark tour:");
        v.printUIntAt(170, 50, @intCast(idx));
        v.printAt(180, 50, "/6");
    } else v.printAt(10, 50, "L/R mode  DPAD +/-  START tour");
}
// Update, draw, measure, and wait for one benchmark iteration.
fn frame(m: Mode, cpu: *i32, gpu: *i32, fps: *Fps, idx: usize) void {
    const start = v.frameCounter();
    const cycles = v.cycleCounter();
    switch (m) {
        .boids => boids.update(),
        .bunnies => bunnies.update(),
        .alpha => alpha.update(),
    }
    v.clearScreen(v.color_black);
    if (m == .alpha) {
        hud(m, cpu.*, gpu.*, fps.hundredths, idx);
        alpha.draw();
    } else {
        if (m == .boids) boids.draw() else bunnies.draw();
        hud(m, cpu.*, gpu.*, fps.hundredths, idx);
    }
    cpu.* = @divTrunc((v.frameCounter() -% start) *% 250000 +% v.cycleCounter() -% cycles, 25);
    const used = 2073600 - @max(0, @min(2073600, v.getGpuRemainingPixels()));
    gpu.* = @divTrunc(used, 20736) * 100 + @divTrunc(@rem(used, 20736) * 100, 20736);
    v.endFrame();
    fps.completed += 1;
    const t = now();
    const elapsed = t -% fps.start;
    if (elapsed >= 60) {
        fps.hundredths = fps.completed * 6000 / elapsed;
        fps.completed = 0;
        fps.start = t;
    }
}
// Run the fixed cases and publish simulator checkpoints.
fn tour() void {
    var checksum: i32 = 0;
    for (cases, 0..) |c, i| {
        reset(c.mode);
        setCount(c.mode, c.count);
        // Keep setup time and its partial display frame outside the timed case.
        v.endFrame();
        var cpu: i32 = 0;
        var gpu: i32 = 0;
        const start = now();
        var fps = Fps{ .start = start };
        var updates: u32 = 0;
        var cpus: u32 = 0;
        var gpus: u32 = 0;
        while (true) {
            frame(c.mode, &cpu, &gpu, &fps, i + 1);
            updates += 1;
            cpus += @intCast(cpu);
            gpus += @intCast(gpu);
            if (now() -% start >= 60) break;
        }
        const n = count(c.mode);
        const cpu_average: u32 = @min(@as(u32, 262143), cpus / updates);
        const gpu_average: u32 = gpus / updates;
        const encoded: u32 = gpu_average | (cpu_average << 14);
        v.setMultiplyColor(@bitCast(encoded));
        v.setDrawingScale(@as(f32, @floatFromInt(updates * 6000 / (now() -% start))) / 100, 1);
        v.setDrawingPoint(@rem(n, 1000), 1001 + @as(i32, @intCast(i)) + 6 * @divTrunc(n, 1000));
        v.setMultiplyColor(v.color_white);
        v.setDrawingScale(1, 1);
        checksum += (@as(i32, @intCast(i)) + 1) * (n + 1);
    }
    v.setDrawingPoint(@rem(checksum, 997), 6);
}
/// Run the cartridge until the console is powered off.
pub fn run() noreturn {
    var m: Mode = .boids;
    var cpu: i32 = 0;
    var gpu: i32 = 0;
    var fps = Fps{ .start = now() };
    var pl = false;
    var pr = false;
    var ps = false;
    var vertical = Repeat{};
    var horizontal = Repeat{};
    v.selectGamepad(0);
    reset(m);
    fps = .{ .start = now() };
    while (true) {
        const start = v.gamepadButtonStart();
        const l = v.gamepadButtonL();
        const r = v.gamepadButtonR();
        if (start and !ps) {
            tour();
            reset(m);
            cpu = 0;
            gpu = 0;
            fps = .{ .start = now() };
            vertical = .{};
            horizontal = .{};
            ps = true;
            pl = v.gamepadButtonL();
            pr = v.gamepadButtonR();
            continue;
        }
        if ((l and !pl) or (r and !pr)) {
            const x: i32 = @backingInt(m);
            m = @fromBackingInt(@intCast(@mod(x + (if (r and !pr) @as(i32, 1) else -1), 3)));
            reset(m);
            cpu = 0;
            gpu = 0;
            fps = .{ .start = now() };
            vertical = .{};
            horizontal = .{};
        }
        const up: i32 = if (v.gamepadUp()) 1 else if (v.gamepadDown()) -1 else 0;
        const side: i32 = if (v.gamepadRight()) 1 else if (v.gamepadLeft()) -1 else 0;
        const step: i32 = switch (m) {
            .boids => 10,
            .bunnies => 100,
            .alpha => 5,
        };
        const delta = repeat(&vertical, up, now()) * step + repeat(&horizontal, side, now());
        if (delta != 0) setCount(m, count(m) + delta);
        pl = l;
        pr = r;
        ps = start;
        frame(m, &cpu, &gpu, &fps, 0);
    }
}
