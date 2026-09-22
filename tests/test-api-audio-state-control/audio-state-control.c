#include <vircon.h>

static float channel_speed;
static float global_volume;

/* Exercises direct SPU state reads and each newly exposed SPU command. */
int main(void)
{
    int result;

    select_sound(3);
    result = get_selected_sound();
    select_channel(4);
    result += get_selected_channel();

    set_channel_position(120);
    result += get_channel_position(4);
    channel_speed = get_channel_speed(4);
    global_volume = get_global_volume();

    stop_channel(4);
    pause_all_channels();
    stop_all_channels();
    resume_all_channels();
    return result;
}
