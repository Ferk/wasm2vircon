#include <vircon.h>

#define DICE_SIDES 6
#define PAGE_ROLLS 49

static int rolls[DICE_SIDES];
static int percentages[DICE_SIDES];

static void draw_percentage_bar(int number, int percentage)
{
    select_region(20 + number);
    set_drawing_point(379 + 43 * number, 219);
    set_drawing_scale(1.0f, -2.0f * (float)percentage);
    draw_region_zoomed();
}

int main(void)
{
    static const struct vircon_region_matrix dice = {10, 287, 0, 321, 36, 287, 0, 1, 6, 1};
    static const struct vircon_region_matrix bars = {20, 287, 228, 309, 228, 287, 228, 1, 6, 1};
    int total = 0;
    select_texture(0);
    select_region(0); set_region_minimum(0, 0); set_region_maximum(285, 359); set_region_hotspot(285, 0);
    define_region_matrix(&dice); define_region_matrix(&bars);
    srand(get_time()); select_gamepad(0);
    select_channel(0); set_channel_volume(0.2f);
    select_channel(1); set_channel_volume(0.8f);
    for (;;) {
        int count;
        set_multiply_color(0xFFFFFFFF); clear_screen(0xFF7C6A40);
        for (count = 0; count < PAGE_ROLLS; ++count) {
            int column = count % 7, row = count / 7, result = rand() % 6, i;
            rolls[result]++; total++; play_sound(0);
            for (i = 0; i < DICE_SIDES; ++i) percentages[i] = (int)((100u * (unsigned)rolls[i]) / (unsigned)total);
            select_region(10 + result); draw_region_at(32 + 43 * column, 36 + 42 * row);
            select_region(0); draw_region_at(639, 0);
            for (i = 0; i < DICE_SIDES; ++i) draw_percentage_bar(i, percentages[i]);
            print_at(370, 300, "Total rolls: "); print_uint_at(500, 300, (unsigned)total);
            sleep(10);
        }
        play_sound(1); set_multiply_color(0xFF0000FF); print_at(370, 320, "Press A to continue...");
        while (gamepad_button_a() <= 0) end_frame();
    }
}
