//! Public Vircon32 API for Zig programs compiled through wasm2vircon.
//!
//! This is the Zig counterpart of the project's public `vircon.h`.
//! Public functions below are normal Zig wrappers and helpers.
//! The private `platform` declarations at the bottom are the only Wasm imports.
//!
//! ## Zig → VirconWasm checklist
//!
//! - Keep a root `pub fn main() void {}` for Zig's entry validation, then
//!   export the cartridge entry explicitly, for example
//!   `pub export fn vircon_main() callconv(.c) void`. Set the same name in
//!   the build driver and in `wasm2vircon --entry`.
//!
//! - Build for `wasm32-freestanding` with `-fno-entry`. The module must define
//!   one normal Wasm memory; do not import memory or use memory64.
//!
//! - Ordinary Zig state is fine when it lives in an array or struct, including
//!   `var` storage whose address you use. Zig places that kind of state in the
//!   program's normal data memory, where wasm2vircon can handle it. Avoid
//!   depending on compiler-generated hidden global variables for mutable state;
//!   the one internal stack-management global Zig normally emits
//!   (`__stack_pointer`) is supported, but arbitrary Wasm globals are not.
//!
//! - This is not a hosted `std` environment. Small compile-time and pure data
//!   helpers are fine, but avoid `std.io`, files, processes, threads,
//!   networking, general allocators, and anything that brings in WASI. Inspect
//!   unfamiliar output with `wasm2vircon --report-profile`.
//!
//! - Prefer direct calls to named functions. Function pointers, callbacks,
//!   virtual/interface dispatch, and runtime-selected functions may compile to
//!   indirect calls and are not supported. Also avoid `f64`, SIMD/vector
//!   types, atomics/threading, exceptions, and broad `i64` use. Basic i64
//!   storage and arithmetic work in selected cases, but passing i64 values to
//!   functions, i64 division/remainder, and 8/16-bit i64 loads or stores do
//!   not yet have general support.
//!
//! Public names use Zig `lowerCamelCase`.
//! Coordinates, resource IDs, and channel IDs are i32.
//! Colours are packed Vircon ABGR words held as i32 bit patterns.
//!
//! This module deliberately does not reproduce C's `malloc` or string/libc
//! spellings: use explicit fixed storage and Zig's own byte-slice operations
//! that fit the supported Wasm profile.

// ----
// Constants

/// Visible Vircon32 video width in pixels.
pub const screen_width: i32 = 640;
/// Visible Vircon32 video height in pixels.
pub const screen_height: i32 = 360;
/// Vircon32's fixed display rate.
pub const frames_per_second: i32 = 60;
/// Duration of one display frame in seconds.
pub const frame_time: f32 = 1.0 / @as(f32, @floatFromInt(frames_per_second));

/// Packed opaque black in Vircon ABGR byte order.
pub const color_black: i32 = @bitCast(@as(u32, 0xff000000));
/// Packed opaque white in Vircon ABGR byte order.
pub const color_white: i32 = @bitCast(@as(u32, 0xffffffff));
/// Packed opaque gray in Vircon ABGR byte order.
pub const color_gray: i32 = @bitCast(@as(u32, 0xff808080));
/// Packed opaque dark gray in Vircon ABGR byte order.
pub const color_darkgray: i32 = @bitCast(@as(u32, 0xff404040));
/// Packed opaque light gray in Vircon ABGR byte order.
pub const color_lightgray: i32 = @bitCast(@as(u32, 0xffc0c0c0));
/// Packed opaque red in Vircon ABGR byte order.
pub const color_red: i32 = @bitCast(@as(u32, 0xff0000ff));
/// Packed opaque green in Vircon ABGR byte order.
pub const color_green: i32 = @bitCast(@as(u32, 0xff00ff00));
/// Packed opaque blue in Vircon ABGR byte order.
pub const color_blue: i32 = @bitCast(@as(u32, 0xffff0000));
/// Packed opaque yellow in Vircon ABGR byte order.
pub const color_yellow: i32 = @bitCast(@as(u32, 0xff00ffff));
/// Packed opaque magenta in Vircon ABGR byte order.
pub const color_magenta: i32 = @bitCast(@as(u32, 0xffff00ff));
/// Packed opaque cyan in Vircon ABGR byte order.
pub const color_cyan: i32 = @bitCast(@as(u32, 0xffffff00));
/// Packed opaque orange in Vircon ABGR byte order.
pub const color_orange: i32 = @bitCast(@as(u32, 0xff0080ff));
/// Packed opaque brown in Vircon ABGR byte order.
pub const color_brown: i32 = @bitCast(@as(u32, 0xff204080));

/// State value returned by `getChannelState` for an idle channel.
pub const channel_stopped: i32 = 0x40;
/// State value returned by `getChannelState` for a paused channel.
pub const channel_paused: i32 = 0x41;
/// State value returned by `getChannelState` for a playing channel.
pub const channel_playing: i32 = 0x42;
/// Vircon32 provides sixteen selectable sound channels.
pub const sound_channels: i32 = 16;
/// Standard Vircon memory-card signature length, in 32-bit words.
pub const game_signature_words: usize = 20;

// ----
// Types

/// Two signed pixel coordinates.
pub const Point = struct { x: i32, y: i32 };
/// Two typed f32 values such as a drawing scale.
pub const Scale = struct { x: f32, y: f32 };
/// Calendar fields decoded from `getDate`.
pub const DateInfo = struct { year: i32, month: i32, day: i32 };
/// Clock fields decoded from `getTime`.
pub const TimeInfo = struct { hours: i32, minutes: i32, seconds: i32 };
/// Parameters for a regular matrix of texture regions.
pub const RegionMatrix = struct {
    first_id: i32,
    first_min_x: i32,
    first_min_y: i32,
    first_max_x: i32,
    first_max_y: i32,
    first_hotspot_x: i32,
    first_hotspot_y: i32,
    elements_x: i32,
    elements_y: i32,
    gap: i32,
};
/// The normal twenty-word memory-card signature type.
pub const GameSignature = [game_signature_words]i32;

// ----
// Public functions: Color

/// Packs an opaque grayscale colour; the low byte is used for every RGB lane.
pub fn makeGray(brightness: i32) i32 {
    return makeColorRgba(brightness, brightness, brightness, 255);
}
/// Packs opaque red, green, and blue components into a Vircon ABGR word.
pub fn makeColorRgb(red: i32, green: i32, blue: i32) i32 {
    return makeColorRgba(red, green, blue, 255);
}
/// Packs RGBA components into a Vircon ABGR word; each component uses its low byte.
pub fn makeColorRgba(red: i32, green: i32, blue: i32, alpha: i32) i32 {
    const r: u32 = @bitCast(red);
    const g: u32 = @bitCast(green);
    const b: u32 = @bitCast(blue);
    const a: u32 = @bitCast(alpha);
    return @bitCast((a & 255) << 24 | (b & 255) << 16 | (g & 255) << 8 | (r & 255));
}
/// Extracts red from a packed Vircon colour word.
pub fn colorRed(color: i32) i32 {
    return @intCast(@as(u32, @bitCast(color)) & 255);
}
/// Extracts green from a packed Vircon colour word.
pub fn colorGreen(color: i32) i32 {
    return @intCast((@as(u32, @bitCast(color)) >> 8) & 255);
}
/// Extracts blue from a packed Vircon colour word.
pub fn colorBlue(color: i32) i32 {
    return @intCast((@as(u32, @bitCast(color)) >> 16) & 255);
}
/// Extracts alpha from a packed Vircon colour word.
pub fn colorAlpha(color: i32) i32 {
    return @intCast((@as(u32, @bitCast(color)) >> 24) & 255);
}

// ----
// Public functions: Screen management

/// Clears the next displayed frame to a packed Vircon ABGR colour.
pub fn clearScreen(color: i32) void {
    platform.vircon_set_background_color(color);
}
/// Waits for the next Vircon32 frame.
pub fn endFrame() void {
    platform.vircon_end_frame();
}

// ----
// Public functions: Texture management

/// Selects an application texture, or -1 for the BIOS texture.
pub fn selectTexture(texture: i32) void {
    platform.vircon_gpu_select_texture(texture);
}
/// Returns the currently selected texture, including BIOS texture -1.
pub fn getSelectedTexture() i32 {
    return platform.vircon_gpu_get_selected_texture();
}
/// Selects a texture region in the selected texture.
pub fn selectRegion(region: i32) void {
    platform.vircon_gpu_select_region(region);
}
/// Returns the selected region for the current texture.
pub fn getSelectedRegion() i32 {
    return platform.vircon_gpu_get_selected_region();
}
/// Sets the selected region's inclusive minimum texture coordinate.
pub fn setRegionMinimum(x: i32, y: i32) void {
    platform.vircon_gpu_set_region_minimum(x, y);
}
/// Sets the selected region's inclusive maximum texture coordinate.
pub fn setRegionMaximum(x: i32, y: i32) void {
    platform.vircon_gpu_set_region_maximum(x, y);
}
/// Sets the selected region's hotspot in texture coordinates.
pub fn setRegionHotspot(x: i32, y: i32) void {
    platform.vircon_gpu_set_region_hotspot(x, y);
}
/// Sets the pixel coordinate used by the next drawing command.
pub fn setDrawingPoint(x: i32, y: i32) void {
    platform.vircon_gpu_set_drawing_point(x, y);
}
/// Returns the current drawing point.
pub fn getDrawingPoint() Point {
    return .{ .x = platform.vircon_gpu_get_drawing_point_x(), .y = platform.vircon_gpu_get_drawing_point_y() };
}
/// Sets the texture colour multiplier used while drawing.
pub fn setMultiplyColor(color: i32) void {
    platform.vircon_gpu_set_multiply_color(color);
}
/// Returns the texture colour multiplier used while drawing.
pub fn getMultiplyColor() i32 {
    return platform.vircon_gpu_get_multiply_color();
}
/// Sets the active GPU blending-mode word.
pub fn setBlendingMode(mode: i32) void {
    platform.vircon_gpu_set_active_blending(mode);
}
/// Returns the active GPU blending-mode word.
pub fn getBlendingMode() i32 {
    return platform.vircon_gpu_get_active_blending();
}
/// Sets the typed f32 drawing scale used by zoomed and rotozoomed draws.
pub fn setDrawingScale(x: f32, y: f32) void {
    platform.vircon_gpu_set_drawing_scale(x, y);
}
/// Sets scale from already-known IEEE-754 f32 bit patterns.
pub fn setDrawingScaleBits(x_bits: i32, y_bits: i32) void {
    platform.vircon_gpu_set_drawing_scale_bits(x_bits, y_bits);
}
/// Returns the current drawing scale.
pub fn getDrawingScale() Scale {
    return .{ .x = platform.vircon_gpu_get_drawing_scale_x(), .y = platform.vircon_gpu_get_drawing_scale_y() };
}
/// Sets the typed f32 drawing angle used by rotated and rotozoomed draws.
pub fn setDrawingAngle(angle: f32) void {
    platform.vircon_gpu_set_drawing_angle(angle);
}
/// Returns the current typed f32 drawing angle.
pub fn getDrawingAngle() f32 {
    return platform.vircon_gpu_get_drawing_angle();
}
/// Draws the selected region without scale or rotation.
pub fn drawRegion() void {
    platform.vircon_gpu_draw_region();
}
/// Draws the selected region using the active drawing scale.
pub fn drawRegionZoomed() void {
    platform.vircon_gpu_draw_region_zoomed();
}
/// Draws the selected region using the active drawing angle.
pub fn drawRegionRotated() void {
    platform.vircon_gpu_draw_region_rotated();
}
/// Draws the selected region using the active scale and angle.
pub fn drawRegionRotozoomed() void {
    platform.vircon_gpu_draw_region_rotozoomed();
}
/// Sets a point then draws the selected region without scale or rotation.
pub fn drawRegionAt(x: i32, y: i32) void {
    setDrawingPoint(x, y);
    drawRegion();
}
/// Sets a point then draws the selected region with the active scale.
pub fn drawRegionZoomedAt(x: i32, y: i32) void {
    setDrawingPoint(x, y);
    drawRegionZoomed();
}
/// Sets a point then draws the selected region with the active angle.
pub fn drawRegionRotatedAt(x: i32, y: i32) void {
    setDrawingPoint(x, y);
    drawRegionRotated();
}
/// Sets a point then draws the selected region with active scale and angle.
pub fn drawRegionRotozoomedAt(x: i32, y: i32) void {
    setDrawingPoint(x, y);
    drawRegionRotozoomed();
}
/// Defines the selected region with explicit bounds and hotspot.
pub fn defineRegion(min_x: i32, min_y: i32, max_x: i32, max_y: i32, hotspot_x: i32, hotspot_y: i32) void {
    setRegionMinimum(min_x, min_y);
    setRegionMaximum(max_x, max_y);
    setRegionHotspot(hotspot_x, hotspot_y);
}
/// Defines the selected region with a top-left hotspot.
pub fn defineRegionTopLeft(min_x: i32, min_y: i32, max_x: i32, max_y: i32) void {
    defineRegion(min_x, min_y, max_x, max_y, min_x, min_y);
}
/// Defines the selected region with its integer-centre hotspot.
pub fn defineRegionCenter(min_x: i32, min_y: i32, max_x: i32, max_y: i32) void {
    defineRegion(min_x, min_y, max_x, max_y, @divTrunc(min_x + max_x, 2), @divTrunc(min_y + max_y, 2));
}
/// Defines every region in a regular matrix, selecting IDs in row order.
pub fn defineRegionMatrix(d: *const RegionMatrix) void {
    var id = d.first_id;
    var min_x = d.first_min_x;
    var min_y = d.first_min_y;
    var max_x = d.first_max_x;
    var max_y = d.first_max_y;
    var hotspot_x = d.first_hotspot_x;
    var hotspot_y = d.first_hotspot_y;
    const advance_x = max_x - min_x + 1 + d.gap;
    const advance_y = max_y - min_y + 1 + d.gap;
    var matrix_y: i32 = 0;
    while (matrix_y < d.elements_y) : (matrix_y += 1) {
        var matrix_x: i32 = 0;
        while (matrix_x < d.elements_x) : (matrix_x += 1) {
            selectRegion(id);
            defineRegion(min_x, min_y, max_x, max_y, hotspot_x, hotspot_y);
            id += 1;
            min_x += advance_x;
            max_x += advance_x;
            hotspot_x += advance_x;
        }
        min_y += advance_y;
        max_y += advance_y;
        hotspot_y += advance_y;
        min_x = d.first_min_x;
        max_x = d.first_max_x;
        hotspot_x = d.first_hotspot_x;
    }
}
/// Draws a BIOS white-pixel horizontal line and leaves its region selected.
pub fn drawBiosHorizontalLine(x1: i32, y: i32, x2: i32) void {
    selectTexture(-1);
    selectRegion(256);
    setDrawingScale(@floatFromInt(x2 - x1 + 1), 1.0);
    drawRegionZoomedAt(x1, y);
}
/// Draws a BIOS white-pixel vertical line and leaves its region selected.
pub fn drawBiosVerticalLine(x: i32, y1: i32, y2: i32) void {
    selectTexture(-1);
    selectRegion(256);
    setDrawingScale(1.0, @floatFromInt(y2 - y1 + 1));
    drawRegionZoomedAt(x, y1);
}

/// Draws a NUL-terminated CP-1252 string using 10×20 BIOS-font glyphs.
/// A newline glyph is drawn first, then x resets and y advances by 20 pixels.
pub fn printAt(initial_x: i32, initial_y: i32, text: [*:0]const u8) void {
    const previous_texture = getSelectedTexture();
    var x = initial_x;
    var y = initial_y;
    var cursor = text;
    selectTexture(-1);
    while (cursor[0] != 0) : (cursor += 1) {
        const glyph: i32 = @intCast(cursor[0]);
        selectRegion(glyph);
        drawRegionAt(x, y);
        x += 10;
        if (glyph == '\n') {
            x = initial_x;
            y += 20;
        }
    }
    selectTexture(previous_texture);
}

// ----
// Public functions: Printing

var digit_text: [1:0]u8 = .{'0'};
/// Recursively draws unsigned decimal digits and returns the next x coordinate.
fn printDigits(x: i32, y: i32, value: u32) i32 {
    const quotient = value / 10;
    var result_x = x;
    if (quotient != 0) result_x = printDigits(result_x, y, quotient);
    digit_text[0] = @intCast('0' + value - quotient * 10);
    printAt(result_x, y, &digit_text);
    return result_x + 10;
}
/// Draws an unsigned decimal i32 value with the BIOS font.
pub fn printUIntAt(x: i32, y: i32, value: u32) void {
    _ = printDigits(x, y, value);
}
/// Draws a signed decimal i32 value with the BIOS font.
pub fn printIntAt(x: i32, y: i32, value: i32) void {
    if (value < 0) {
        printAt(x, y, "-");
        printUIntAt(x + 10, y, @as(u32, @intCast(-(value + 1))) + 1);
    } else printUIntAt(x, y, @as(u32, @intCast(value)));
}
var fraction_text: [2:0]u8 = .{ '0', '0' };
/// Draws a finite f32 with two decimal places; it is not general formatting.
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

// ----
// Public methods: Input

/// Selects gamepad 0 through 3 for subsequent gamepad reads.
pub fn selectGamepad(gamepad: i32) void {
    platform.vircon_input_select_gamepad(gamepad);
}
/// Returns the input device's selected gamepad ID.
pub fn getSelectedGamepad() i32 {
    return platform.vircon_input_get_selected_gamepad();
}
/// Returns whether the selected gamepad's left direction is pressed.
pub fn gamepadLeft() bool {
    return platform.vircon_input_gamepad_left() > 0;
}
/// Returns whether the selected gamepad's right direction is pressed.
pub fn gamepadRight() bool {
    return platform.vircon_input_gamepad_right() > 0;
}
/// Returns whether the selected gamepad's up direction is pressed.
pub fn gamepadUp() bool {
    return platform.vircon_input_gamepad_up() > 0;
}
/// Returns whether the selected gamepad's down direction is pressed.
pub fn gamepadDown() bool {
    return platform.vircon_input_gamepad_down() > 0;
}
/// Returns whether the selected gamepad is connected.
pub fn gamepadIsConnected() bool {
    return platform.vircon_input_gamepad_connected() > 0;
}
/// Returns whether the selected gamepad's A button is pressed.
pub fn gamepadButtonA() bool {
    return platform.vircon_input_gamepad_button_a() > 0;
}
/// Returns whether the selected gamepad's B button is pressed.
pub fn gamepadButtonB() bool {
    return platform.vircon_input_gamepad_button_b() > 0;
}
/// Returns whether the selected gamepad's X button is pressed.
pub fn gamepadButtonX() bool {
    return platform.vircon_input_gamepad_button_x() > 0;
}
/// Returns whether the selected gamepad's Y button is pressed.
pub fn gamepadButtonY() bool {
    return platform.vircon_input_gamepad_button_y() > 0;
}
/// Returns whether the selected gamepad's L button is pressed.
pub fn gamepadButtonL() bool {
    return platform.vircon_input_gamepad_button_l() > 0;
}
/// Returns whether the selected gamepad's R button is pressed.
pub fn gamepadButtonR() bool {
    return platform.vircon_input_gamepad_button_r() > 0;
}
/// Returns whether the selected gamepad's Start button is pressed.
pub fn gamepadButtonStart() bool {
    return platform.vircon_input_gamepad_button_start() > 0;
}
/// Returns horizontal D-pad direction as -1, 0, or 1; left wins ties.
pub fn gamepadDirectionX() i32 {
    return if (gamepadLeft()) -1 else if (gamepadRight()) 1 else 0;
}
/// Returns vertical D-pad direction as -1, 0, or 1; up wins ties.
pub fn gamepadDirectionY() i32 {
    return if (gamepadUp()) -1 else if (gamepadDown()) 1 else 0;
}
/// Returns the unnormalized D-pad direction pair.
pub fn gamepadDirection() Point {
    return .{ .x = gamepadDirectionX(), .y = gamepadDirectionY() };
}
/// Returns the D-pad direction as a unit f32 vector for diagonal movement.
pub fn gamepadDirectionNormalized() Scale {
    const direction = gamepadDirection();
    const factor: f32 = if (direction.x != 0 and direction.y != 0) 0.70710678 else 1.0;
    return .{ .x = @as(f32, @floatFromInt(direction.x)) * factor, .y = @as(f32, @floatFromInt(direction.y)) * factor };
}

// ----
// Public functions: Time and CPU

/// Returns CPU cycles elapsed in the current display frame.
pub fn cycleCounter() i32 {
    return platform.vircon_timer_get_cycle_counter();
}
/// Returns the display-frame counter.
pub fn frameCounter() i32 {
    return platform.vircon_timer_get_frame_counter();
}
/// Returns a nonnegative count of seconds since midnight.
pub fn getTime() i32 {
    return platform.vircon_timer_get_current_time();
}
/// Returns Vircon's packed year/day-of-year date value.
pub fn getDate() i32 {
    return platform.vircon_timer_get_current_date();
}
/// Splits seconds since midnight into ordinary clock fields.
pub fn translateTime(time: i32) TimeInfo {
    const value: u32 = @bitCast(time);
    return .{
        .hours = @intCast(value / 3600),
        .minutes = @intCast((value % 3600) / 60),
        .seconds = @intCast(value % 60),
    };
}
/// Splits Vircon's packed year/day-of-year value into calendar fields.
pub fn translateDate(date: i32) DateInfo {
    const month_days = [_]i32{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    var result = DateInfo{ .year = date >> 16, .month = 12, .day = 1 };
    var days = date & 0xffff;
    const leap_year = @mod(result.year, 4) == 0 and @mod(result.year, 100) != 0;
    var month: usize = 0;
    while (month < 11) : (month += 1) {
        var days_in_month = month_days[month];
        if (month == 1 and leap_year) days_in_month = 29;
        if (days < days_in_month) {
            result.month = @intCast(month + 1);
            result.day = days + 1;
            return result;
        }
        days -= days_in_month;
    }
    result.day = days + 1;
    return result;
}
/// Reads Vircon's current RNG state; it is not a hosted libc generator.
pub fn rand() i32 {
    return platform.vircon_rng_get_current_value();
}
/// Writes Vircon's current RNG state; target hardware owns normalization.
pub fn srand(seed: i32) void {
    platform.vircon_rng_set_current_value(seed);
}
/// Waits for the requested number of display frames.
pub fn sleep(frames: i32) void {
    const final_frame = frameCounter() + frames;
    while (frameCounter() < final_frame) endFrame();
}
/// Halts the Vircon32 CPU; there is no hosted process exit status.
pub fn halt() void {
    platform.vircon_cpu_halt();
}

// ----
// Public functions: Sound

/// Selects a sound resource by cartridge resource ID.
pub fn selectSound(sound: i32) void {
    platform.vircon_spu_select_sound(sound);
}
/// Returns the selected sound resource ID.
pub fn getSelectedSound() i32 {
    return platform.vircon_spu_get_selected_sound();
}
/// Enables or disables looping for the selected sound resource.
pub fn setSoundLoop(enabled: bool) void {
    platform.vircon_spu_set_sound_play_with_loop(@intFromBool(enabled));
}
/// Sets the selected sound's loop-start offset in samples.
pub fn setSoundLoopStart(position: i32) void {
    platform.vircon_spu_set_sound_loop_start(position);
}
/// Sets the selected sound's loop-end offset in samples.
pub fn setSoundLoopEnd(position: i32) void {
    platform.vircon_spu_set_sound_loop_end(position);
}
/// Selects an SPU channel from 0 through 15.
pub fn selectChannel(channel: i32) void {
    platform.vircon_spu_select_channel(channel);
}
/// Returns the selected SPU channel ID.
pub fn getSelectedChannel() i32 {
    return platform.vircon_spu_get_selected_channel();
}
/// Assigns a sound resource to a channel without starting it.
pub fn assignChannelSound(channel: i32, sound: i32) void {
    selectChannel(channel);
    platform.vircon_spu_set_channel_assigned_sound(sound);
}
/// Assigns a sound resource to a channel and starts it.
pub fn playSoundInChannel(sound: i32, channel: i32) void {
    assignChannelSound(channel, sound);
    platform.vircon_spu_play_selected_channel();
}
/// Sets the selected channel's typed f32 volume multiplier.
pub fn setChannelVolume(value: f32) void {
    platform.vircon_spu_set_channel_volume(value);
}
/// Sets the selected channel's typed f32 speed multiplier.
pub fn setChannelSpeed(value: f32) void {
    platform.vircon_spu_set_channel_speed(value);
}
/// Sets the selected channel's sample position.
pub fn setChannelPosition(position: i32) void {
    platform.vircon_spu_set_channel_position(position);
}
/// Enables or disables looping on the selected channel.
pub fn setChannelLoop(enabled: bool) void {
    platform.vircon_spu_set_channel_loop_enabled(@intFromBool(enabled));
}
/// Sets the SPU's typed f32 global volume multiplier.
pub fn setGlobalVolume(value: f32) void {
    platform.vircon_spu_set_global_volume(value);
}
/// Starts an already configured channel.
pub fn playChannel(channel: i32) void {
    selectChannel(channel);
    platform.vircon_spu_play_selected_channel();
}
/// Pauses one channel.
pub fn pauseChannel(channel: i32) void {
    selectChannel(channel);
    platform.vircon_spu_pause_selected_channel();
}
/// Stops one channel.
pub fn stopChannel(channel: i32) void {
    selectChannel(channel);
    platform.vircon_spu_stop_selected_channel();
}
/// Returns one of `channel_stopped`, `channel_paused`, or `channel_playing`.
pub fn getChannelState(channel: i32) i32 {
    selectChannel(channel);
    return platform.vircon_spu_get_channel_state();
}
/// Returns a channel's typed f32 speed multiplier.
pub fn getChannelSpeed(channel: i32) f32 {
    selectChannel(channel);
    return platform.vircon_spu_get_channel_speed();
}
/// Returns a channel's current sample position.
pub fn getChannelPosition(channel: i32) i32 {
    selectChannel(channel);
    return platform.vircon_spu_get_channel_position();
}
/// Returns the SPU's typed f32 global volume multiplier.
pub fn getGlobalVolume() f32 {
    return platform.vircon_spu_get_global_volume();
}
/// Pauses all SPU channels.
pub fn pauseAllChannels() void {
    platform.vircon_spu_pause_all_channels();
}
/// Stops all SPU channels.
pub fn stopAllChannels() void {
    platform.vircon_spu_stop_all_channels();
}
/// Resumes all SPU channels.
pub fn resumeAllChannels() void {
    platform.vircon_spu_resume_all_channels();
}
/// Plays a sound on the first stopped channel, or returns -1 when none is free.
pub fn playSound(sound: i32) i32 {
    var channel: i32 = 0;
    while (channel < sound_channels) : (channel += 1) {
        selectChannel(channel);
        if (platform.vircon_spu_get_channel_state() == channel_stopped) {
            platform.vircon_spu_set_channel_assigned_sound(sound);
            platform.vircon_spu_play_selected_channel();
            return channel;
        }
    }
    return -1;
}

// ----
// Public functions: Memory Card

/// Returns whether a Vircon memory card is connected.
pub fn cardIsConnected() bool {
    return platform.vircon_memcard_is_connected() > 0;
}
/// Reads one word-addressed memory-card word.
pub fn cardReadWord(word_index: i32) i32 {
    return platform.vircon_memcard_read_word(word_index);
}
/// Writes one word-addressed memory-card word.
pub fn cardWriteWord(word_index: i32, value: i32) void {
    platform.vircon_memcard_write_word(word_index, value);
}
/// Copies card words to a Zig i32 buffer; offsets and counts are words.
pub fn cardReadWords(destination: [*]i32, card_word_offset: i32, word_count: i32) void {
    var index: i32 = 0;
    while (index < word_count) : (index += 1) destination[@intCast(index)] = cardReadWord(card_word_offset + index);
}
/// Copies a Zig i32 buffer to card words; offsets and counts are words.
pub fn cardWriteWords(source: [*]const i32, card_word_offset: i32, word_count: i32) void {
    var index: i32 = 0;
    while (index < word_count) : (index += 1) cardWriteWord(card_word_offset + index, source[@intCast(index)]);
}
/// Returns whether a card word range equals the supplied Zig i32 buffer.
pub fn cardWordsMatch(expected: [*]const i32, card_word_offset: i32, word_count: i32) bool {
    var index: i32 = 0;
    while (index < word_count) : (index += 1) {
        if (cardReadWord(card_word_offset + index) != expected[@intCast(index)]) return false;
    }
    return true;
}
/// Reads the standard twenty-word card signature.
pub fn cardReadSignature(signature: *GameSignature) void {
    cardReadWords(&signature[0], 0, @intCast(game_signature_words));
}
/// Writes the standard twenty-word card signature.
pub fn cardWriteSignature(signature: *const GameSignature) void {
    cardWriteWords(&signature[0], 0, @intCast(game_signature_words));
}
/// Returns whether the standard card signature matches the supplied signature.
pub fn cardSignatureMatches(signature: *const GameSignature) bool {
    return cardWordsMatch(&signature[0], 0, @intCast(game_signature_words));
}
/// Returns whether the standard card signature is entirely zero.
pub fn cardIsEmpty() bool {
    const empty: GameSignature = [_]i32{0} ** game_signature_words;
    return cardSignatureMatches(&empty);
}
/// Copies little-endian card words into an ordinary byte-addressed Zig buffer.
pub fn cardReadData(destination: [*]u8, card_word_offset: i32, word_count: i32) void {
    var index: i32 = 0;
    while (index < word_count) : (index += 1) {
        const word: u32 = @bitCast(cardReadWord(card_word_offset + index));
        const base: usize = @intCast(index * 4);
        destination[base] = @truncate(word);
        destination[base + 1] = @truncate(word >> 8);
        destination[base + 2] = @truncate(word >> 16);
        destination[base + 3] = @truncate(word >> 24);
    }
}
/// Packs ordinary byte-addressed Zig data as little-endian card words.
pub fn cardWriteData(source: [*]const u8, card_word_offset: i32, word_count: i32) void {
    var index: i32 = 0;
    while (index < word_count) : (index += 1) {
        const base: usize = @intCast(index * 4);
        const word: u32 = @as(u32, source[base]) |
            @as(u32, source[base + 1]) << 8 |
            @as(u32, source[base + 2]) << 16 |
            @as(u32, source[base + 3]) << 24;
        cardWriteWord(card_word_offset + index, @bitCast(word));
    }
}

// ----
// Public functions: Math

/// Returns the Vircon CPU's finite-domain sine result.
pub fn sin(value: f32) f32 {
    return platform.vircon_cpu_sin(value);
}
/// Returns cosine through Vircon sine with a π/2 offset.
pub fn cos(value: f32) f32 {
    return sin(value + 1.57079632679);
}
/// Returns tangent as finite-domain sine divided by cosine.
pub fn tan(value: f32) f32 {
    return sin(value) / cos(value);
}
/// Returns the Vircon CPU's finite-domain inverse cosine result.
pub fn acos(value: f32) f32 {
    return platform.vircon_cpu_acos(value);
}
/// Returns inverse sine derived from inverse cosine.
pub fn asin(value: f32) f32 {
    return 1.57079632679 - acos(value);
}
/// Returns e raised to a finite-domain exponent.
pub fn exp(value: f32) f32 {
    return platform.vircon_cpu_pow(2.71828182846, value);
}
/// Returns the Vircon CPU's finite-domain natural logarithm result.
pub fn log(value: f32) f32 {
    return platform.vircon_cpu_log(value);
}
/// Returns the Vircon CPU's finite-domain power result.
pub fn pow(x: f32, y: f32) f32 {
    return platform.vircon_cpu_pow(x, y);
}
/// Returns the Vircon CPU's finite-domain floating remainder result.
pub fn fmod(x: f32, y: f32) f32 {
    return platform.vircon_cpu_fmod(x, y);
}
/// Returns the smaller signed i32 through the Vircon CPU operation.
pub fn min(x: i32, y: i32) i32 {
    return platform.vircon_cpu_imin(x, y);
}
/// Returns the larger signed i32 through the Vircon CPU operation.
pub fn max(x: i32, y: i32) i32 {
    return platform.vircon_cpu_imax(x, y);
}
/// Returns signed i32 absolute value through the Vircon CPU operation.
pub fn abs(value: i32) i32 {
    return platform.vircon_cpu_iabs(value);
}
/// Returns the smaller f32 through the Vircon CPU operation.
pub fn fmin(x: f32, y: f32) f32 {
    return platform.vircon_cpu_fmin(x, y);
}
/// Returns the larger f32 through the Vircon CPU operation.
pub fn fmax(x: f32, y: f32) f32 {
    return platform.vircon_cpu_fmax(x, y);
}
/// Returns f32 absolute value through the Vircon CPU operation.
pub fn fabs(value: f32) f32 {
    return platform.vircon_cpu_fabs(value);
}
/// Returns f32 floor through the Vircon CPU operation.
pub fn floor(value: f32) f32 {
    return platform.vircon_cpu_floor(value);
}
/// Returns f32 ceiling through the Vircon CPU operation.
pub fn ceil(value: f32) f32 {
    return platform.vircon_cpu_ceil(value);
}
/// Returns f32 rounding through the Vircon CPU operation.
pub fn round(value: f32) f32 {
    return platform.vircon_cpu_round(value);
}
/// Returns the Vircon CPU's finite-domain two-argument arctangent result.
pub fn atan2(y: f32, x: f32) f32 {
    return platform.vircon_cpu_atan2(y, x);
}
/// Returns square root through the Vircon finite-domain power operation.
pub fn sqrt(value: f32) f32 {
    return pow(value, 0.5);
}

//
// ----
// Private hardware-like imports. Don't use these, but the wrappers above.
const platform = struct {
    extern "env" fn vircon_set_background_color(i32) void;
    extern "env" fn vircon_end_frame() void;
    extern "env" fn vircon_gpu_get_selected_texture() i32;
    extern "env" fn vircon_gpu_select_texture(i32) void;
    extern "env" fn vircon_gpu_get_selected_region() i32;
    extern "env" fn vircon_gpu_select_region(i32) void;
    extern "env" fn vircon_gpu_set_region_minimum(i32, i32) void;
    extern "env" fn vircon_gpu_set_region_maximum(i32, i32) void;
    extern "env" fn vircon_gpu_set_region_hotspot(i32, i32) void;
    extern "env" fn vircon_gpu_set_drawing_point(i32, i32) void;
    extern "env" fn vircon_gpu_get_drawing_point_x() i32;
    extern "env" fn vircon_gpu_get_drawing_point_y() i32;
    extern "env" fn vircon_gpu_set_multiply_color(i32) void;
    extern "env" fn vircon_gpu_get_multiply_color() i32;
    extern "env" fn vircon_gpu_set_active_blending(i32) void;
    extern "env" fn vircon_gpu_get_active_blending() i32;
    extern "env" fn vircon_gpu_set_drawing_scale_bits(i32, i32) void;
    extern "env" fn vircon_gpu_set_drawing_scale(f32, f32) void;
    extern "env" fn vircon_gpu_get_drawing_scale_x() f32;
    extern "env" fn vircon_gpu_get_drawing_scale_y() f32;
    extern "env" fn vircon_gpu_set_drawing_angle(f32) void;
    extern "env" fn vircon_gpu_get_drawing_angle() f32;
    extern "env" fn vircon_gpu_draw_region() void;
    extern "env" fn vircon_gpu_draw_region_zoomed() void;
    extern "env" fn vircon_gpu_draw_region_rotated() void;
    extern "env" fn vircon_gpu_draw_region_rotozoomed() void;
    extern "env" fn vircon_input_select_gamepad(i32) void;
    extern "env" fn vircon_input_get_selected_gamepad() i32;
    extern "env" fn vircon_input_gamepad_left() i32;
    extern "env" fn vircon_input_gamepad_right() i32;
    extern "env" fn vircon_input_gamepad_up() i32;
    extern "env" fn vircon_input_gamepad_down() i32;
    extern "env" fn vircon_input_gamepad_connected() i32;
    extern "env" fn vircon_input_gamepad_button_a() i32;
    extern "env" fn vircon_input_gamepad_button_b() i32;
    extern "env" fn vircon_input_gamepad_button_x() i32;
    extern "env" fn vircon_input_gamepad_button_y() i32;
    extern "env" fn vircon_input_gamepad_button_l() i32;
    extern "env" fn vircon_input_gamepad_button_r() i32;
    extern "env" fn vircon_input_gamepad_button_start() i32;
    extern "env" fn vircon_timer_get_frame_counter() i32;
    extern "env" fn vircon_timer_get_cycle_counter() i32;
    extern "env" fn vircon_timer_get_current_time() i32;
    extern "env" fn vircon_timer_get_current_date() i32;
    extern "env" fn vircon_rng_get_current_value() i32;
    extern "env" fn vircon_rng_set_current_value(i32) void;
    extern "env" fn vircon_memcard_is_connected() i32;
    extern "env" fn vircon_memcard_read_word(i32) i32;
    extern "env" fn vircon_memcard_write_word(i32, i32) void;
    extern "env" fn vircon_spu_select_sound(i32) void;
    extern "env" fn vircon_spu_get_selected_sound() i32;
    extern "env" fn vircon_spu_set_sound_play_with_loop(i32) void;
    extern "env" fn vircon_spu_set_sound_loop_start(i32) void;
    extern "env" fn vircon_spu_set_sound_loop_end(i32) void;
    extern "env" fn vircon_spu_select_channel(i32) void;
    extern "env" fn vircon_spu_get_selected_channel() i32;
    extern "env" fn vircon_spu_set_channel_assigned_sound(i32) void;
    extern "env" fn vircon_spu_play_selected_channel() void;
    extern "env" fn vircon_spu_pause_selected_channel() void;
    extern "env" fn vircon_spu_stop_selected_channel() void;
    extern "env" fn vircon_spu_set_channel_volume(f32) void;
    extern "env" fn vircon_spu_set_channel_speed(f32) void;
    extern "env" fn vircon_spu_set_channel_position(i32) void;
    extern "env" fn vircon_spu_set_channel_loop_enabled(i32) void;
    extern "env" fn vircon_spu_set_global_volume(f32) void;
    extern "env" fn vircon_spu_get_channel_state() i32;
    extern "env" fn vircon_spu_get_channel_speed() f32;
    extern "env" fn vircon_spu_get_channel_position() i32;
    extern "env" fn vircon_spu_get_global_volume() f32;
    extern "env" fn vircon_spu_pause_all_channels() void;
    extern "env" fn vircon_spu_stop_all_channels() void;
    extern "env" fn vircon_spu_resume_all_channels() void;
    extern "env" fn vircon_cpu_sin(f32) f32;
    extern "env" fn vircon_cpu_acos(f32) f32;
    extern "env" fn vircon_cpu_log(f32) f32;
    extern "env" fn vircon_cpu_pow(f32, f32) f32;
    extern "env" fn vircon_cpu_fmod(f32, f32) f32;
    extern "env" fn vircon_cpu_imin(i32, i32) i32;
    extern "env" fn vircon_cpu_imax(i32, i32) i32;
    extern "env" fn vircon_cpu_iabs(i32) i32;
    extern "env" fn vircon_cpu_fmin(f32, f32) f32;
    extern "env" fn vircon_cpu_fmax(f32, f32) f32;
    extern "env" fn vircon_cpu_fabs(f32) f32;
    extern "env" fn vircon_cpu_floor(f32) f32;
    extern "env" fn vircon_cpu_ceil(f32) f32;
    extern "env" fn vircon_cpu_round(f32) f32;
    extern "env" fn vircon_cpu_atan2(f32, f32) f32;
    extern "env" fn vircon_cpu_halt() void;
};
