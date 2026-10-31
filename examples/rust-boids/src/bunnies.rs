//! Deterministic moving BIOS glyphs for the BunnyMark workload.
use crate::vircon as v;
#[derive(Clone, Copy)]
struct Bunny {
    x: f32,
    y: f32,
    dx: f32,
    dy: f32,
}
static mut BUNNIES: [Bunny; 9999] = [Bunny {
    x: 0.0,
    y: 0.0,
    dx: 0.0,
    dy: 0.0,
}; 9999];
static mut ACTIVE: i32 = 0;
static mut INITIALIZED: i32 = 0;
// Return a pointer to one element of the fixed array.
fn at(i: i32) -> *mut Bunny {
    unsafe {
        core::ptr::addr_of_mut!(BUNNIES)
            .cast::<Bunny>()
            .add(i as usize)
    }
}
/// Initialize one deterministic sprite.
fn initialize(i: i32) {
    unsafe {
        *at(i) = Bunny {
            x: (10 + (i * 13) % 600) as f32,
            y: (20 + (i * 17) % 300) as f32,
            dx: ((i & 7) - 3) as f32 * 0.5,
            dy: ((i & 3) - 2) as f32 * 0.5,
        }
    }
}
/// Reset the workload.
pub fn reset() {
    for i in 0..500 {
        initialize(i)
    }
    unsafe {
        ACTIVE = 100;
        INITIALIZED = 500
    }
}
/// Select the active workload size.
pub fn set_count(n: i32) {
    let n = n.clamp(0, 9999);
    unsafe {
        while INITIALIZED < n {
            initialize(INITIALIZED);
            INITIALIZED += 1
        }
        ACTIVE = n
    }
}
/// Return the active workload size.
pub fn count() -> i32 {
    unsafe { ACTIVE }
}
/// Advance and bounce the sprites.
pub fn update() {
    for i in 0..count() {
        unsafe {
            let b = &mut *at(i);
            b.x += b.dx;
            b.y += b.dy;
            if b.x < 0.0 || b.x > 629.0 {
                b.dx = -b.dx
            }
            if b.y < 0.0 || b.y > 339.0 {
                b.dy = -b.dy
            }
        }
    }
}
/// Draw the BIOS glyph sprites.
pub fn draw() {
    v::select_texture(-1);
    v::select_region(b'b' as i32);
    v::set_multiply_color(0xff808080u32 as i32);
    for i in 0..count() {
        unsafe {
            let b = &*at(i);
            v::set_drawing_point(b.x as i32, b.y as i32);
            v::draw_region()
        }
    }
    v::set_multiply_color(v::COLOR_WHITE)
}
