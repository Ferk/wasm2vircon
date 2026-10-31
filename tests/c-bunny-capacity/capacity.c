/* Check the benchmark's complete bunny capacity and clamping behavior. */
#include "bunnies.h"

/* Return zero only if the documented limit and both clamps work. */
int main(void)
{
    reset_bunnies();
    set_bunny_count(9999);
    if (get_bunny_count() != 9999)
        return 1;
    set_bunny_count(10000);
    if (get_bunny_count() != 9999)
        return 2;
    set_bunny_count(-1);
    return get_bunny_count() != 0;
}
