#include <vircon.h>

static const int month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
static void digit(int base, unsigned value, int x, int y) { select_region(base + value % 10u); draw_region_at(x, y); }

int main(void)
{
    static const struct vircon_region_matrix large = {10,1,255,40,313,1,255,10,1,1};
    static const struct vircon_region_matrix small = {20,1,315,20,346,1,315,10,1,1};
    static const struct vircon_region_matrix date = {30,1,348,26,383,1,348,10,1,1};
    static const struct vircon_region_matrix months = {40,1,385,82,420,1,385,4,3,1};
    select_texture(0); select_region(0); set_region_minimum(1,1); set_region_maximum(362,253); set_region_hotspot(1,1);
    /* Region 1 is the 1x31 table strip, hot-spotted at its bottom; region 2
     * is the full 6x32 seconds colon. Both are separate from the digit grids. */
    select_region(1); set_region_minimum(364,1); set_region_maximum(364,31); set_region_hotspot(364,31);
    select_region(2); set_region_minimum(211,315); set_region_maximum(216,346); set_region_hotspot(211,315);
    define_region_matrix(&large); define_region_matrix(&small); define_region_matrix(&date); define_region_matrix(&months);
    assign_channel_sound(1,0); set_channel_volume(.2f); set_channel_speed(1.1892f);
    assign_channel_sound(0,0); set_channel_volume(.25f);
    for (;;) {
      unsigned time=(unsigned)get_time(), hour=time/3600u, minute=(time%3600u)/60u, second=time%60u, raw=(unsigned)get_date(), year=raw/65536u, days=raw&65535u, month=1, day; int frame=get_frame_counter(), i;
      for(i=0;i<11 && days >= (unsigned)month_days[i];++i) { days-=(unsigned)month_days[i]; month++; } day=days+1;
      clear_screen(0xFF000000);
      select_region(1); set_drawing_scale(640.0f, 1.0f); set_drawing_point(0,359); draw_region_zoomed();
      select_region(0); draw_region_at(138,76);
      digit(10,hour/10u,195,125); digit(10,hour%10u,238,125); digit(10,minute/10u,304,125); digit(10,minute%10u,347,125); digit(20,second/10u,405,152); digit(20,second%10u,427,152);
      digit(30,year/1000u,353,229); digit(30,(year/100u)%10u,381,229); digit(30,(year/10u)%10u,409,229); digit(30,year%10u,437,229); select_region(40+month-1); draw_region_at(250,229); digit(30,day/10u,175,229); digit(30,day%10u,203,229);
      if ((frame%60)>=30) { select_region(2); draw_region_at(393,152); }
      if ((frame%60)==0) play_channel((int)(second%2u)); end_frame();
    }
}
