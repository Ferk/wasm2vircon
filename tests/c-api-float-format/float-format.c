/* Check fixed-decimal placement and the ordinary C ftoa text formatter. */
#include <vircon.h>

static int point_x, point_y;
static char text[32];

/* Print a value and compare the last glyph's position with the expected x. */
static int last_glyph_x(float value, int expected_x)
{
    print_fixed_2_at(20, 20, value);
    get_drawing_point(&point_x, &point_y);
    return point_x == expected_x && point_y == 20;
}

/* Exercise different integer widths, the negative sign, and ftoa rounding. */
int main(void)
{
    if (!last_glyph_x(1.25f, 50) || !last_glyph_x(10.25f, 60) ||
        !last_glyph_x(100.25f, 70) || !last_glyph_x(-0.05f, 60) ||
        !last_glyph_x(-0.001f, 60))
        return 1;
    ftoa(1.025f, text);
    if (strcmp(text, "1.025") != 0)
        return 2;
    ftoa(-0.001f, text);
    if (strcmp(text, "-0.001") != 0)
        return 3;
    ftoa(1.999999f, text);
    if (strcmp(text, "2") != 0)
        return 4;
    ftoa(0.00001f, text);
    if (strcmp(text, "0.00001") != 0)
        return 5;
    return 0;
}
