/* Exercise count repeat timing, slow updates, reversal, and frame wraparound. */
#include "count_repeat.h"

/* Return zero only if each press and elapsed interval produces the right steps. */
int main(void)
{
    CountRepeat state = {0};
    if (count_repeat_steps(&state, 0, 99) != 0 || count_repeat_steps(&state, 1, 100) != 1 ||
        count_repeat_steps(&state, 1, 117) != 0 || count_repeat_steps(&state, 1, 118) != 1 ||
        count_repeat_steps(&state, 1, 124) != 1 || count_repeat_steps(&state, 1, 142) != 3 ||
        count_repeat_steps(&state, 1, 142) != 0 || count_repeat_steps(&state, 0, 143) != 0 ||
        count_repeat_steps(&state, -1, 144) != -1 || count_repeat_steps(&state, -1, 162) != -1)
        return 1;
    if (count_repeat_steps(&state, 1, 0xfffffffau) != 1 || count_repeat_steps(&state, 1, 12) != 1 ||
        count_repeat_steps(&state, -1, 13) != -1 || count_repeat_steps(&state, -1, 10000) != -100)
        return 2;
    {
        CountRepeat vertical = {0}, horizontal = {0};
        if (count_repeat_steps(&vertical, 1, 10) != 1 || count_repeat_steps(&horizontal, -1, 15) != -1 ||
            count_repeat_steps(&vertical, 1, 28) != 1 || count_repeat_steps(&horizontal, -1, 28) != 0 ||
            count_repeat_steps(&horizontal, -1, 33) != -1 || count_repeat_steps(&vertical, 0, 33) != 0 ||
            count_repeat_steps(&horizontal, -1, 39) != -1)
            return 3;
    }
    return 0;
}
