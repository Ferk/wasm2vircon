#include <vircon.h>

int main(void)
{
    int frames = 0;

    while (1)
    {
        clear_screen(0x202040);

        print_at(220, 160, "Hello, Vircon32!");

        if (frames & 32)
            print_at(260, 200, "Compiled through WebAssembly");

        frames++;

        end_frame();
    }
}
