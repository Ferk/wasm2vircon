// helloworld.c
//
// Minimal VirconWasm -> Vircon32 example.
// Draws a changing background so that successful execution
// is immediately visible in the emulator.

extern void vircon_set_background_color(int color);
extern void vircon_end_frame(void);

int main(void)
{
    int color = 0xFFFF40FF;

    while (1)
    {
        vircon_set_background_color(color);
        vircon_end_frame();
    }

    return 0;
}
