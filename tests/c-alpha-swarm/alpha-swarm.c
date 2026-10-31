/* Verify Alpha Swarm count bounds and its effect on the GPU pixel budget. */
#include <vircon.h>
#include "alpha_swarm.h"

static int x, y;

/* Return zero when layer drawing consumes and can exhaust the budget. */
int main(void)
{
    int before, after, previous_x, saw_left = 0, saw_right = 0;
    reset_alpha_swarm();
    if (get_alpha_swarm_count() != 4)
        return 1;
    set_alpha_swarm_count(-1);
    if (get_alpha_swarm_count() != 0)
        return 2;
    set_alpha_swarm_count(1);
    before = get_gpu_remaining_pixels();
    draw_alpha_swarm();
    after = get_gpu_remaining_pixels();
    if (after < 0 || after >= before || get_multiply_color() != color_white)
        return 3;
    get_drawing_point(&x, &y);
    previous_x = x;
    for (int i = 0; i < 200; ++i) {
        update_alpha_swarm();
        draw_alpha_swarm();
        get_drawing_point(&x, &y);
        if (x < 114 || x > 526 || y < 194 || y > 246)
            return 4;
        if (x < previous_x) saw_left = 1;
        if (x > previous_x) saw_right = 1;
        previous_x = x;
    }
    if (!saw_left || !saw_right)
        return 5;
    set_alpha_swarm_count(1000);
    if (get_alpha_swarm_count() != 96)
        return 6;
    draw_alpha_swarm();
    return get_gpu_remaining_pixels() != -1;
}
