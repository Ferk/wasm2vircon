#include "vircon.h"
#include "vircon_platform.h"

void clear_screen(int color)
{
    vircon_set_background_color(color);
}

void end_frame(void)
{
    vircon_end_frame();
}
