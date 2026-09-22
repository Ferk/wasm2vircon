#include <vircon.h>

/* Exercises both public region-definition forms through existing GPU writes. */
int main(void)
{
    int value = 1;
    define_region(value++, value++, value++, value++, value++, value++);
    define_region_topleft(10, 20, 30, 40);
    return value;
}
