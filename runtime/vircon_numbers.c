#include "vircon.h"

static char digit_text[2] = {'0', 0};

static int print_digits(int x, int y, unsigned value)
{
    unsigned quotient = value / 10u;
    if (quotient != 0) x = print_digits(x, y, quotient);
    digit_text[0] = (char)('0' + value - quotient * 10u);
    print_at(x, y, digit_text);
    return x + 10;
}

void print_uint_at(int drawing_x, int drawing_y, unsigned value)
{
    (void)print_digits(drawing_x, drawing_y, value);
}
