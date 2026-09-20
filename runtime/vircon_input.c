#include "vircon.h"
#include "vircon_platform.h"

void select_gamepad(int gamepad_id)
{
    vircon_input_select_gamepad(gamepad_id);
}

void gamepad_direction(int *delta_x, int *delta_y)
{
    *delta_x = gamepad_direction_x();
    *delta_y = gamepad_direction_y();
}

int gamepad_direction_x(void)
{
    if (vircon_input_gamepad_left() > 0)
        return -1;
    if (vircon_input_gamepad_right() > 0)
        return 1;
    return 0;
}

int gamepad_direction_y(void)
{
    if (vircon_input_gamepad_up() > 0)
        return -1;
    if (vircon_input_gamepad_down() > 0)
        return 1;
    return 0;
}

int gamepad_up(void) { return vircon_input_gamepad_up(); }
int gamepad_down(void) { return vircon_input_gamepad_down(); }

int gamepad_is_connected(void) { return vircon_input_gamepad_connected(); }
int gamepad_button_a(void) { return vircon_input_gamepad_button_a(); }
int gamepad_button_b(void) { return vircon_input_gamepad_button_b(); }
int gamepad_button_x(void) { return vircon_input_gamepad_button_x(); }
int gamepad_button_y(void) { return vircon_input_gamepad_button_y(); }
int gamepad_button_l(void) { return vircon_input_gamepad_button_l(); }
int gamepad_button_r(void) { return vircon_input_gamepad_button_r(); }
int gamepad_button_start(void) { return vircon_input_gamepad_button_start(); }

int get_frame_counter(void)
{
    return vircon_timer_get_frame_counter();
}
