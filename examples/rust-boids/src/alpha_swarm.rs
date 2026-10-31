//! Rotating translucent BIOS glyphs for GPU load measurement.
use crate::vircon as v;
#[derive(Clone, Copy)]
struct Particle {
    x: i32,
    y: i32,
    dx: i32,
    dy: i32,
    angle: f32,
}
static mut PARTICLES: [Particle; 96] = [Particle {
    x: 0,
    y: 0,
    dx: 0,
    dy: 0,
    angle: 0.0,
}; 96];
static mut ACTIVE: i32 = 4;
// Return a pointer to one element of the fixed array.
fn at(i: i32) -> *mut Particle {
    unsafe {
        core::ptr::addr_of_mut!(PARTICLES)
            .cast::<Particle>()
            .add(i as usize)
    }
}
// Align the glyph with its movement direction.
fn angle(p: &Particle) -> f32 {
    v::atan2(p.dy as f32, p.dx as f32) - 3.1415926 / 2.0
}
/// Reset particles and configure a centered BIOS glyph.
pub fn reset() {
    unsafe { ACTIVE = 4 }
    for i in 0..96 {
        unsafe {
            let p = &mut *at(i);
            *p = Particle {
                x: 114 + (i * 83) % 413,
                y: 194 + (i * 29) % 53,
                dx: if i & 1 != 0 { 3 } else { -3 },
                dy: if i & 2 != 0 { 2 } else { -2 },
                angle: 0.0,
            };
            p.angle = angle(p)
        }
    }
    v::select_texture(-1);
    v::select_region(b'v' as i32);
    let (x, y) = v::region_minimum();
    v::select_region(257);
    v::define_region(x, y)
}
/// Set the active number of layers.
pub fn set_count(n: i32) {
    unsafe { ACTIVE = n.clamp(0, 96) }
}
/// Return the active number of layers.
pub fn count() -> i32 {
    unsafe { ACTIVE }
}
/// Move layers, bouncing and rotating at the edges.
pub fn update() {
    for i in 0..count() {
        unsafe {
            let p = &mut *at(i);
            p.x += p.dx;
            p.y += p.dy;
            let mut bounced = false;
            if p.x < 114 || p.x > 526 {
                p.x = p.x.clamp(114, 526);
                p.dx = -p.dx;
                bounced = true
            }
            if p.y < 194 || p.y > 246 {
                p.y = p.y.clamp(194, 246);
                p.dy = -p.dy;
                bounced = true
            }
            if bounced {
                p.angle = angle(p)
            }
        }
    }
}
/// Draw layers and restore shared GPU state.
pub fn draw() {
    let colors = [0x70ff4030u32, 0x7040d060, 0x703060ff, 0x70e0c040];
    v::select_texture(-1);
    v::select_region(257);
    v::set_blending(0x20);
    v::set_drawing_scale(10.0, 10.0);
    for i in 0..count() {
        unsafe {
            let p = &*at(i);
            v::set_multiply_color(colors[(i & 3) as usize] as i32);
            v::set_angle(p.angle);
            v::set_drawing_point(p.x, p.y);
            v::draw_rotozoom()
        }
    }
    v::set_drawing_scale(1.0, 1.0);
    v::set_angle(0.0);
    v::set_multiply_color(v::COLOR_WHITE)
}
