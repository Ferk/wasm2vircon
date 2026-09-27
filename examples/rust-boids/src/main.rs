//! Cartridge entry and frame presentation for the freestanding Rust boids demo.
#![no_std]
#![no_main]

mod boids;
mod vircon;

use core::panic::PanicInfo;

/// The benchmark has no recoverable failures; aborting avoids a Rust runtime.
#[panic_handler]
fn panic(_: &PanicInfo) -> ! {
    loop {}
}

/// Explicit C ABI export selected as the cartridge entry by wasm2vircon.
#[unsafe(no_mangle)]
pub extern "C" fn vircon_main() -> ! {
    let mut previous_up_pressed = false;
    let mut previous_down_pressed = false;
    boids::initialize();

    loop {
        vircon::select_gamepad(0);
        let up_pressed = vircon::gamepad_up();
        let down_pressed = vircon::gamepad_down();
        if up_pressed && !previous_up_pressed {
            boids::increase();
        } else if down_pressed && !previous_down_pressed {
            boids::decrease();
        }
        previous_up_pressed = up_pressed;
        previous_down_pressed = down_pressed;

        let update_start_frame = vircon::frame_counter();
        boids::update();
        let update_frames = vircon::frame_counter().wrapping_sub(update_start_frame).wrapping_add(1);
        let frames_per_update = vircon::FRAMES_PER_SECOND as f32 / update_frames as f32;

        vircon::clear_screen(vircon::COLOR_BLACK);
        boids::draw();
        vircon::print_at(10, 10, b"Boids:");
        vircon::print_uint_at(80, 10, boids::count() as u32);
        vircon::print_at(120, 10, b"(up/down +/-10)");
        vircon::print_at(10, 30, b"Update frames:");
        vircon::print_uint_at(160, 30, update_frames as u32);
        vircon::print_at(10, 50, b"FPS:");
        vircon::print_fixed_2_at(60, 50, frames_per_update);
        vircon::end_frame();
    }
}
