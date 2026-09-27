//! Small Zig-facing Vircon32 API used by the boids example.
//!
//! These extern declarations are the narrow hardware-like imports understood
//! by wasm2vircon. The text and decimal helpers below are ordinary Zig code;
//! no high-level text operation is a compiler intrinsic.

pub const frames_per_second: i32 = 60;
pub const color_black: i32 = @bitCast(@as(u32, 0xff000000));
pub const color_white: i32 = @bitCast(@as(u32, 0xffffffff));
pub const color_cyan: i32 = @bitCast(@as(u32, 0xffffff00));

extern "env" fn vircon_set_background_color(color: i32) void;
extern "env" fn vircon_end_frame() void;
extern "env" fn vircon_gpu_get_selected_texture() i32;
extern "env" fn vircon_gpu_select_texture(texture: i32) void;
extern "env" fn vircon_gpu_select_region(region: i32) void;
extern "env" fn vircon_gpu_set_drawing_point(x: i32, y: i32) void;
extern "env" fn vircon_gpu_set_multiply_color(color: i32) void;
extern "env" fn vircon_gpu_set_drawing_scale(x: f32, y: f32) void;
extern "env" fn vircon_gpu_draw_region() void;
extern "env" fn vircon_gpu_draw_region_zoomed() void;
extern "env" fn vircon_input_select_gamepad(gamepad: i32) void;
extern "env" fn vircon_input_gamepad_up() i32;
extern "env" fn vircon_input_gamepad_down() i32;
extern "env" fn vircon_timer_get_frame_counter() i32;

/// Clears the next displayed frame to a packed Vircon ABGR colour.
pub fn clearScreen(color: i32) void {
    vircon_set_background_color(color);
}

/// Waits for the next Vircon32 frame.
pub fn endFrame() void {
    vircon_end_frame();
}

/// Selects an application texture, or -1 for the BIOS texture.
pub fn selectTexture(texture: i32) void {
    vircon_gpu_select_texture(texture);
}

/// Selects a texture region in the selected texture.
pub fn selectRegion(region: i32) void {
    vircon_gpu_select_region(region);
}

/// Sets the pixel coordinate used by the next drawing command.
pub fn setDrawingPoint(x: i32, y: i32) void {
    vircon_gpu_set_drawing_point(x, y);
}

/// Sets the colour multiplied with a texture region while drawing it.
pub fn setMultiplyColor(color: i32) void {
    vircon_gpu_set_multiply_color(color);
}

/// Sets the typed floating-point scale used by drawRegionZoomed.
pub fn setDrawingScale(x: f32, y: f32) void {
    vircon_gpu_set_drawing_scale(x, y);
}

/// Draws the selected region without changing its scale.
pub fn drawRegion() void {
    vircon_gpu_draw_region();
}

/// Draws the selected region using the selected drawing scale.
pub fn drawRegionZoomed() void {
    vircon_gpu_draw_region_zoomed();
}

/// Chooses the gamepad read by the directional helpers.
pub fn selectGamepad(gamepad: i32) void {
    vircon_input_select_gamepad(gamepad);
}

/// Reads the current up-button state. Positive values mean pressed.
pub fn gamepadUp() bool {
    return vircon_input_gamepad_up() > 0;
}

/// Reads the current down-button state. Positive values mean pressed.
pub fn gamepadDown() bool {
    return vircon_input_gamepad_down() > 0;
}

/// Returns the current video-frame counter.
pub fn frameCounter() i32 {
    return vircon_timer_get_frame_counter();
}

/// Draws a NUL-terminated CP-1252 byte string through the BIOS font.
pub fn printAt(initial_x: i32, initial_y: i32, text: [*:0]const u8) void {
    const previous_texture = vircon_gpu_get_selected_texture();
    var x = initial_x;
    var y = initial_y;
    var cursor = text;

    selectTexture(-1);
    while (cursor[0] != 0) : (cursor += 1) {
        const glyph: i32 = @intCast(cursor[0]);
        selectRegion(glyph);
        setDrawingPoint(x, y);
        drawRegion();
        x += 10;
        if (glyph == '\n') {
            x = initial_x;
            y += 20;
        }
    }
    selectTexture(previous_texture);
}

var digit_text: [1:0]u8 = .{'0'};

/// Prints an unsigned decimal value and returns the x coordinate after it.
fn printDigits(x: i32, y: i32, value: u32) i32 {
    const quotient = value / 10;
    var result_x = x;
    if (quotient != 0) result_x = printDigits(result_x, y, quotient);
    digit_text[0] = @intCast('0' + value - quotient * 10);
    printAt(result_x, y, &digit_text);
    return result_x + 10;
}

/// Prints a compact unsigned decimal value with the BIOS font.
pub fn printUIntAt(x: i32, y: i32, value: u32) void {
    _ = printDigits(x, y, value);
}

var fraction_text: [2:0]u8 = .{ '0', '0' };

/// Prints a value with two decimal places; this is intentionally not printf.
pub fn printFixed2At(initial_x: i32, y: i32, value: f32) void {
    var x = initial_x;
    var hundredths: i32 = @intFromFloat(value * 100.0);
    if (hundredths < 0) {
        printAt(x, y, "-");
        x += 10;
        hundredths = -hundredths;
    }

    const magnitude: u32 = @intCast(hundredths);
    printUIntAt(x, y, magnitude / 100);
    printAt(x + 20, y, ".");
    fraction_text[0] = @intCast('0' + (magnitude / 10) % 10);
    fraction_text[1] = @intCast('0' + magnitude % 10);
    printAt(x + 30, y, &fraction_text);
}
