#include <vircon.h>

/* Normal-C port of ConsoleSoftware/TestPrograms/Test-TileMap.
 * TileMap is generated from the self-contained TMX asset by tiled2vircon and
 * imported as a little-endian i32 array by the ordinary build helper. */

#define FIRST_REGION_TILE_SET 0
#define REGION_ARROW_LEFT 100
#define REGION_ARROW_RIGHT 101
#define REGION_ARROW_UP 102
#define REGION_ARROW_DOWN 103

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define MAP_TILES_X 32
#define MAP_TILES_Y 18
#define TILE_SIZE 40

#define LEVEL_MIN_X 0
#define LEVEL_MIN_Y 0
#define LEVEL_MAX_X (MAP_TILES_X * TILE_SIZE)
#define LEVEL_MAX_Y (MAP_TILES_Y * TILE_SIZE)
#define CAMERA_MIN_X (LEVEL_MIN_X + SCREEN_WIDTH / 2)
#define CAMERA_MAX_X (LEVEL_MAX_X - SCREEN_WIDTH / 2)
#define CAMERA_MIN_Y (LEVEL_MIN_Y + SCREEN_HEIGHT / 2)
#define CAMERA_MAX_Y (LEVEL_MAX_Y - SCREEN_HEIGHT / 2)

extern const int TileMap[MAP_TILES_Y][MAP_TILES_X];

static const struct vircon_region_matrix tile_set_regions = {
    FIRST_REGION_TILE_SET, 0, 0, 39, 39, 0, 0, 8, 6, 0
};

static int max_i32(int a, int b)
{
    return a > b ? a : b;
}

static int min_i32(int a, int b)
{
    return a < b ? a : b;
}

int main(void)
{
    select_texture(0);
    select_gamepad(0);

    define_region_matrix(&tile_set_regions);

    select_region(REGION_ARROW_LEFT);
    set_region_minimum(0, 241);
    set_region_maximum(15, 272);
    set_region_hotspot(0, 256);

    select_region(REGION_ARROW_RIGHT);
    set_region_minimum(17, 241);
    set_region_maximum(32, 272);
    set_region_hotspot(32, 256);

    select_region(REGION_ARROW_UP);
    set_region_minimum(34, 241);
    set_region_maximum(65, 256);
    set_region_hotspot(49, 241);

    select_region(REGION_ARROW_DOWN);
    set_region_minimum(34, 258);
    set_region_maximum(65, 273);
    set_region_hotspot(49, 273);

    int camera_x = LEVEL_MAX_X / 2;
    int camera_y = LEVEL_MAX_Y / 2;

    for (;;) {
        int delta_x;
        int delta_y;
        int map_top_left_x;
        int map_top_left_y;
        int min_tile_x;
        int min_tile_y;
        int max_tile_x;
        int max_tile_y;
        int render_y;

        select_gamepad(0);
        delta_x = gamepad_direction_x();
        delta_y = gamepad_direction_y();

        camera_x += 2 * delta_x;
        camera_y += 2 * delta_y;
        camera_x = max_i32(camera_x, CAMERA_MIN_X);
        camera_x = min_i32(camera_x, CAMERA_MAX_X);
        camera_y = max_i32(camera_y, CAMERA_MIN_Y);
        camera_y = min_i32(camera_y, CAMERA_MAX_Y);

        map_top_left_x = -camera_x + SCREEN_WIDTH / 2;
        map_top_left_y = -camera_y + SCREEN_HEIGHT / 2;

        clear_screen(0xFF000000);
        set_multiply_color(0xFFFFFFFF);

        min_tile_x = max_i32(-map_top_left_x / TILE_SIZE, 0);
        min_tile_y = max_i32(-map_top_left_y / TILE_SIZE, 0);
        max_tile_x = min_i32((-map_top_left_x + SCREEN_WIDTH) / TILE_SIZE,
                             MAP_TILES_X - 1);
        max_tile_y = min_i32((-map_top_left_y + SCREEN_HEIGHT) / TILE_SIZE,
                             MAP_TILES_Y - 1);

        render_y = map_top_left_y + min_tile_y * TILE_SIZE;
        for (int tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
            int render_x = map_top_left_x + min_tile_x * TILE_SIZE;
            const int *current_tile = &TileMap[tile_y][min_tile_x];

            for (int tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
                select_region(FIRST_REGION_TILE_SET + *current_tile);
                draw_region_at(render_x, render_y);
                render_x += TILE_SIZE;
                ++current_tile;
            }
            render_y += TILE_SIZE;
        }

        if ((get_frame_counter() % 30) < 25) {
            if (camera_x > CAMERA_MIN_X) {
                set_multiply_color(delta_x < 0 ? 0xFF0000FF : 0xFFFFFFFF);
                select_region(REGION_ARROW_LEFT);
                draw_region_at(20, 180);
            }
            if (camera_x < CAMERA_MAX_X) {
                set_multiply_color(delta_x > 0 ? 0xFF0000FF : 0xFFFFFFFF);
                select_region(REGION_ARROW_RIGHT);
                draw_region_at(620, 180);
            }
            if (camera_y > CAMERA_MIN_Y) {
                set_multiply_color(delta_y < 0 ? 0xFF0000FF : 0xFFFFFFFF);
                select_region(REGION_ARROW_UP);
                draw_region_at(320, 20);
            }
            if (camera_y < CAMERA_MAX_Y) {
                set_multiply_color(delta_y > 0 ? 0xFF0000FF : 0xFFFFFFFF);
                select_region(REGION_ARROW_DOWN);
                draw_region_at(320, 340);
            }
        }

        end_frame();
    }
}
