//! Small Rust-facing Vircon32 API used by the boids example.
//!
//! The imports are narrow hardware operations. Text and decimal rendering are
//! ordinary Rust code, not compiler intrinsics.

pub const FRAMES_PER_SECOND: i32 = 60;
pub const COLOR_BLACK: i32 = 0xff00_0000u32 as i32;
pub const COLOR_WHITE: i32 = 0xffff_ffffu32 as i32;
pub const COLOR_CYAN: i32 = 0xffff_ff00u32 as i32;

#[link(wasm_import_module = "env")]
unsafe extern "C" {
    #[link_name = "vircon_set_background_color"]
    fn set_background_color(color: i32);
    #[link_name = "vircon_end_frame"]
    fn wait_for_frame();
    #[link_name = "vircon_gpu_get_selected_texture"]
    fn get_selected_texture() -> i32;
    #[link_name = "vircon_gpu_select_texture"]
    fn gpu_select_texture(texture: i32);
    #[link_name = "vircon_gpu_select_region"]
    fn gpu_select_region(region: i32);
    #[link_name = "vircon_gpu_set_drawing_point"]
    fn gpu_set_drawing_point(x: i32, y: i32);
    #[link_name = "vircon_gpu_set_multiply_color"]
    fn gpu_set_multiply_color(color: i32);
    #[link_name = "vircon_gpu_set_drawing_scale"]
    fn gpu_set_drawing_scale(x: f32, y: f32);
    #[link_name = "vircon_gpu_draw_region"]
    fn gpu_draw_region();
    #[link_name = "vircon_gpu_draw_region_zoomed"]
    fn gpu_draw_region_zoomed();
    #[link_name = "vircon_input_select_gamepad"]
    fn input_select_gamepad(gamepad: i32);
    #[link_name = "vircon_input_gamepad_up"]
    fn input_gamepad_up() -> i32;
    #[link_name = "vircon_input_gamepad_down"]
    fn input_gamepad_down() -> i32;
    #[link_name = "vircon_timer_get_frame_counter"]
    fn timer_get_frame_counter() -> i32;
}

/// Clears the next displayed frame to a packed Vircon ABGR colour.
pub fn clear_screen(color: i32) { unsafe { set_background_color(color); } }
/// Waits for the next Vircon32 video frame.
pub fn end_frame() { unsafe { wait_for_frame(); } }
/// Selects an application texture, or -1 for the BIOS texture.
pub fn select_texture(texture: i32) { unsafe { gpu_select_texture(texture); } }
/// Selects a region in the active texture.
pub fn select_region(region: i32) { unsafe { gpu_select_region(region); } }
/// Sets the coordinate used by the next GPU drawing command.
pub fn set_drawing_point(x: i32, y: i32) { unsafe { gpu_set_drawing_point(x, y); } }
/// Sets the drawing multiply colour.
pub fn set_multiply_color(color: i32) { unsafe { gpu_set_multiply_color(color); } }
/// Sets the typed floating-point scale used by zoomed drawing.
pub fn set_drawing_scale(x: f32, y: f32) { unsafe { gpu_set_drawing_scale(x, y); } }
/// Draws the selected texture region.
pub fn draw_region() { unsafe { gpu_draw_region(); } }
/// Draws the selected texture region with the selected scale.
pub fn draw_region_zoomed() { unsafe { gpu_draw_region_zoomed(); } }
/// Chooses the gamepad used by subsequent input reads.
pub fn select_gamepad(gamepad: i32) { unsafe { input_select_gamepad(gamepad); } }
/// Returns whether the selected gamepad's up direction is pressed.
pub fn gamepad_up() -> bool { unsafe { input_gamepad_up() > 0 } }
/// Returns whether the selected gamepad's down direction is pressed.
pub fn gamepad_down() -> bool { unsafe { input_gamepad_down() > 0 } }
/// Returns the current video-frame counter.
pub fn frame_counter() -> i32 { unsafe { timer_get_frame_counter() } }

/// Draws a byte string as CP-1252 BIOS-font glyphs.
pub fn print_at(initial_x: i32, initial_y: i32, text: &[u8]) {
    let previous_texture = unsafe { get_selected_texture() };
    let mut x = initial_x;
    let mut y = initial_y;
    select_texture(-1);
    for &glyph in text {
        select_region(glyph as i32);
        set_drawing_point(x, y);
        draw_region();
        x += 10;
        if glyph == b'\n' {
            x = initial_x;
            y += 20;
        }
    }
    select_texture(previous_texture);
}

/// Recursively draws decimal digits and returns the next x coordinate.
fn print_digits(x: i32, y: i32, value: u32) -> i32 {
    let quotient = value / 10;
    let next_x = if quotient != 0 { print_digits(x, y, quotient) } else { x };
    let digit = [b'0' + (value - quotient * 10) as u8];
    print_at(next_x, y, &digit);
    next_x + 10
}

/// Draws an unsigned decimal value without requiring formatting support.
pub fn print_uint_at(x: i32, y: i32, value: u32) { let _ = print_digits(x, y, value); }

/// Draws a floating-point value with two decimal places without formatting.
pub fn print_fixed_2_at(initial_x: i32, y: i32, value: f32) {
    let mut x = initial_x;
    let mut hundredths = (value * 100.0) as i32;
    if hundredths < 0 {
        print_at(x, y, b"-");
        x += 10;
        hundredths = -hundredths;
    }
    let magnitude = hundredths as u32;
    print_uint_at(x, y, magnitude / 100);
    print_at(x + 20, y, b".");
    let fraction = [b'0' + ((magnitude / 10) % 10) as u8, b'0' + (magnitude % 10) as u8];
    print_at(x + 30, y, &fraction);
}
