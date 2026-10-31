/* Exercise the selected-region coordinate readers through the public C API. */
#include <vircon.h>

static int x, y;

/* Return zero when both coordinates match the selected region. */
int main(void)
{
    select_texture(-1);
    select_region(257);
    set_region_minimum(17, 29);
    get_region_minimum(&x, &y);
    return x != 17 || y != 29;
}
