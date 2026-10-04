// BunnyMark for Vircon32: a port of Timofffee's Playdate bunnymark
// (github.com/Timofffee/bunnymark-playdate), itself raylib's textures_bunnymark
// by Ramon Santamaria.
//
// Press A to add a bunny, B a hundred, X five hundred, Y a thousand, each once when the
// button is let go; A + B takes them all away again. Every bunny moves and bounces off the
// box each frame; the screen shows the frame rate, how much of a frame's CPU time the last
// frame took, and the count. See README.md.
//
// The Playdate's movement code, floats and all (Vircon32's floats are full speed hardware),
// on the console's 640x360 screen, with the Playdate's box margins; the bunnies bounce at its
// edges. The bunny is the image at half size, 16x16, the size every port uses so their frame
// rates can be compared, at half the speed; the GPU draws it as a texture region, one draw_region_at
// per bunny.
//
// Vircon32 runs at a fixed 60 frames a second and end_frame() waits for the next one, so a
// frame that needs more than one frame's CPU time (250,000 cycles at 15 MHz) takes two:
// the frame rate falls to 30, 20, ... The CPU line shows how much of the budget is used,
// past 100% once the work takes more than a frame.
//

#include <vircon.h>

#define MAX_BUNNIES 100000
#define CYCLES_PER_FRAME 250000

#define REGION_BUNNY 0
#define REGION_BLACK 1

typedef int bool;
#define false 0
#define true 1

typedef struct
{
    float x;
    float y;
} Vector2;

typedef struct
{
    Vector2 position;
    Vector2 speed;
} Bunny;


static Bunny bunnies[MAX_BUNNIES];
static int bunniesCount = 0;
static bool pressed = false;


/* The box the bunnies bounce in */
#define BOX_X 5
#define BOX_Y 40
#define BOX_W 630
#define BOX_H 315

#define BUNNY_SIZE 16

#define GPU_PIXELS_PER_FRAME 2073600


static int fps = 0;
static int cpuPercent = 0;
static bool resetHeld = false;


/* ---------------------------------------------------------
   Bunny management
   --------------------------------------------------------- */

static void instantiateBunnies(int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (bunniesCount >= MAX_BUNNIES)
            break;

        Bunny *b = &bunnies[bunniesCount];

        b->position.x = 320.0f;
        b->position.y = 180.0f;

        /*
         * Playdate speeds halved.
         *
         * wasm2vircon provides rand()/srand() through vircon.h.
         */
        b->speed.x =
            (float)(rand() % 500 - 250) / 100.0f;

        b->speed.y =
            (float)(rand() % 500 - 250) / 100.0f;

        ++bunniesCount;
    }
}


static void updateBunnies(void)
{
    for (int i = 0; i < bunniesCount; ++i)
    {
        Bunny *b = &bunnies[i];

        b->position.x += b->speed.x;
        b->position.y += b->speed.y;

        if ((b->position.x > (BOX_X + BOX_W - BUNNY_SIZE)) ||
            (b->position.x < BOX_X))
        {
            b->speed.x *= -1.0f;
        }

        if ((b->position.y > (BOX_Y + BOX_H - BUNNY_SIZE)) ||
            (b->position.y < BOX_Y))
        {
            b->speed.y *= -1.0f;
        }
    }
}


/* ---------------------------------------------------------
   Drawing
   --------------------------------------------------------- */

static void drawBar(int x, int y, int w, int h)
{
    select_region(REGION_BLACK);

    set_drawing_scale(
        (float)w / 4.0f,
        (float)h / 4.0f
    );

    draw_region_zoomed_at(x, y);
}


static void drawRect(int x, int y, int w, int h)
{
    drawBar(x,         y,         w, 1);
    drawBar(x,         y + h - 1, w, 1);
    drawBar(x,         y,         1, h);
    drawBar(x + w - 1, y,         1, h);
}


static void drawBunnies(void)
{
    clear_screen(color_white);

    int rX = 0;
    int rY = 0;

    if (pressed)
    {
        rX = rand() % 4 - 2;
        rY = rand() % 4 - 2;
    }

    drawRect(
        BOX_X + rX,
        BOX_Y + rY,
        BOX_W,
        BOX_H
    );


    /*
     * IMPORTANT normal-C change:
     *
     * These are char buffers, not int buffers.
     *
     * wasm2vircon's strcpy(), strcat() and itoa()
     * operate on normal char * strings.
     */
    char text[64];
    char number[16];


    strcpy(text, "FPS ");

    itoa(fps, number, 10);
    strcat(text, number);

    strcat(text, "  CPU ");

    itoa(cpuPercent, number, 10);
    strcat(text, number);

    strcat(text, "%  GPU ");

    int gpuPercent =
        (640 * 360 + bunniesCount * 16 * 16)
        / (GPU_PIXELS_PER_FRAME / 100);

    itoa(gpuPercent, number, 10);
    strcat(text, number);

    strcat(text, "%");


    set_multiply_color(color_black);

    print_at(
        10 + rX,
        10 + rY,
        text
    );


    strcpy(text, "bunnies: ");

    itoa(bunniesCount, number, 10);
    strcat(text, number);

    print_at(
        400 + rX,
        10 + rY,
        text
    );


    set_multiply_color(color_white);


    /*
     * drawRect() selected REGION_BLACK,
     * so restore our bunny texture/region.
     */
    select_texture(0);
    select_region(REGION_BUNNY);


    for (int i = 0; i < bunniesCount; ++i)
    {
        draw_region_at(
            (int)bunnies[i].position.x,
            (int)bunnies[i].position.y
        );
    }
}


/* ---------------------------------------------------------
   Input
   --------------------------------------------------------- */

static bool releasedNow(int button)
{
    return button == -1;
}


static void checkButtons(void)
{
    int a = gamepad_button_a();
    int b = gamepad_button_b();
    int x = gamepad_button_x();
    int y = gamepad_button_y();


    if ((a > 0) && (b > 0))
    {
        bunniesCount = 0;
        resetHeld = true;
    }


    /*
     * Add bunnies once when the button is released.
     */
    if (!resetHeld)
    {
        if (releasedNow(a))
            instantiateBunnies(1);

        if (releasedNow(b))
            instantiateBunnies(100);
    }


    if ((a <= 0) && (b <= 0))
        resetHeld = false;


    if (releasedNow(x))
        instantiateBunnies(500);

    if (releasedNow(y))
        instantiateBunnies(1000);


    /*
     * Shake the box while any button is held.
     */
    pressed =
        (a > 0) ||
        (b > 0) ||
        (x > 0) ||
        (y > 0);
}


/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

void main(void)
{
    select_gamepad(0);

    srand(get_time());


    /*
     * Texture 0:
     *
     * bunny = pixels 0..15
     * black block = pixels 32..35
     */
    select_texture(0);

    select_region(REGION_BUNNY);
    define_region_topleft(
        0, 0,
        15, 15
    );

    select_region(REGION_BLACK);
    define_region_topleft(
        32, 0,
        35, 3
    );


    int frames = 0;
    int secondStart = get_frame_counter();


    while (true)
    {
        int startFrame =
            get_frame_counter();

        int startCycles =
            get_cycle_counter();


        checkButtons();
        updateBunnies();
        drawBunnies();


        /*
         * Calculate how many Vircon CPU cycles the work took.
         */
        int cycles =
            (get_frame_counter() - startFrame)
                * CYCLES_PER_FRAME
            + get_cycle_counter()
            - startCycles;

        cpuPercent =
            cycles * 100 / CYCLES_PER_FRAME;


        /*
         * FPS measured against Vircon's 60 Hz frame counter.
         */
        ++frames;

        int now = get_frame_counter();

        if (now - secondStart >= 60)
        {
            fps =
                frames * 60
                / (now - secondStart);

            frames = 0;
            secondStart = now;
        }


        end_frame();
    }
}