#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-Gamepads. */

static void draw_horizontal_line(int y)
{
    select_region(40);
    set_drawing_point(0, y);
    set_drawing_scale_bits(0x43A00000, 0x3F800000); /* 320.0f, 1.0f */
    draw_region_zoomed();
}

static void draw_vertical_line(int x)
{
    select_region(40);
    set_drawing_point(x, 0);
    set_drawing_scale_bits(0x3F800000, 0x43340000); /* 1.0f, 180.0f */
    draw_region_zoomed();
}

int main(void)
{
    static const struct vircon_region_matrix player_text = {10, 37, 137, 80, 164, 37, 137, 4, 1, 1};
    static const struct vircon_region_matrix arrows = {20, 1, 134, 16, 149, 1, 134, 2, 2, 1};
    static const struct vircon_region_matrix front_buttons = {30, 121, 114, 140, 133, 121, 114, 4, 1, 1};
    static const struct vircon_region_matrix shoulder_buttons = {34, 1, 114, 59, 132, 1, 114, 2, 1, 1};

    select_texture(0);
    select_region(0); set_region_minimum(1, 1); set_region_maximum(227, 112); set_region_hotspot(1, 1);
    define_region_matrix(&player_text);
    define_region_matrix(&arrows);
    define_region_matrix(&front_buttons);
    define_region_matrix(&shoulder_buttons);
    select_region(36); set_region_minimum(205, 114); set_region_maximum(225, 128); set_region_hotspot(205, 114);
    select_region(40); set_region_minimum(205, 130); set_region_maximum(206, 131); set_region_hotspot(205, 130);

    for (;;) {
        set_multiply_color(0xFFFFFFFF);
        clear_screen(0xFFDACA9D);
        draw_horizontal_line(179);
        draw_vertical_line(319);
        for (int gamepad_id = 0; gamepad_id < 4; ++gamepad_id) {
            int gamepad_x = 46;
            int gamepad_y = 46;
            select_gamepad(gamepad_id);
            if (gamepad_id & 1) gamepad_x = 367;
            if (gamepad_id & 2) gamepad_y = 227;
            select_region(10 + gamepad_id); draw_region_at(gamepad_x - 38, gamepad_y - 38);
            if (!gamepad_is_connected()) {
                set_multiply_color(0x60606060); select_region(0); draw_region_at(gamepad_x, gamepad_y); set_multiply_color(0xFFFFFFFF);
                continue;
            }
            select_region(0); draw_region_at(gamepad_x, gamepad_y);
            if (gamepad_direction_x() < 0) { select_region(20); draw_region_at(gamepad_x + 26, gamepad_y + 56); }
            if (gamepad_direction_x() > 0) { select_region(21); draw_region_at(gamepad_x + 54, gamepad_y + 56); }
            if (gamepad_direction_y() < 0) { select_region(22); draw_region_at(gamepad_x + 40, gamepad_y + 42); }
            if (gamepad_direction_y() > 0) { select_region(23); draw_region_at(gamepad_x + 40, gamepad_y + 70); }
            if (gamepad_button_a() > 0) { select_region(30); draw_region_at(gamepad_x + 188, gamepad_y + 54); }
            if (gamepad_button_b() > 0) { select_region(31); draw_region_at(gamepad_x + 169, gamepad_y + 73); }
            if (gamepad_button_x() > 0) { select_region(32); draw_region_at(gamepad_x + 169, gamepad_y + 35); }
            if (gamepad_button_y() > 0) { select_region(33); draw_region_at(gamepad_x + 150, gamepad_y + 54); }
            if (gamepad_button_l() > 0) { select_region(34); draw_region_at(gamepad_x + 4, gamepad_y + 5); }
            if (gamepad_button_r() > 0) { select_region(35); draw_region_at(gamepad_x + 164, gamepad_y + 5); }
            if (gamepad_button_start() > 0) { select_region(36); draw_region_at(gamepad_x + 103, gamepad_y + 82); }
        }
        end_frame();
    }
}
