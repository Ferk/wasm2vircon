#include <vircon.h>

static float direction_x;
static float direction_y;
static date_info current_date;
static time_info current_time;

/* Exercises the public input and timer helpers, including pure C decoding. */
int main(void)
{
    int result;

    select_gamepad(2);
    result = get_selected_gamepad();
    gamepad_direction_normalized(&direction_x, &direction_y);
    result += (int)direction_x + (int)direction_y;

    translate_date(get_date(), &current_date);
    translate_time(get_time(), &current_time);
    result += get_cycle_counter();
    return result + current_date.year + current_date.month + current_date.day
        + current_time.hours + current_time.minutes + current_time.seconds;
}
