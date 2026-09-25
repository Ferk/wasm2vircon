#include <vircon.h>

/* Faithful normal-C port of ConsoleSoftware/TestPrograms/Test-MinimalTest.
 * The original custom header's centered hotspot evaluates to (69, 49). */
int main(void)
{
    select_texture(0);
    select_region(0);
    define_region_center(0, 0, 139, 99);

    clear_screen(0xFFFF00FF);
    draw_region_at(640 / 2, 360 / 2);
    play_sound_in_channel(0, 0);

    return 0;
}
