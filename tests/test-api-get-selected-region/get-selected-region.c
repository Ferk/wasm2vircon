#include <vircon.h>

/* Proves the public selected-region getter uses GPU state readback. */
int main(void)
{
    select_region(42);
    return get_selected_region();
}
