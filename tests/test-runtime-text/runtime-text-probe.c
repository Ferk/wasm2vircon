#include "vircon.h"

int main(void)
{
    for (;;) {
        clear_screen(0x00202040);
        print_at(220, 160, "A\nB");
        end_frame();
    }
}
