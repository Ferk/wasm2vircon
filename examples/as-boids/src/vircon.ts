/**
 * Reusable Vircon32 API for AssemblyScript programs compiled by wasm2vircon.
 *
 * Copy this file into a project and import it, e.g. `import * as v from "./vircon"`.
 * Export an entry function and compile it to a defined-memory wasm32 module;
 * see this example's asconfig.json and README for a working configuration.
 * The functions are Wasm code and narrow `env.vircon_*` hardware imports,
 * not JavaScript host bindings. Unused functions/imports are removed by asc.
 *
 * Coordinates, resource IDs and colors use i32; Vircon colors are packed ABGR
 * words. Drawing scales, angles and math inputs use f32. Gamepad helpers return
 * bool, while legacy short names return i32. Frame counters are wrapping u32.
 * State such as the selected texture, channel and gamepad belongs to hardware.
 *
 * Do not assume a hosted JavaScript runtime, f64 math, a GC, WASI, mutable
 * Wasm globals, callbacks or unconstrained memory growth are supported.
 * For returned multi-field values, supply a caller-owned `@unmanaged` record,
 * e.g. `const point = changetype<v.Point>(memory.data(offsetof<v.Point>()));`.
 * Allocate buffers with memory.data; buffer addresses are byte-addressed Wasm
 * `usize`, whereas memory-card offsets and ramReadWord use Vircon word indices.
 * Helper functions document side effects and do not allocate managed objects.
 */

// Hardware imports. The legacy names are kept as a public compatibility API;
// use the descriptive camelCase helpers below for new projects.
/** Clears the next displayed frame to a packed ABGR color. */
@external("env", "vircon_set_background_color") export declare function clearScreen(color: i32): void;
/** Waits for the next display frame. */
@external("env", "vircon_end_frame") export declare function endFrame(): void;
/** Returns the selected texture, including BIOS texture -1. */
@external("env", "vircon_gpu_get_selected_texture") export declare function selectedTexture(): i32;
/** Selects a cartridge texture ID, or -1 for the BIOS texture. */
@external("env", "vircon_gpu_select_texture") export declare function selectTexture(id: i32): void;
/** Selects a region in the current texture. */
@external("env", "vircon_gpu_select_region") export declare function selectRegion(id: i32): void;
/** Sets the GPU drawing point in pixels. */
@external("env", "vircon_gpu_set_drawing_point") export declare function drawingPoint(x: i32, y: i32): void;
/** Sets the packed ABGR texture color multiplier. */
@external("env", "vircon_gpu_set_multiply_color") export declare function multiplyColor(color: i32): void;
/** Sets the drawing scale used by zoomed and rotozoomed draws. */
@external("env", "vircon_gpu_set_drawing_scale") export declare function drawingScale(x: f32, y: f32): void;
/** Sets the drawing angle in radians. */
@external("env", "vircon_gpu_set_drawing_angle") export declare function drawingAngle(angle: f32): void;
/** Draws the selected region at the current point. */
@external("env", "vircon_gpu_draw_region") export declare function drawRegion(): void;
/** Draws the selected region using the current scale. */
@external("env", "vircon_gpu_draw_region_zoomed") export declare function drawZoomed(): void;
/** Draws the selected region using the current scale and angle. */
@external("env", "vircon_gpu_draw_region_rotozoomed") export declare function drawRotozoomed(): void;
/** Sets the GPU blend mode, such as BLENDING_ALPHA. */
@external("env", "vircon_gpu_set_active_blending") export declare function blending(mode: i32): void;
/** Returns the remaining GPU pixel budget, or -1 if exhausted. */
@external("env", "vircon_gpu_get_remaining_pixels") export declare function remainingPixels(): i32;
/** Returns the selected region's minimum texture X coordinate. */
@external("env", "vircon_gpu_get_region_min_x") export declare function regionMinX(): i32;
/** Returns the selected region's minimum texture Y coordinate. */
@external("env", "vircon_gpu_get_region_min_y") export declare function regionMinY(): i32;
/** Sets the selected region's inclusive minimum texture coordinate. */
@external("env", "vircon_gpu_set_region_minimum") export declare function regionMinimum(x: i32, y: i32): void;
/** Sets the selected region's inclusive maximum texture coordinate. */
@external("env", "vircon_gpu_set_region_maximum") export declare function regionMaximum(x: i32, y: i32): void;
/** Sets the selected region's hotspot in texture coordinates. */
@external("env", "vircon_gpu_set_region_hotspot") export declare function regionHotspot(x: i32, y: i32): void;
/** Selects gamepad 0 through 3 for subsequent input reads. */
@external("env", "vircon_input_select_gamepad") export declare function selectGamepad(id: i32): void;
/** Returns the selected gamepad's raw left button state. */
@external("env", "vircon_input_gamepad_left") export declare function left(): i32;
/** Returns the selected gamepad's raw right button state. */
@external("env", "vircon_input_gamepad_right") export declare function right(): i32;
/** Returns the selected gamepad's raw up button state. */
@external("env", "vircon_input_gamepad_up") export declare function up(): i32;
/** Returns the selected gamepad's raw down button state. */
@external("env", "vircon_input_gamepad_down") export declare function down(): i32;
/** Returns the selected gamepad's raw L button state. */
@external("env", "vircon_input_gamepad_button_l") export declare function buttonL(): i32;
/** Returns the selected gamepad's raw R button state. */
@external("env", "vircon_input_gamepad_button_r") export declare function buttonR(): i32;
/** Returns the selected gamepad's raw Start button state. */
@external("env", "vircon_input_gamepad_button_start") export declare function buttonStart(): i32;
/** Returns the wrapping display-frame counter. */
@external("env", "vircon_timer_get_frame_counter") export declare function frameCounter(): u32;
/** Returns cycles elapsed in the current display frame. */
@external("env", "vircon_timer_get_cycle_counter") export declare function cycleCounter(): i32;
/** Returns the Vircon CPU's finite-domain two-argument arctangent. */
@external("env", "vircon_cpu_atan2") export declare function atan2(y: f32, x: f32): f32;
/** Returns the selected region ID for the current texture. */
@external("env", "vircon_gpu_get_selected_region") export declare function getSelectedRegion(): i32;
/** Returns the current drawing point's X coordinate. */
@external("env", "vircon_gpu_get_drawing_point_x") export declare function getDrawingPointX(): i32;
/** Returns the current drawing point's Y coordinate. */
@external("env", "vircon_gpu_get_drawing_point_y") export declare function getDrawingPointY(): i32;
/** Returns the active packed ABGR color multiplier. */
@external("env", "vircon_gpu_get_multiply_color") export declare function getMultiplyColor(): i32;
/** Returns the active GPU blend-mode word. */
@external("env", "vircon_gpu_get_active_blending") export declare function getBlendingMode(): i32;
/** Sets drawing scale from already-known IEEE-754 f32 bit patterns. */
@external("env", "vircon_gpu_set_drawing_scale_bits") export declare function setDrawingScaleBits(xBits: i32, yBits: i32): void;
/** Returns the current horizontal drawing scale. */
@external("env", "vircon_gpu_get_drawing_scale_x") export declare function getDrawingScaleX(): f32;
/** Returns the current vertical drawing scale. */
@external("env", "vircon_gpu_get_drawing_scale_y") export declare function getDrawingScaleY(): f32;
/** Returns the current drawing angle in radians. */
@external("env", "vircon_gpu_get_drawing_angle") export declare function getDrawingAngle(): f32;
/** Draws the selected region using the current angle. */
@external("env", "vircon_gpu_draw_region_rotated") export declare function drawRegionRotated(): void;
/** Returns the currently selected gamepad ID. */
@external("env", "vircon_input_get_selected_gamepad") export declare function getSelectedGamepad(): i32;
/** Returns the current time as seconds since midnight. */
@external("env", "vircon_timer_get_current_time") export declare function getTime(): i32;
/** Returns the packed year and zero-based day-of-year date. */
@external("env", "vircon_timer_get_current_date") export declare function getDate(): i32;
/** Reads the Vircon hardware random generator's current value. */
@external("env", "vircon_rng_get_current_value") export declare function rand(): i32;
/** Sets the Vircon hardware random generator's state. */
@external("env", "vircon_rng_set_current_value") export declare function srand(seed: i32): void;
/** Halts the Vircon CPU; no hosted exit status is produced. */
@external("env", "vircon_cpu_halt") export declare function halt(): void;
/** Selects a sound resource ID for sound configuration. */
@external("env", "vircon_spu_select_sound") export declare function selectSound(sound: i32): void;
/** Returns the currently selected sound resource ID. */
@external("env", "vircon_spu_get_selected_sound") export declare function getSelectedSound(): i32;
/** Sets the selected sound's loop start in samples. */
@external("env", "vircon_spu_set_sound_loop_start") export declare function setSoundLoopStart(position: i32): void;
/** Sets the selected sound's loop end in samples. */
@external("env", "vircon_spu_set_sound_loop_end") export declare function setSoundLoopEnd(position: i32): void;
/** Selects SPU channel 0 through 15 for subsequent commands. */
@external("env", "vircon_spu_select_channel") export declare function selectChannel(channel: i32): void;
/** Returns the selected SPU channel ID. */
@external("env", "vircon_spu_get_selected_channel") export declare function getSelectedChannel(): i32;
/** Sets the selected channel's volume multiplier. */
@external("env", "vircon_spu_set_channel_volume") export declare function setChannelVolume(value: f32): void;
/** Sets the selected channel's speed multiplier. */
@external("env", "vircon_spu_set_channel_speed") export declare function setChannelSpeed(value: f32): void;
/** Sets the selected channel's sample position. */
@external("env", "vircon_spu_set_channel_position") export declare function setChannelPosition(position: i32): void;
/** Sets the SPU's global volume multiplier. */
@external("env", "vircon_spu_set_global_volume") export declare function setGlobalVolume(value: f32): void;
/** Returns the SPU's global volume multiplier. */
@external("env", "vircon_spu_get_global_volume") export declare function getGlobalVolume(): f32;
/** Pauses all sound channels. */
@external("env", "vircon_spu_pause_all_channels") export declare function pauseAllChannels(): void;
/** Stops all sound channels. */
@external("env", "vircon_spu_stop_all_channels") export declare function stopAllChannels(): void;
/** Resumes all sound channels. */
@external("env", "vircon_spu_resume_all_channels") export declare function resumeAllChannels(): void;
/** Reads a Vircon RAM word using a word address, not a Wasm byte address. */
@external("env", "vircon_ram_read_word") export declare function ramReadWord(wordAddress: i32): i32;
/** Reads one memory-card word by word index. */
@external("env", "vircon_memcard_read_word") export declare function cardReadWord(wordIndex: i32): i32;
/** Writes one memory-card word by word index. */
@external("env", "vircon_memcard_write_word") export declare function cardWriteWord(wordIndex: i32, value: i32): void;
/** Returns the Vircon CPU's finite-domain sine. */
@external("env", "vircon_cpu_sin") export declare function sin(value: f32): f32;
/** Returns the Vircon CPU's finite-domain inverse cosine. */
@external("env", "vircon_cpu_acos") export declare function acos(value: f32): f32;
/** Returns the Vircon CPU's finite-domain natural logarithm. */
@external("env", "vircon_cpu_log") export declare function log(value: f32): f32;
/** Returns the Vircon CPU's finite-domain power result. */
@external("env", "vircon_cpu_pow") export declare function pow(x: f32, y: f32): f32;
/** Returns the Vircon CPU's finite-domain floating remainder. */
@external("env", "vircon_cpu_fmod") export declare function fmod(x: f32, y: f32): f32;
/** Returns the smaller signed i32 using the Vircon CPU. */
@external("env", "vircon_cpu_imin") export declare function min(x: i32, y: i32): i32;
/** Returns the larger signed i32 using the Vircon CPU. */
@external("env", "vircon_cpu_imax") export declare function max(x: i32, y: i32): i32;
/** Returns the signed i32 absolute value using the Vircon CPU. */
@external("env", "vircon_cpu_iabs") export declare function abs(value: i32): i32;
/** Returns the smaller f32 using the Vircon CPU. */
@external("env", "vircon_cpu_fmin") export declare function fmin(x: f32, y: f32): f32;
/** Returns the larger f32 using the Vircon CPU. */
@external("env", "vircon_cpu_fmax") export declare function fmax(x: f32, y: f32): f32;
/** Returns the f32 absolute value using the Vircon CPU. */
@external("env", "vircon_cpu_fabs") export declare function fabs(value: f32): f32;
/** Returns the f32 floor using the Vircon CPU. */
@external("env", "vircon_cpu_floor") export declare function floor(value: f32): f32;
/** Returns the f32 ceiling using the Vircon CPU. */
@external("env", "vircon_cpu_ceil") export declare function ceil(value: f32): f32;
/** Returns the f32 rounded value using the Vircon CPU. */
@external("env", "vircon_cpu_round") export declare function round(value: f32): f32;

// Private low-level imports used by the higher-level bool and channel helpers.
@external("env", "vircon_input_gamepad_connected") declare function inputConnected(): i32;
@external("env", "vircon_input_gamepad_button_a") declare function inputA(): i32;
@external("env", "vircon_input_gamepad_button_b") declare function inputB(): i32;
@external("env", "vircon_input_gamepad_button_x") declare function inputX(): i32;
@external("env", "vircon_input_gamepad_button_y") declare function inputY(): i32;
@external("env", "vircon_memcard_is_connected") declare function memcardConnected(): i32;
@external("env", "vircon_spu_set_sound_play_with_loop") declare function soundLoop(value: i32): void;
@external("env", "vircon_spu_set_channel_loop_enabled") declare function channelLoop(value: i32): void;
@external("env", "vircon_spu_set_channel_assigned_sound") declare function channelSound(value: i32): void;
@external("env", "vircon_spu_play_selected_channel") declare function channelPlay(): void;
@external("env", "vircon_spu_pause_selected_channel") declare function channelPause(): void;
@external("env", "vircon_spu_stop_selected_channel") declare function channelStop(): void;
@external("env", "vircon_spu_get_channel_state") declare function channelState(): i32;
@external("env", "vircon_spu_get_channel_speed") declare function channelSpeed(): f32;
@external("env", "vircon_spu_get_channel_position") declare function channelPosition(): i32;

/** Legacy short name for opaque black. */
export const BLACK: i32 = 0xff000000;
/** Legacy short name for opaque white. */
export const WHITE: i32 = -1;

function glyph(x: i32, y: i32, code: i32): void {
  selectRegion(code);
  drawingPoint(x, y);
  drawRegion();
}

// Map Unicode code points represented by CP-1252's non-Latin-1 slots.
function biosCode(code: i32): i32 {
  if (code < 128 || (code >= 160 && code <= 255)) return code;
  switch (code) {
    case 0x20ac: return 0x80; case 0x201a: return 0x82; case 0x0192: return 0x83;
    case 0x201e: return 0x84; case 0x2026: return 0x85; case 0x2020: return 0x86;
    case 0x2021: return 0x87; case 0x02c6: return 0x88; case 0x2030: return 0x89;
    case 0x0160: return 0x8a; case 0x2039: return 0x8b; case 0x0152: return 0x8c;
    case 0x017d: return 0x8e; case 0x2018: return 0x91; case 0x2019: return 0x92;
    case 0x201c: return 0x93; case 0x201d: return 0x94; case 0x2022: return 0x95;
    case 0x2013: return 0x96; case 0x2014: return 0x97; case 0x02dc: return 0x98;
    case 0x2122: return 0x99; case 0x0161: return 0x9a; case 0x203a: return 0x9b;
    case 0x0153: return 0x9c; case 0x017e: return 0x9e; case 0x0178: return 0x9f;
    default: return 63;
  }
}

/** Draws a UTF-16 AssemblyScript string with the BIOS CP-1252 font.
 * Newlines advance 20 pixels and reset X; unrepresentable characters become
 * '?'. The selected texture is restored; the region and drawing point change.
 * String literals require no allocation. */
export function printAt(initialX: i32, y: i32, text: string): void {
  const previous = selectedTexture();
  selectTexture(-1);
  let x = initialX;
  for (let i = 0; i < text.length; ++i) {
    let code = <i32>load<u16>(changetype<usize>(text) + <usize>(i * 2));
    if (code == 10) { x = initialX; y += 20; }
    else {
      // Replace a complete surrogate pair with a single fallback glyph.
      if (code >= 0xd800 && code <= 0xdbff && i + 1 < text.length) {
        const next = load<u16>(changetype<usize>(text) + <usize>((i + 1) * 2));
        if (next >= 0xdc00 && next <= 0xdfff) ++i;
      }
      glyph(x, y, biosCode(code)); x += 10;
    }
  }
  selectTexture(previous);
}

function digits(x: i32, y: i32, value: u32): i32 {
  if (value >= 10) x = digits(x, y, value / 10);
  glyph(x, y, 48 + <i32>(value % 10));
  return x + 10;
}

/** Draws an unsigned decimal integer using the BIOS font; restores selected texture. */
export function printUIntAt(x: i32, y: i32, value: u32): void {
  const previous = selectedTexture();
  selectTexture(-1);
  digits(x, y, value);
  selectTexture(previous);
}

/** Draws a nonnegative integer count of hundredths as a two-decimal number. */
export function printHundredthsAt(x: i32, y: i32, value: u32): void {
  const previous = selectedTexture();
  selectTexture(-1);
  x = digits(x, y, value / 100);
  glyph(x, y, 46);
  glyph(x + 10, y, 48 + <i32>(value / 10 % 10));
  glyph(x + 20, y, 48 + <i32>(value % 10));
  selectTexture(previous);
}

// Display constants and packed ABGR colors.
/** Visible display width in pixels. */
export const SCREEN_WIDTH: i32 = 640;
/** Visible display height in pixels. */
export const SCREEN_HEIGHT: i32 = 360;
/** GPU pixel budget per display frame. */
export const GPU_PIXELS_PER_FRAME: i32 = 2073600;
/** Display frames per second. */
export const FRAMES_PER_SECOND: i32 = 60;
/** Duration of a display frame in seconds. */
export const FRAME_TIME: f32 = <f32>(1.0 / 60.0);
/** BIOS font character width, in pixels. */
export const BIOS_CHARACTER_WIDTH: i32 = 10;
/** BIOS font character height, in pixels. */
export const BIOS_CHARACTER_HEIGHT: i32 = 20;
/** BIOS texture ID. */
export const BIOS_TEXTURE: i32 = -1;
/** BIOS region containing one white pixel. */
export const BIOS_PIXEL_REGION: i32 = 256;
/** Normal alpha blending command. */
export const BLENDING_ALPHA: i32 = 0x20;
/** Additive blending command. */
export const BLENDING_ADD: i32 = 0x21;
/** Subtractive blending command. */
export const BLENDING_SUBTRACT: i32 = 0x22;
/** Opaque black in ABGR format. */
export const COLOR_BLACK: i32 = BLACK;
/** Opaque white in ABGR format. */
export const COLOR_WHITE: i32 = WHITE;
/** Opaque gray in ABGR format. */
export const COLOR_GRAY: i32 = 0xff808080;
/** Opaque dark gray in ABGR format. */
export const COLOR_DARKGRAY: i32 = 0xff404040;
/** Opaque light gray in ABGR format. */
export const COLOR_LIGHTGRAY: i32 = 0xffc0c0c0;
/** Opaque red in ABGR format. */
export const COLOR_RED: i32 = 0xff0000ff;
/** Opaque green in ABGR format. */
export const COLOR_GREEN: i32 = 0xff00ff00;
/** Opaque blue in ABGR format. */
export const COLOR_BLUE: i32 = 0xffff0000;
/** Opaque yellow in ABGR format. */
export const COLOR_YELLOW: i32 = 0xff00ffff;
/** Opaque magenta in ABGR format. */
export const COLOR_MAGENTA: i32 = 0xffff00ff;
/** Opaque cyan in ABGR format. */
export const COLOR_CYAN: i32 = 0xffffff00;
/** Opaque orange in ABGR format. */
export const COLOR_ORANGE: i32 = 0xff0080ff;
/** Opaque brown in ABGR format. */
export const COLOR_BROWN: i32 = 0xff204080;
/** Stopped sound channel state. */
export const CHANNEL_STOPPED: i32 = 0x40;
/** Paused sound channel state. */
export const CHANNEL_PAUSED: i32 = 0x41;
/** Playing sound channel state. */
export const CHANNEL_PLAYING: i32 = 0x42;
/** Number of hardware sound channels. */
export const SOUND_CHANNELS: i32 = 16;
/** Number of words in a standard memory-card game signature. */
export const GAME_SIGNATURE_WORDS: i32 = 20;
/** Single-precision pi. */
export const PI: f32 = 3.1415926;

/** Caller-owned pair of pixel coordinates or integer directions. */
@unmanaged export class Point {
  /** Horizontal coordinate. */ x: i32;
  /** Vertical coordinate. */ y: i32;
}
/** Caller-owned pair of single-precision scale or vector components. */
@unmanaged export class Scale {
  /** Horizontal scale or vector component. */ x: f32;
  /** Vertical scale or vector component. */ y: f32;
}
/** Caller-owned calendar date; month and day start at one. */
@unmanaged export class DateInfo {
  /** Full year. */ year: i32;
  /** Month from 1 through 12. */ month: i32;
  /** Day of month starting at 1. */ day: i32;
}
/** Caller-owned hour, minute and second values. */
@unmanaged export class TimeInfo {
  /** Hour since midnight. */ hours: i32;
  /** Minute within the hour. */ minutes: i32;
  /** Second within the minute. */ seconds: i32;
}
/** Caller-owned regular region grid; texture bounds are inclusive. */
@unmanaged export class RegionMatrix {
  /** Region ID assigned to the top-left cell. */ firstId: i32;
  /** First cell's inclusive minimum X texture coordinate. */ firstMinX: i32;
  /** First cell's inclusive minimum Y texture coordinate. */ firstMinY: i32;
  /** First cell's inclusive maximum X texture coordinate. */ firstMaxX: i32;
  /** First cell's inclusive maximum Y texture coordinate. */ firstMaxY: i32;
  /** First cell's hotspot X texture coordinate. */ firstHotspotX: i32;
  /** First cell's hotspot Y texture coordinate. */ firstHotspotY: i32;
  /** Number of cells per row. */ elementsX: i32;
  /** Number of rows. */ elementsY: i32;
  /** Texture-pixel gap between adjacent cells. */ gap: i32;
}

/** Packs opaque gray from the low eight bits of brightness. */
export function makeGray(brightness: i32): i32 { return makeColorRgb(brightness, brightness, brightness); }
/** Packs opaque RGB channels into an ABGR word. */
export function makeColorRgb(red: i32, green: i32, blue: i32): i32 { return makeColorRgba(red, green, blue, 255); }
/** Packs the low eight bits of RGBA components into an ABGR word. */
export function makeColorRgba(red: i32, green: i32, blue: i32, alpha: i32): i32 {
  return ((alpha & 255) << 24) | ((blue & 255) << 16) | ((green & 255) << 8) | (red & 255);
}
/** Extracts the red byte from an ABGR word. */
export function colorRed(color: i32): i32 { return color & 255; }
/** Extracts the green byte from an ABGR word. */
export function colorGreen(color: i32): i32 { return (color >>> 8) & 255; }
/** Extracts the blue byte from an ABGR word. */
export function colorBlue(color: i32): i32 { return (color >>> 16) & 255; }
/** Extracts the alpha byte from an ABGR word. */
export function colorAlpha(color: i32): i32 { return (color >>> 24) & 255; }

/** Returns the current texture ID, including BIOS texture -1. */
export function getSelectedTexture(): i32 { return selectedTexture(); }
/** Returns the GPU pixel budget, or -1 if exhausted. */
export function getGpuRemainingPixels(): i32 { return remainingPixels(); }
/** Writes the selected region's minimum coordinate to caller-owned storage. */
export function getRegionMinimum(out: Point): void { out.x = regionMinX(); out.y = regionMinY(); }
/** Sets the selected region's inclusive minimum coordinate. */
export function setRegionMinimum(x: i32, y: i32): void { regionMinimum(x, y); }
/** Sets the selected region's inclusive maximum coordinate. */
export function setRegionMaximum(x: i32, y: i32): void { regionMaximum(x, y); }
/** Sets the selected region's hotspot in texture coordinates. */
export function setRegionHotspot(x: i32, y: i32): void { regionHotspot(x, y); }
/** Sets the current pixel drawing point. */
export function setDrawingPoint(x: i32, y: i32): void { drawingPoint(x, y); }
/** Writes the current drawing point to caller-owned storage. */
export function getDrawingPoint(out: Point): void { out.x = getDrawingPointX(); out.y = getDrawingPointY(); }
/** Sets the packed ABGR texture multiplier. */
export function setMultiplyColor(color: i32): void { multiplyColor(color); }
/** Sets a BLENDING_* GPU blend mode. */
export function setBlendingMode(mode: i32): void { blending(mode); }
/** Sets the typed f32 drawing scale. */
export function setDrawingScale(x: f32, y: f32): void { drawingScale(x, y); }
/** Writes the current f32 drawing scale to caller-owned storage. */
export function getDrawingScale(out: Scale): void { out.x = getDrawingScaleX(); out.y = getDrawingScaleY(); }
/** Sets the drawing angle in radians. */
export function setDrawingAngle(angle: f32): void { drawingAngle(angle); }
/** Draws the selected region using the active scale. */
export function drawRegionZoomed(): void { drawZoomed(); }
/** Draws the selected region using the active scale and angle. */
export function drawRegionRotozoomed(): void { drawRotozoomed(); }
/** Sets a drawing point and draws the selected region. */
export function drawRegionAt(x: i32, y: i32): void { drawingPoint(x, y); drawRegion(); }
/** Sets a drawing point and draws with the active scale. */
export function drawRegionZoomedAt(x: i32, y: i32): void { drawingPoint(x, y); drawZoomed(); }
/** Sets a drawing point and draws with the active angle. */
export function drawRegionRotatedAt(x: i32, y: i32): void { drawingPoint(x, y); drawRegionRotated(); }
/** Sets a drawing point and draws with the active scale and angle. */
export function drawRegionRotozoomedAt(x: i32, y: i32): void { drawingPoint(x, y); drawRotozoomed(); }
/** Defines the selected region's inclusive bounds and explicit hotspot. */
export function defineRegion(minX: i32, minY: i32, maxX: i32, maxY: i32, hotspotX: i32, hotspotY: i32): void {
  regionMinimum(minX, minY); regionMaximum(maxX, maxY); regionHotspot(hotspotX, hotspotY);
}
/** Defines a region with a top-left hotspot. */
export function defineRegionTopLeft(minX: i32, minY: i32, maxX: i32, maxY: i32): void {
  defineRegion(minX, minY, maxX, maxY, minX, minY);
}
/** Defines a region with an integer-center hotspot. */
export function defineRegionCenter(minX: i32, minY: i32, maxX: i32, maxY: i32): void {
  defineRegion(minX, minY, maxX, maxY, (minX + maxX) / 2, (minY + maxY) / 2);
}
/** Defines a row-major grid of regions; leaves the last region selected. */
export function defineRegionMatrix(d: RegionMatrix): void {
  let id = d.firstId;
  const stepX = d.firstMaxX - d.firstMinX + 1 + d.gap;
  const stepY = d.firstMaxY - d.firstMinY + 1 + d.gap;
  for (let y = 0; y < d.elementsY; ++y) {
    for (let x = 0; x < d.elementsX; ++x) {
      const dx = x * stepX, dy = y * stepY;
      selectRegion(id++);
      defineRegion(d.firstMinX + dx, d.firstMinY + dy, d.firstMaxX + dx, d.firstMaxY + dy,
                   d.firstHotspotX + dx, d.firstHotspotY + dy);
    }
  }
}
/** Draws an inclusive horizontal BIOS-pixel line; leaves texture, region and scale selected. */
export function drawBiosHorizontalLine(x1: i32, y: i32, x2: i32): void {
  selectTexture(BIOS_TEXTURE); selectRegion(BIOS_PIXEL_REGION);
  drawingScale(<f32>(x2 - x1 + 1), 1); drawRegionZoomedAt(x1, y);
}
/** Draws an inclusive vertical BIOS-pixel line; leaves texture, region and scale selected. */
export function drawBiosVerticalLine(x: i32, y1: i32, y2: i32): void {
  selectTexture(BIOS_TEXTURE); selectRegion(BIOS_PIXEL_REGION);
  drawingScale(1, <f32>(y2 - y1 + 1)); drawRegionZoomedAt(x, y1);
}

/** Prints an AssemblyScript string at the current GPU drawing point. */
export function print(text: string): void { printAt(getDrawingPointX(), getDrawingPointY(), text); }
/** Prints a signed decimal integer, including i32.MIN_VALUE, with the BIOS font. */
export function printIntAt(x: i32, y: i32, value: i32): void {
  const previous = selectedTexture(); selectTexture(-1);
  if (value < 0) { glyph(x, y, 45); x += 10; }
  digits(x, y, value < 0 ? <u32>(-(value + 1)) + 1 : <u32>value);
  selectTexture(previous);
}
/** Prints a finite f32 with two truncated decimal places; magnitude * 100 must fit i32. */
export function printFixed2At(x: i32, y: i32, value: f32): void {
  const previous = selectedTexture(); selectTexture(-1);
  if (value < 0) { glyph(x, y, 45); x += 10; }
  const signed = <i32>(value * <f32>100);
  const magnitude = signed < 0 ? <u32>(-(signed + 1)) + 1 : <u32>signed;
  x = digits(x, y, magnitude / 100);
  glyph(x, y, 46); glyph(x + 10, y, 48 + <i32>(magnitude / 10 % 10));
  glyph(x + 20, y, 48 + <i32>(magnitude % 10)); selectTexture(previous);
}

// Input: boolean helpers query the currently selected gamepad.
/** Returns whether the selected gamepad is connected. */
export function gamepadIsConnected(): bool { return inputConnected() > 0; }
/** Returns whether left is pressed. */
export function gamepadLeft(): bool { return left() > 0; }
/** Returns whether right is pressed. */
export function gamepadRight(): bool { return right() > 0; }
/** Returns whether up is pressed. */
export function gamepadUp(): bool { return up() > 0; }
/** Returns whether down is pressed. */
export function gamepadDown(): bool { return down() > 0; }
/** Returns whether A is pressed. */
export function gamepadButtonA(): bool { return inputA() > 0; }
/** Returns whether B is pressed. */
export function gamepadButtonB(): bool { return inputB() > 0; }
/** Returns whether X is pressed. */
export function gamepadButtonX(): bool { return inputX() > 0; }
/** Returns whether Y is pressed. */
export function gamepadButtonY(): bool { return inputY() > 0; }
/** Returns whether L is pressed. */
export function gamepadButtonL(): bool { return buttonL() > 0; }
/** Returns whether R is pressed. */
export function gamepadButtonR(): bool { return buttonR() > 0; }
/** Returns whether Start is pressed. */
export function gamepadButtonStart(): bool { return buttonStart() > 0; }
/** Returns horizontal direction -1, 0 or 1; left wins ties. */
export function gamepadDirectionX(): i32 { return gamepadLeft() ? -1 : gamepadRight() ? 1 : 0; }
/** Returns vertical direction -1, 0 or 1; up wins ties. */
export function gamepadDirectionY(): i32 { return gamepadUp() ? -1 : gamepadDown() ? 1 : 0; }
/** Writes integer D-pad direction to caller-owned storage. */
export function gamepadDirection(out: Point): void { out.x = gamepadDirectionX(); out.y = gamepadDirectionY(); }
/** Writes a unit f32 vector for diagonal input, or zero when idle. */
export function gamepadDirectionNormalized(out: Scale): void {
  const x = gamepadDirectionX(), y = gamepadDirectionY();
  const factor: f32 = x != 0 && y != 0 ? <f32>0.70710678 : 1;
  out.x = <f32>x * factor; out.y = <f32>y * factor;
}

// Time, CPU and randomness.
/** Writes hours, minutes and seconds for a nonnegative count since midnight. */
export function translateTime(time: i32, out: TimeInfo): void {
  const seconds = <u32>time;
  out.hours = <i32>(seconds / 3600); out.minutes = <i32>(seconds % 3600 / 60); out.seconds = <i32>(seconds % 60);
}
/** Decodes Vircon's year/day-of-year format using the C API's leap-year rule. */
export function translateDate(date: i32, out: DateInfo): void {
  out.year = date >> 16;
  let days = date & 0xffff;
  const leap = out.year % 4 == 0 && out.year % 100 != 0;
  let month = 1;
  while (month < 12) {
    const length = month == 2 ? (leap ? 29 : 28) :
      (month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31);
    if (days < length) break;
    days -= length; ++month;
  }
  out.month = month; out.day = days + 1;
}
/** Waits at least `frames` display frames, including across counter wrap; nonpositive values return. */
export function sleep(frames: i32): void {
  if (frames <= 0) return;
  const start = frameCounter();
  while (frameCounter() - start < <u32>frames) endFrame();
}

// Sound: helpers accepting a channel select it and leave it selected.
/** Enables or disables looping on the selected sound resource. */
export function setSoundLoop(enabled: bool): void { soundLoop(enabled ? 1 : 0); }
/** Enables or disables looping on the selected sound channel. */
export function setChannelLoop(enabled: bool): void { channelLoop(enabled ? 1 : 0); }
/** Selects a channel and assigns a sound without starting playback. */
export function assignChannelSound(channel: i32, sound: i32): void { selectChannel(channel); channelSound(sound); }
/** Selects a channel, assigns a sound and begins playback. */
export function playSoundInChannel(sound: i32, channel: i32): void { assignChannelSound(channel, sound); channelPlay(); }
/** Selects and starts an already configured channel. */
export function playChannel(channel: i32): void { selectChannel(channel); channelPlay(); }
/** Selects and pauses one channel. */
export function pauseChannel(channel: i32): void { selectChannel(channel); channelPause(); }
/** Selects and stops one channel. */
export function stopChannel(channel: i32): void { selectChannel(channel); channelStop(); }
/** Selects a channel and returns its CHANNEL_* playback state. */
export function getChannelState(channel: i32): i32 { selectChannel(channel); return channelState(); }
/** Selects a channel and returns its f32 speed multiplier. */
export function getChannelSpeed(channel: i32): f32 { selectChannel(channel); return channelSpeed(); }
/** Selects a channel and returns its sample position. */
export function getChannelPosition(channel: i32): i32 { selectChannel(channel); return channelPosition(); }
/** Plays on the first stopped channel and returns its ID, or -1 if all are busy. */
export function playSound(sound: i32): i32 {
  for (let channel = 0; channel < SOUND_CHANNELS; ++channel) {
    selectChannel(channel);
    if (channelState() == CHANNEL_STOPPED) { channelSound(sound); channelPlay(); return channel; }
  }
  return -1;
}

// Memory-card offsets/counts are words; Wasm buffer addresses are bytes.
/** Returns whether a memory card is connected. */
export function cardIsConnected(): bool { return memcardConnected() > 0; }
/** Reads card words into a writable Wasm buffer of at least 4 * wordCount bytes. */
export function cardReadWords(destination: usize, cardWordOffset: i32, wordCount: i32): void {
  for (let i = 0; i < wordCount; ++i) store<i32>(destination + <usize>(i * 4), cardReadWord(cardWordOffset + i));
}
/** Writes card words from a Wasm buffer of at least 4 * wordCount bytes. */
export function cardWriteWords(source: usize, cardWordOffset: i32, wordCount: i32): void {
  for (let i = 0; i < wordCount; ++i) cardWriteWord(cardWordOffset + i, load<i32>(source + <usize>(i * 4)));
}
/** Compares card words with a Wasm buffer of at least 4 * wordCount bytes. */
export function cardWordsMatch(expected: usize, cardWordOffset: i32, wordCount: i32): bool {
  for (let i = 0; i < wordCount; ++i)
    if (cardReadWord(cardWordOffset + i) != load<i32>(expected + <usize>(i * 4))) return false;
  return true;
}
/** Reads the 20-word game signature at card offset zero. */
export function cardReadSignature(destination: usize): void { cardReadWords(destination, 0, GAME_SIGNATURE_WORDS); }
/** Writes the 20-word game signature at card offset zero. */
export function cardWriteSignature(source: usize): void { cardWriteWords(source, 0, GAME_SIGNATURE_WORDS); }
/** Compares the card's 20-word game signature with a Wasm buffer. */
export function cardSignatureMatches(expected: usize): bool { return cardWordsMatch(expected, 0, GAME_SIGNATURE_WORDS); }
/** Returns whether the card's 20 signature words are all zero. */
export function cardIsEmpty(): bool {
  for (let i = 0; i < GAME_SIGNATURE_WORDS; ++i) if (cardReadWord(i) != 0) return false;
  return true;
}

/** Returns the defined Wasm memory's current size in 64 KiB pages. */
export function memorySizePages(): i32 { return <i32>memory.size(); }
/** Attempts to grow Wasm memory by pages; returns its previous size or -1 on failure.
 * Growth is bounded by the module maximum and target memory; fixed-memory projects
 * can omit this helper entirely. It does not allocate or initialize objects. */
export function memoryGrowPages(pages: i32): i32 { return <i32>memory.grow(pages); }
/** Reads card words as little-endian bytes into a buffer of at least 4 * wordCount bytes. */
export function cardReadData(destination: usize, cardWordOffset: i32, wordCount: i32): void {
  for (let i = 0; i < wordCount; ++i) {
    const word = <u32>cardReadWord(cardWordOffset + i), base = destination + <usize>(i * 4);
    store<u8>(base, <u8>word); store<u8>(base + 1, <u8>(word >>> 8));
    store<u8>(base + 2, <u8>(word >>> 16)); store<u8>(base + 3, <u8>(word >>> 24));
  }
}
/** Packs little-endian bytes from a buffer of at least 4 * wordCount bytes into card words. */
export function cardWriteData(source: usize, cardWordOffset: i32, wordCount: i32): void {
  for (let i = 0; i < wordCount; ++i) {
    const base = source + <usize>(i * 4);
    const word = <u32>load<u8>(base) | (<u32>load<u8>(base + 1) << 8) |
      (<u32>load<u8>(base + 2) << 16) | (<u32>load<u8>(base + 3) << 24);
    cardWriteWord(cardWordOffset + i, <i32>word);
  }
}

// Finite math matches the Vircon C/Zig API; no general IEEE NaN/Inf guarantees.
/** Returns cosine using Vircon sine with a pi/2 offset. */
export function cos(value: f32): f32 { return sin(value + <f32>1.57079632679); }
/** Returns tangent as sine divided by cosine on the supported finite domain. */
export function tan(value: f32): f32 { return sin(value) / cos(value); }
/** Returns inverse sine derived from inverse cosine. */
export function asin(value: f32): f32 { return <f32>1.57079632679 - acos(value); }
/** Returns e raised to a finite-domain exponent. */
export function exp(value: f32): f32 { return pow(<f32>2.71828182846, value); }
/** Returns square root through the finite-domain power operation. */
export function sqrt(value: f32): f32 { return pow(value, <f32>0.5); }
