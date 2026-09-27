//! Flocking state, simulation and rendering for the Rust boids benchmark.

use crate::vircon;

const MAX_BOID_COUNT: usize = 150;
const INITIAL_BOID_COUNT: i32 = 50;
const BOID_COUNT_STEP: i32 = 10;
const WIDTH: f32 = 640.0;
const HEIGHT: f32 = 360.0;
const VIEW_RANGE: f32 = 60.0;
const AVOID_RANGE: f32 = 20.0;
const CENTERING_FACTOR: f32 = 0.005;
const MATCHING_FACTOR: f32 = 0.05;
const AVOID_FACTOR: f32 = 0.05;
const MAX_SPEED: f32 = 4.0;
const MIN_SPEED: f32 = 2.0;
const MARGIN: f32 = 50.0;
const TURN_FACTOR: f32 = 0.2;

#[derive(Clone, Copy)]
struct Boid {
    x: f32,
    y: f32,
    vx: f32,
    vy: f32,
}

const EMPTY_BOID: Boid = Boid { x: 0.0, y: 0.0, vx: 0.0, vy: 0.0 };
static mut FLOCK: [Boid; MAX_BOID_COUNT] = [EMPTY_BOID; MAX_BOID_COUNT];
static mut ACTIVE_BOID_COUNT: i32 = INITIAL_BOID_COUNT;
static mut RANDOM_STATE: u32 = 1;

/// Returns a raw pointer so the fixed flock is plainly ordinary Wasm memory.
fn boid_pointer(index: usize) -> *mut Boid {
    unsafe { core::ptr::addr_of_mut!(FLOCK).cast::<Boid>().add(index) }
}

/// Returns the number of active flock members.
pub fn count() -> i32 {
    unsafe { ACTIVE_BOID_COUNT }
}

/// Advances the benchmark's deterministic local pseudo-random generator.
fn random_u32() -> u32 {
    unsafe {
        RANDOM_STATE = RANDOM_STATE.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
        RANDOM_STATE
    }
}

/// Produces a deterministic float in the requested interval.
fn random_float(minimum: f32, maximum: f32) -> f32 {
    let fraction = (random_u32() & 0xffff) as f32 / 65535.0;
    minimum + (maximum - minimum) * fraction
}

/// Initializes one flock member with deterministic position and velocity.
fn initialize_boid(index: usize) {
    unsafe {
        let boid = &mut *boid_pointer(index);
        boid.x = random_float(100.0, WIDTH - 100.0);
        boid.y = random_float(100.0, HEIGHT - 100.0);
        boid.vx = random_float(-2.0, 2.0);
        boid.vy = random_float(-2.0, 2.0);
    }
}

/// Initializes the visible starting workload.
pub fn initialize() {
    let mut index = 0;
    while index < count() {
        initialize_boid(index as usize);
        index += 1;
    }
}

/// Adds one batch without exceeding the fixed flock capacity.
pub fn increase() {
    let target = (count() + BOID_COUNT_STEP).min(MAX_BOID_COUNT as i32);
    while count() < target {
        initialize_boid(count() as usize);
        unsafe { ACTIVE_BOID_COUNT += 1; }
    }
}

/// Removes one batch while retaining a visible flock.
pub fn decrease() {
    unsafe {
        if ACTIVE_BOID_COUNT > BOID_COUNT_STEP {
            ACTIVE_BOID_COUNT -= BOID_COUNT_STEP;
        } else {
            ACTIVE_BOID_COUNT = 10;
        }
    }
}

/// Runs one deliberately simple O(N²) flocking simulation step.
pub fn update() {
    let mut i = 0;
    while i < count() {
        let mut center_x = 0.0;
        let mut center_y = 0.0;
        let mut average_vx = 0.0;
        let mut average_vy = 0.0;
        let mut avoid_x = 0.0;
        let mut avoid_y = 0.0;
        let mut neighbors = 0;
        let mut j = 0;
        while j < count() {
            if i != j {
                unsafe {
                    let current = &*boid_pointer(i as usize);
                    let other = &*boid_pointer(j as usize);
                    let dx = current.x - other.x;
                    let dy = current.y - other.y;
                    let distance_squared = dx * dx + dy * dy;
                    if distance_squared < VIEW_RANGE * VIEW_RANGE {
                        center_x += other.x;
                        center_y += other.y;
                        average_vx += other.vx;
                        average_vy += other.vy;
                        neighbors += 1;
                    }
                    if distance_squared < AVOID_RANGE * AVOID_RANGE {
                        avoid_x += dx;
                        avoid_y += dy;
                    }
                }
            }
            j += 1;
        }

        unsafe {
            let current = &mut *boid_pointer(i as usize);
            if neighbors > 0 {
                let neighbor_count = neighbors as f32;
                center_x /= neighbor_count;
                center_y /= neighbor_count;
                average_vx /= neighbor_count;
                average_vy /= neighbor_count;
                current.vx += (center_x - current.x) * CENTERING_FACTOR;
                current.vy += (center_y - current.y) * CENTERING_FACTOR;
                current.vx += (average_vx - current.vx) * MATCHING_FACTOR;
                current.vy += (average_vy - current.vy) * MATCHING_FACTOR;
            }
            current.vx += avoid_x * AVOID_FACTOR;
            current.vy += avoid_y * AVOID_FACTOR;
            if current.x < MARGIN { current.vx += TURN_FACTOR; }
            if current.x > WIDTH - MARGIN { current.vx -= TURN_FACTOR; }
            if current.y < MARGIN { current.vy += TURN_FACTOR; }
            if current.y > HEIGHT - MARGIN { current.vy -= TURN_FACTOR; }
            let speed_squared = current.vx * current.vx + current.vy * current.vy;
            if speed_squared > MAX_SPEED * MAX_SPEED {
                current.vx *= 0.9;
                current.vy *= 0.9;
            }
            if speed_squared < MIN_SPEED * MIN_SPEED {
                current.vx *= 1.1;
                current.vy *= 1.1;
            }
            current.x += current.vx;
            current.y += current.vy;
        }
        i += 1;
    }
}

/// Draws every active flock member as a small cyan BIOS-pixel square.
pub fn draw() {
    vircon::select_texture(-1);
    vircon::select_region(256);
    vircon::set_multiply_color(vircon::COLOR_CYAN);
    vircon::set_drawing_scale(3.0, 3.0);
    let mut index = 0;
    while index < count() {
        unsafe {
            let boid = &*boid_pointer(index as usize);
            vircon::set_drawing_point(boid.x as i32, boid.y as i32);
        }
        vircon::draw_region_zoomed();
        index += 1;
    }
    vircon::set_drawing_scale(1.0, 1.0);
    vircon::set_multiply_color(vircon::COLOR_WHITE);
}
