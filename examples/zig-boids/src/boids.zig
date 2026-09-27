//! Flocking state and simulation for the Zig boids benchmark.
//!
//! The cartridge entry and frame presentation live in main.zig. Keeping this
//! module focused on the workload makes the benchmark logic reusable.

const vircon = @import("vircon.zig");

const max_boid_count: i32 = 150;
const initial_boid_count: i32 = 50;
const boid_count_step: i32 = 10;

const width: f32 = 640.0;
const height: f32 = 360.0;
const view_range: f32 = 60.0;
const avoid_range: f32 = 20.0;
const centering_factor: f32 = 0.005;
const matching_factor: f32 = 0.05;
const avoid_factor: f32 = 0.05;
const max_speed: f32 = 4.0;
const min_speed: f32 = 2.0;
const margin: f32 = 50.0;
const turn_factor: f32 = 0.2;

const Boid = struct {
    x: f32,
    y: f32,
    vx: f32,
    vy: f32,
};

var boids: [@intCast(max_boid_count)]Boid = undefined;
var boid_count_storage: [1]i32 = .{initial_boid_count};
var random_state_storage: [1]u32 = .{1};

/// Provides addressable state so it belongs to ordinary Wasm linear memory.
fn boidCount() *i32 {
    return &boid_count_storage[0];
}

/// Returns the active workload rather than the fixed array capacity.
pub fn count() i32 {
    return boidCount().*;
}

/// Advances the benchmark's deterministic local pseudo-random generator.
fn randomU32() u32 {
    const state = &random_state_storage[0];
    state.* = state.* *% 1_664_525 +% 1_013_904_223;
    return state.*;
}

/// Generates a deterministic float uniformly across the requested interval.
fn randomFloat(min: f32, max: f32) f32 {
    const value = randomU32() & 0xffff;
    const fraction: f32 = @floatFromInt(value);
    return min + (max - min) * (fraction / 65535.0);
}

/// Initializes one newly enabled boid from the deterministic local generator.
fn initializeBoid(index: usize) void {
    boids[index].x = randomFloat(100.0, width - 100.0);
    boids[index].y = randomFloat(100.0, height - 100.0);
    boids[index].vx = randomFloat(-2.0, 2.0);
    boids[index].vy = randomFloat(-2.0, 2.0);
}

/// Initializes the visible starting workload.
pub fn initialize() void {
    var index: i32 = 0;
    while (index < boidCount().*) : (index += 1) {
        initializeBoid(@intCast(index));
    }
}

/// Adds one fixed batch without exceeding the benchmark's fixed capacity.
pub fn increase() void {
    const count_pointer = boidCount();
    var target = count_pointer.* + boid_count_step;
    if (target > max_boid_count) target = max_boid_count;
    while (count_pointer.* < target) {
        initializeBoid(@intCast(count_pointer.*));
        count_pointer.* += 1;
    }
}

/// Removes one batch while retaining at least one visible boid.
pub fn decrease() void {
    const count_pointer = boidCount();
    if (count_pointer.* > boid_count_step)
        count_pointer.* -= boid_count_step
    else
        count_pointer.* = 10;
}

/// Updates one deliberate O(N²) flocking simulation step.
pub fn update() void {
    var i: i32 = 0;
    while (i < boidCount().*) : (i += 1) {
        var center_x: f32 = 0.0;
        var center_y: f32 = 0.0;
        var average_vx: f32 = 0.0;
        var average_vy: f32 = 0.0;
        var avoid_x: f32 = 0.0;
        var avoid_y: f32 = 0.0;
        var neighbors: i32 = 0;

        var j: i32 = 0;
        while (j < boidCount().*) : (j += 1) {
            if (i == j) continue;

            const current = &boids[@intCast(i)];
            const other = &boids[@intCast(j)];
            const dx = current.x - other.x;
            const dy = current.y - other.y;
            const distance_squared = dx * dx + dy * dy;

            if (distance_squared < view_range * view_range) {
                center_x += other.x;
                center_y += other.y;
                average_vx += other.vx;
                average_vy += other.vy;
                neighbors += 1;
            }
            if (distance_squared < avoid_range * avoid_range) {
                avoid_x += dx;
                avoid_y += dy;
            }
        }

        const current = &boids[@intCast(i)];
        if (neighbors > 0) {
            const neighbor_count: f32 = @floatFromInt(neighbors);
            center_x /= neighbor_count;
            center_y /= neighbor_count;
            average_vx /= neighbor_count;
            average_vy /= neighbor_count;
            current.vx += (center_x - current.x) * centering_factor;
            current.vy += (center_y - current.y) * centering_factor;
            current.vx += (average_vx - current.vx) * matching_factor;
            current.vy += (average_vy - current.vy) * matching_factor;
        }

        current.vx += avoid_x * avoid_factor;
        current.vy += avoid_y * avoid_factor;
        if (current.x < margin) current.vx += turn_factor;
        if (current.x > width - margin) current.vx -= turn_factor;
        if (current.y < margin) current.vy += turn_factor;
        if (current.y > height - margin) current.vy -= turn_factor;

        const speed_squared = current.vx * current.vx + current.vy * current.vy;
        if (speed_squared > max_speed * max_speed) {
            current.vx *= 0.9;
            current.vy *= 0.9;
        }
        if (speed_squared < min_speed * min_speed) {
            current.vx *= 1.1;
            current.vy *= 1.1;
        }
        current.x += current.vx;
        current.y += current.vy;
    }
}

/// Draws every boid as a small cyan square from the BIOS texture.
pub fn draw() void {
    vircon.selectTexture(-1);
    vircon.selectRegion(256);
    vircon.setMultiplyColor(vircon.color_cyan);
    vircon.setDrawingScale(3.0, 3.0);

    var index: i32 = 0;
    while (index < boidCount().*) : (index += 1) {
        const boid = &boids[@intCast(index)];
        vircon.setDrawingPoint(@intFromFloat(boid.x), @intFromFloat(boid.y));
        vircon.drawRegionZoomed();
    }
    vircon.setDrawingScale(1.0, 1.0);
    vircon.setMultiplyColor(vircon.color_white);
}
