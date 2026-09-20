#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-BeatEditor.
 * `cells` is writable static byte data rather than the official compiler's
 * word-addressed local bool matrix. This preserves the visible beat pattern
 * while keeping standard C byte indexing and avoiding a Wasm stack global. */

enum {
    TEXTURE_EDITOR = 0,
    REGION_BACKGROUND = 0,
    REGION_ACTIVE_COLUMN = 1,
    REGION_STOP = 2,
    REGION_PAUSE = 3,
    REGION_PLAY = 4,
    REGION_BUTTON_L = 5,
    REGION_BUTTON_R = 6,
    REGION_DRUM = 7,
    REGION_SELECTION = 11,
    REGION_CELL_MARK = 12,
    SOUND_DRUM = 0,
    SOUND_STICKS = 1,
    SOUND_CLAP = 2,
    SOUND_BELL = 3,
    CHANNEL_DRUM = 0,
    CHANNEL_STICKS = 1,
    CHANNEL_CLAP = 2,
    CHANNEL_BELL = 3,
    STATE_STOPPED = 0,
    STATE_PAUSED = 1,
    STATE_PLAYING = 2,
    CHANNEL_PLAYING = 0x42
};

static unsigned char cells[4][16];

static int get_cell_left(int cell_x)
{
    return 66 + (cell_x / 4) * 141 + (cell_x % 4) * 33;
}

static int get_cell_top(int cell_y)
{
    return 153 + cell_y * 48;
}

static int is_key_triggered(int key_value)
{
    if (key_value < 20)
        return key_value == 1;
    return !((key_value - 20) % 6);
}

static void define_editor_regions(void)
{
    static const struct vircon_region_matrix state_icons = {
        REGION_STOP, 1, 361, 70, 430, 1, 361, 3, 1, 1
    };
    static const struct vircon_region_matrix buttons = {
        REGION_BUTTON_L, 214, 361, 253, 400, 214, 361, 2, 1, 1
    };
    static const struct vircon_region_matrix instruments = {
        REGION_DRUM, 296, 361, 335, 400, 296, 361, 4, 1, 1
    };

    select_texture(TEXTURE_EDITOR);
    select_region(REGION_BACKGROUND);
    set_region_minimum(0, 0); set_region_maximum(639, 359); set_region_hotspot(0, 0);
    select_region(REGION_ACTIVE_COLUMN);
    set_region_minimum(641, 1); set_region_maximum(674, 184); set_region_hotspot(641, 1);
    define_region_matrix(&state_icons);
    define_region_matrix(&buttons);
    define_region_matrix(&instruments);
    select_region(REGION_SELECTION);
    set_region_minimum(460, 361); set_region_maximum(501, 408); set_region_hotspot(464, 365);
    select_region(REGION_CELL_MARK);
    set_region_minimum(503, 361); set_region_maximum(536, 400); set_region_hotspot(503, 361);
}

int main(void)
{
    int state = STATE_STOPPED;
    int played_frames = 0;
    int active_column = 0;
    int selected_cell_x = 0;
    int selected_cell_y = 0;

    define_editor_regions();
    select_gamepad(0);

    for (;;) {
        int cell_y;

        if (gamepad_button_l() == 1 && state != STATE_STOPPED) {
            state = STATE_STOPPED;
            played_frames = 0;
            active_column = 0;
        }
        if (gamepad_button_r() == 1) {
            if (state == STATE_STOPPED)
                state = STATE_PLAYING;
            else if (state == STATE_PLAYING)
                state = STATE_PAUSED;
            else
                state = STATE_PLAYING;
        }
        if (gamepad_button_a() == 1)
            cells[selected_cell_y][selected_cell_x] = !cells[selected_cell_y][selected_cell_x];
        if (is_key_triggered(gamepad_left()) && selected_cell_x > 0)
            --selected_cell_x;
        if (is_key_triggered(gamepad_right()) && selected_cell_x < 15)
            ++selected_cell_x;
        if (is_key_triggered(gamepad_up()) && selected_cell_y > 0)
            --selected_cell_y;
        if (is_key_triggered(gamepad_down()) && selected_cell_y < 3)
            ++selected_cell_y;

        select_region(REGION_BACKGROUND); draw_region_at(0, 0);
        if (state == STATE_PLAYING) {
            select_region(REGION_PLAY); draw_region_at(499, 30);
            select_region(REGION_BUTTON_L); draw_region_at(29, 46);
            select_region(REGION_BUTTON_R); draw_region_at(237, 46);
        }
        else if (state == STATE_PAUSED) {
            select_region(REGION_PAUSE); draw_region_at(285, 30);
            select_region(REGION_BUTTON_L); draw_region_at(29, 46);
            select_region(REGION_BUTTON_R); draw_region_at(453, 46);
        }
        else {
            select_region(REGION_STOP); draw_region_at(75, 30);
            select_region(REGION_BUTTON_R); draw_region_at(453, 46);
        }
        select_region(REGION_ACTIVE_COLUMN);
        draw_region_at(get_cell_left(active_column), get_cell_top(0));
        select_region(REGION_CELL_MARK);
        for (cell_y = 0; cell_y < 4; ++cell_y) {
            int cell_x;
            for (cell_x = 0; cell_x < 16; ++cell_x)
                if (cells[cell_y][cell_x])
                    draw_region_at(get_cell_left(cell_x), get_cell_top(cell_y));
        }
        select_region(REGION_SELECTION);
        draw_region_at(get_cell_left(selected_cell_x), get_cell_top(selected_cell_y));
        for (cell_y = 0; cell_y < 4; ++cell_y) {
            if (get_channel_state(cell_y) == CHANNEL_PLAYING) {
                select_region(REGION_DRUM + cell_y);
                draw_region_at(18, get_cell_top(cell_y));
            }
        }

        if (state == STATE_PLAYING && !(played_frames % 7)) {
            int channel;
            for (channel = 0; channel < 4; ++channel) {
                if (cells[0][active_column]) play_sound_in_channel(SOUND_DRUM, CHANNEL_DRUM);
                if (cells[1][active_column]) play_sound_in_channel(SOUND_STICKS, CHANNEL_STICKS);
                if (cells[2][active_column]) play_sound_in_channel(SOUND_CLAP, CHANNEL_CLAP);
                if (cells[3][active_column]) play_sound_in_channel(SOUND_BELL, CHANNEL_BELL);
            }
        }
        if (state == STATE_PLAYING) {
            ++played_frames;
            active_column = (played_frames / 7) % 16;
        }
        end_frame();
    }
}
