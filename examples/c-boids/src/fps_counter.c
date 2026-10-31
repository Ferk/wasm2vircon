/* Frame-window FPS measurement for complete update and drawing iterations. */
#include "fps_counter.h"

enum { WINDOW_FRAMES = 60, FPS_HUNDREDTHS_PER_SECOND = 6000 };

/* Default to 60 FPS while the first measurement window is filling. */
void fps_counter_reset(FpsCounter *counter, unsigned frame)
{
    counter->start_frame = frame;
    counter->completed = 0;
    counter->hundredths = FPS_HUNDREDTHS_PER_SECOND;
}

/* Count the finished iteration, including any display frames spent drawing. */
void fps_counter_complete(FpsCounter *counter, unsigned frame)
{
    unsigned elapsed = frame - counter->start_frame;
    ++counter->completed;
    if (elapsed >= WINDOW_FRAMES) {
        counter->hundredths = counter->completed * FPS_HUNDREDTHS_PER_SECOND / elapsed;
        counter->start_frame = frame;
        counter->completed = 0;
    }
}
