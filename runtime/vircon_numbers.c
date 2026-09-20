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

void print_int_at(int drawing_x, int drawing_y, int value)
{
    if (value < 0) {
        print_at(drawing_x, drawing_y, "-");
        drawing_x += 10;
        value = -value;
    }
    print_uint_at(drawing_x, drawing_y, (unsigned)value);
}

void print_fixed_2_at(int drawing_x, int drawing_y, float value)
{
    static char fraction[3] = {'0', '0', 0};
    int hundredths = (int)(value * 100.0f);
    unsigned magnitude;

    if (hundredths < 0) {
        print_at(drawing_x, drawing_y, "-");
        drawing_x += 10;
        hundredths = -hundredths;
    }
    magnitude = (unsigned)hundredths;
    print_uint_at(drawing_x, drawing_y, magnitude / 100u);
    print_at(drawing_x + 20, drawing_y, ".");
    fraction[0] = (char)('0' + (magnitude / 10u) % 10u);
    fraction[1] = (char)('0' + magnitude % 10u);
    print_at(drawing_x + 30, drawing_y, fraction);
}
