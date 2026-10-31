//! Shared controls, timing, and deterministic six-case benchmark tour.
use crate::{alpha_swarm as alpha, boids, bunnies, vircon as v};
const FPS_HUNDREDTHS: u32 = v::FRAMES_PER_SECOND as u32 * 100;
#[derive(Clone, Copy)]
struct Case {
    mode: i32,
    count: i32,
}
const CASES: [Case; 6] = [
    Case { mode: 0, count: 10 },
    Case { mode: 0, count: 50 },
    Case {
        mode: 1,
        count: 500,
    },
    Case {
        mode: 1,
        count: 2500,
    },
    Case { mode: 2, count: 4 },
    Case { mode: 2, count: 64 },
];
#[derive(Default)]
struct Repeat {
    direction: i32,
    start: u32,
    emitted: u32,
}
struct Fps {
    start: u32,
    completed: u32,
    hundredths: u32,
}
// Read the wrapping display-frame counter.
fn now() -> u32 {
    v::frame_counter() as u32
}
// Start a fresh FPS measurement window.
fn fps() -> Fps {
    Fps {
        start: now(),
        completed: 0,
        hundredths: FPS_HUNDREDTHS,
    }
}
// Reset the selected workload.
fn reset(m: i32) {
    match m {
        0 => boids::initialize(),
        1 => bunnies::reset(),
        _ => alpha::reset(),
    }
}
// Return the selected workload count.
fn count(m: i32) -> i32 {
    match m {
        0 => boids::count(),
        1 => bunnies::count(),
        _ => alpha::count(),
    }
}
// Select and clamp the workload count.
fn set_count(m: i32, n: i32) {
    match m {
        0 => boids::set_count(n),
        1 => bunnies::set_count(n),
        _ => alpha::set_count(n),
    }
}
// Convert a held direction into frame-timed count changes.
fn repeat(r: &mut Repeat, d: i32, t: u32) -> i32 {
    if d == 0 {
        r.direction = 0;
        return 0;
    }
    if d != r.direction {
        *r = Repeat {
            direction: d,
            start: t,
            emitted: 0,
        };
        return d;
    }
    let elapsed = t.wrapping_sub(r.start);
    let due = if elapsed < 18 {
        0
    } else {
        1 + (elapsed - 18) / 6
    };
    let steps = due.wrapping_sub(r.emitted).min(100);
    r.emitted = due;
    d * steps as i32
}
// Draw the common benchmark status overlay.
fn hud(mode: i32, cpu: i32, gpu: i32, fps: u32, tour: i32) {
    v::select_texture(-1);
    v::select_region(256);
    v::set_blending(0x20);
    v::set_multiply_color(0xb0000000u32 as i32);
    v::set_drawing_scale(380.0, 70.0);
    v::set_drawing_point(5, 5);
    v::draw_region_zoomed();
    v::set_drawing_scale(1.0, 1.0);
    v::set_multiply_color(v::COLOR_WHITE);
    let name: &[u8] = match mode {
        0 => b"Boids",
        1 => b"BunnyMark",
        _ => b"Alpha Swarm",
    };
    v::print_at(10, 10, name);
    v::print_at(230, 10, b"Count:");
    v::print_uint_at(295, 10, count(mode) as u32);
    v::print_at(10, 30, b"FPS:");
    v::print_fixed_2_at(55, 30, fps as f32 / 100.0);
    v::print_at(120, 30, b"GPU%:");
    v::print_fixed_2_at(175, 30, gpu as f32 / 100.0);
    v::print_at(245, 30, b"CPU%:");
    v::print_fixed_2_at(300, 30, cpu as f32 / 100.0);
    if tour != 0 {
        v::print_at(10, 50, b"Benchmark tour:");
        v::print_uint_at(170, 50, tour as u32);
        v::print_at(180, 50, b"/6")
    } else {
        v::print_at(10, 50, b"L/R mode  DPAD +/-  START tour")
    }
}
// Update, draw, measure, and wait for one benchmark iteration.
fn frame(mode: i32, cpu: &mut i32, gpu: &mut i32, fps: &mut Fps, tour: i32) {
    let start = v::frame_counter();
    let cycles = v::cycle_counter();
    match mode {
        0 => boids::update(),
        1 => bunnies::update(),
        _ => alpha::update(),
    }
    v::clear_screen(v::COLOR_BLACK);
    if mode == 2 {
        hud(mode, *cpu, *gpu, fps.hundredths, tour);
        alpha::draw()
    } else {
        if mode == 0 {
            boids::draw()
        } else {
            bunnies::draw()
        }
        hud(mode, *cpu, *gpu, fps.hundredths, tour)
    }
    *cpu = (v::frame_counter()
        .wrapping_sub(start)
        .wrapping_mul(v::CYCLES_PER_FRAME)
        .wrapping_add(v::cycle_counter().wrapping_sub(cycles)))
        / (v::CYCLES_PER_FRAME / 10000);
    let remaining = v::gpu_remaining_pixels().clamp(0, 2073600);
    let used = 2073600 - remaining;
    *gpu = (used / 20736) * 100 + (used % 20736) * 100 / 20736;
    v::end_frame();
    fps.completed += 1;
    let t = now();
    let elapsed = t.wrapping_sub(fps.start);
    if elapsed >= v::FRAMES_PER_SECOND as u32 {
        fps.hundredths = fps.completed * FPS_HUNDREDTHS / elapsed;
        fps.completed = 0;
        fps.start = t
    }
}
// Run the fixed cases and publish simulator checkpoints.
fn tour() {
    let mut checksum = 0;
    for (i, c) in CASES.iter().enumerate() {
        reset(c.mode);
        set_count(c.mode, c.count);
        // Keep setup time and its partial display frame outside the timed case.
        v::end_frame();
        let (mut cpu, mut gpu) = (0, 0);
        let start = now();
        let mut rate = Fps {
            start,
            completed: 0,
            hundredths: FPS_HUNDREDTHS,
        };
        let (mut updates, mut cpus, mut gpus) = (0u32, 0u32, 0u32);
        loop {
            frame(c.mode, &mut cpu, &mut gpu, &mut rate, i as i32 + 1);
            updates += 1;
            cpus += cpu as u32;
            gpus += gpu as u32;
            if now().wrapping_sub(start) >= v::FRAMES_PER_SECOND as u32 {
                break;
            }
        }
        let n = count(c.mode);
        let encoded = (gpus / updates) | ((cpus / updates).min(262143) << 14);
        v::set_multiply_color(encoded as i32);
        v::set_drawing_scale(
            (updates * FPS_HUNDREDTHS / now().wrapping_sub(start)) as f32 / 100.0,
            1.0,
        );
        v::set_drawing_point(n % 1000, 1001 + i as i32 + 6 * (n / 1000));
        v::set_multiply_color(v::COLOR_WHITE);
        v::set_drawing_scale(1.0, 1.0);
        checksum += (i as i32 + 1) * (n + 1)
    }
    v::set_drawing_point(checksum % 997, 6)
}
/// Run the interactive benchmark controller.
pub fn run() -> ! {
    let (mut mode, mut cpu, mut gpu) = (0, 0, 0);
    v::select_gamepad(0);
    reset(mode);
    let mut rate = fps();
    let (mut pl, mut pr, mut ps) = (false, false, false);
    let (mut vertical, mut horizontal) = (Repeat::default(), Repeat::default());
    loop {
        let start = v::gamepad_start();
        let l = v::gamepad_l();
        let r = v::gamepad_r();
        if start && !ps {
            tour();
            reset(mode);
            cpu = 0;
            gpu = 0;
            rate = fps();
            vertical = Repeat::default();
            horizontal = Repeat::default();
            ps = true;
            pl = v::gamepad_l();
            pr = v::gamepad_r();
            continue;
        }
        if (l && !pl) || (r && !pr) {
            mode = (mode + if r && !pr { 1 } else { 2 }) % 3;
            reset(mode);
            cpu = 0;
            gpu = 0;
            rate = fps();
            vertical = Repeat::default();
            horizontal = Repeat::default()
        }
        let up = if v::gamepad_up() {
            1
        } else if v::gamepad_down() {
            -1
        } else {
            0
        };
        let side = if v::gamepad_right() {
            1
        } else if v::gamepad_left() {
            -1
        } else {
            0
        };
        let step = match mode {
            0 => 10,
            1 => 100,
            _ => 5,
        };
        let delta = repeat(&mut vertical, up, now()) * step + repeat(&mut horizontal, side, now());
        if delta != 0 {
            set_count(mode, count(mode) + delta)
        }
        pl = l;
        pr = r;
        ps = start;
        frame(mode, &mut cpu, &mut gpu, &mut rate, 0)
    }
}
