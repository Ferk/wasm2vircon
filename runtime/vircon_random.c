#include "vircon.h"
#include "vircon_platform.h"

int rand(void) { return vircon_rng_get_current_value(); }
void srand(int seed) { vircon_rng_set_current_value(seed); }
int get_time(void) { return vircon_timer_get_current_time(); }
int get_date(void) { return vircon_timer_get_current_date(); }

void sleep(int frames)
{
    int final_frame = get_frame_counter() + frames;
    while (get_frame_counter() < final_frame) end_frame();
}
