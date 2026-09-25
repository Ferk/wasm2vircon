#include <vircon.h>

static volatile int integer_input = -7;
static volatile float float_input = -2.75f;

/* Exercises every remaining finite-math API through normal C calls. */
int main(void)
{
    int integer_result;
    float float_result;

    integer_result = min(integer_input, 3);
    integer_result += max(integer_input, 3);
    integer_result += abs(integer_input);

    float_result = fmod(float_input, 2.0f);
    float_result += fmin(float_input, 3.0f);
    float_result += fmax(float_input, 3.0f);
    float_result += fabs(float_input);
    float_result += floor(float_input);
    float_result += ceil(float_input);
    float_result += round(float_input);
    float_result += asin(0.5f);
    float_result += atan2(1.0f, 1.0f);
    float_result += sqrt(4.0f);

    clear_screen(integer_result);
    set_drawing_angle(float_result);
    return integer_result;
}
