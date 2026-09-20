#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-SoundEffects.
 * Mutable upstream globals become main-local state because this restricted
 * frontend intentionally rejects Wasm globals. The static note tables remain
 * normal active data and retain their original values. */

enum {
    TEXTURE_PIANO = 0,
    SOUND_PIANO = 0,
    CHANNEL_PIANO = 15,
    OCTAVE_NOTES = 12,
    OCTAVE_WHITE_NOTES = 7,
    CHANNEL_STOPPED = 0x40,
    CHANNEL_PAUSED = 0x41,
    CHANNEL_PLAYING = 0x42,
    REGION_CONTROLS = 0,
    REGION_STATE_PANEL = 1,
    REGION_WHITE_KEY = 2,
    REGION_WHITE_KEY_PRESSED = 3,
    REGION_BLACK_KEY = 4,
    REGION_BLACK_KEY_PRESSED = 5,
    REGION_ARROW = 6,
    REGION_VOLUME_SLIDER = 7,
    FIRST_REGION_PLAY_BUTTONS = 8
};

static const unsigned char black_octave_notes[OCTAVE_NOTES] = {
    0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0
};
static const int octave_note_x_offsets[OCTAVE_NOTES] = {
    0, 13, 27, 45, 54, 81, 93, 108, 125, 135, 157, 162
};

static void define_piano_regions(void)
{
    static const struct vircon_region_matrix play_buttons = {
        FIRST_REGION_PLAY_BUTTONS, 1, 237, 46, 282, 1, 237, 3, 1, 1
    };

    select_texture(TEXTURE_PIANO);
    select_region(REGION_CONTROLS);
    set_region_minimum(1, 1); set_region_maximum(269, 115); set_region_hotspot(1, 1);
    select_region(REGION_STATE_PANEL);
    set_region_minimum(1, 119); set_region_maximum(239, 233); set_region_hotspot(1, 119);
    select_region(REGION_WHITE_KEY);
    set_region_minimum(243, 119); set_region_maximum(269, 214); set_region_hotspot(243, 119);
    select_region(REGION_WHITE_KEY_PRESSED);
    set_region_minimum(271, 119); set_region_maximum(297, 214); set_region_hotspot(271, 119);
    select_region(REGION_BLACK_KEY);
    set_region_minimum(243, 218); set_region_maximum(260, 275); set_region_hotspot(243, 218);
    select_region(REGION_BLACK_KEY_PRESSED);
    set_region_minimum(262, 218); set_region_maximum(279, 275); set_region_hotspot(262, 218);
    select_region(REGION_ARROW);
    set_region_minimum(144, 237); set_region_maximum(169, 267); set_region_hotspot(149, 267);
    select_region(REGION_VOLUME_SLIDER);
    set_region_minimum(173, 237); set_region_maximum(182, 252); set_region_hotspot(173, 237);
    define_region_matrix(&play_buttons);
}

static void draw_white_notes(void)
{
    int x = 131;
    select_region(REGION_WHITE_KEY);
    for (int i = 0; i < 2 * OCTAVE_WHITE_NOTES; ++i) {
        draw_region_at(x, 230);
        x += 27;
    }
}

static void draw_black_notes(void)
{
    select_region(REGION_BLACK_KEY);
    for (int i = 0; i < OCTAVE_NOTES; ++i) {
        if (!black_octave_notes[i]) continue;
        draw_region_at(131 + octave_note_x_offsets[i], 230);
        draw_region_at(131 + 189 + octave_note_x_offsets[i], 230);
    }
}

static void draw_pressed_note(int sounding_note)
{
    int octave = sounding_note / OCTAVE_NOTES;
    int note = sounding_note % OCTAVE_NOTES;
    select_region(black_octave_notes[note] ? REGION_BLACK_KEY_PRESSED : REGION_WHITE_KEY_PRESSED);
    draw_region_at(131 + octave * 189 + octave_note_x_offsets[note], 230);
}

static void draw_scene(int selected_note, int sounding_note, int volume_level, int note_pressed)
{
    int pressed_note = sounding_note % OCTAVE_NOTES;

    clear_screen(0xFF5C6C8D);
    select_region(REGION_CONTROLS); draw_region_at(58, 57);
    select_region(REGION_STATE_PANEL); draw_region_at(343, 57);
    draw_white_notes();
    if (note_pressed && !black_octave_notes[pressed_note]) draw_pressed_note(sounding_note);
    draw_black_notes();
    if (note_pressed && black_octave_notes[pressed_note]) draw_pressed_note(sounding_note);
    select_region(REGION_ARROW); draw_region_at(128 + 16 * selected_note, 225);
    select_region(REGION_VOLUME_SLIDER); draw_region_at(367 + volume_level / 2, 139);
}

static void draw_channel_state(int channel_state)
{
    if (channel_state == CHANNEL_STOPPED) {
        select_region(FIRST_REGION_PLAY_BUTTONS); draw_region_at(351, 65);
    }
    else if (channel_state == CHANNEL_PAUSED) {
        select_region(FIRST_REGION_PLAY_BUTTONS + 1); draw_region_at(399, 65);
    }
    else if (channel_state == CHANNEL_PLAYING) {
        select_region(FIRST_REGION_PLAY_BUTTONS + 2); draw_region_at(447, 65);
    }
}

int main(void)
{
    int selected_note = 12;
    int sounding_note = 0;
    int volume_level = 100;
    int note_pressed = 0;
    int channel_state = CHANNEL_STOPPED;

    define_piano_regions();
    select_sound(SOUND_PIANO);
    set_sound_loop(1);
    set_sound_loop_start(9081);
    set_sound_loop_end(15655);
    select_channel(CHANNEL_PIANO);
    set_channel_volume(1.0f);
    set_global_volume(0.5f);
    select_gamepad(0);

    for (;;) {
        if (gamepad_left() == 1 && selected_note > 0) --selected_note;
        if (gamepad_right() == 1 && selected_note < 23) ++selected_note;

        if (gamepad_down() == 1 && volume_level > 0) {
            volume_level -= 20;
            set_channel_volume((float)volume_level / 100.0f);
        }
        if (gamepad_up() == 1 && volume_level < 200) {
            volume_level += 20;
            set_channel_volume((float)volume_level / 100.0f);
        }

        if (gamepad_button_a() == 1) {
            play_sound_in_channel(SOUND_PIANO, CHANNEL_PIANO);
            sounding_note = selected_note;
            note_pressed = 1;
            set_channel_speed(powf(2.0f, (float)(sounding_note - OCTAVE_NOTES) / 12.0f));
        }
        else if (gamepad_button_a() == -1) {
            set_channel_loop(0);
            note_pressed = 0;
        }

        channel_state = get_channel_state(CHANNEL_PIANO);
        if (gamepad_button_b() == 1) {
            if (channel_state == CHANNEL_PLAYING) pause_channel(CHANNEL_PIANO);
            else if (channel_state == CHANNEL_PAUSED) play_channel(CHANNEL_PIANO);
        }

        draw_scene(selected_note, sounding_note, volume_level, note_pressed);
        draw_channel_state(channel_state);
        end_frame();
    }
}
