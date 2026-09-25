#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-MemoryCard. The
 * original card ABI is word-addressed, so signatures and scene records use
 * i32 fields and card offsets/counts below are always words. */

enum {
    REGION_TILE = 10, REGION_DOG = 11, REGION_BONE = 12,
    REGION_DOG_HOUSE = 20, REGION_WINDOW = 30, REGION_CONTENTS_TEXT = 31,
    REGION_CONTENTS_INFO = 32, REGION_LOAD = 40, REGION_SAVE = 41,
    REGION_CANCEL = 42, REGION_EXIT = 43,
    TILE_SIZE = 80, TILES_X = 6, TILES_Y = 4,
    SIGNATURE_WORDS = 20, SCENE_WORDS = 5
};

struct point { int x, y; };
struct scene_data { struct point player, item; int score; };

static game_signature card_game_signature = {
    'M','E','M','O','R','Y','C','A','R','D','T','E','S','T'
};
static struct scene_data game_state;
static struct scene_data saved_game;
static char score_text[6];

/* Keep adjacent scene fields as ordinary i32 stores. Clang otherwise folds
 * this initialization into i64 stores, which are not part of VirconWasm. */
static void store_scene_value(volatile int *destination, int value)
    __attribute__((noinline));
static void store_scene_value(volatile int *destination, int value)
{
    *destination = value;
}

static void print_score(int x, int y, int score)
{
    int value = 10000 + score;
    int index;
    score_text[5] = 0;
    for (index = 4; index >= 0; --index) {
        score_text[index] = (char)('0' + value % 10);
        value /= 10;
    }
    print_at(x, y, &score_text[1]);
}

static void clamp(int *value, int minimum, int maximum)
{
    if (*value < minimum) *value = minimum;
    if (*value > maximum) *value = maximum;
}

static void reset_game_scene(void)
{
    store_scene_value(&game_state.player.x, 0);
    store_scene_value(&game_state.player.y, 2);
    store_scene_value(&game_state.item.x, 2);
    store_scene_value(&game_state.item.y, 2);
    store_scene_value(&game_state.score, 0);
}

static void create_new_item(void)
{
    game_state.item.x = rand() % TILES_X;
    game_state.item.y = rand() % TILES_Y;
}

static int text_length(const char *text)
{
    int length = 0;
    while (text[length] != 0) ++length;
    return length;
}

static void show_message(const char *text)
{
    select_region(REGION_WINDOW); draw_region_at(183, 116);
    select_region(REGION_EXIT); draw_region_at(287, 129);
    print_at(319 - text_length(text) * 5, 188, text);
    while (gamepad_button_start() != 1) end_frame();
}

static void draw_card_contents(void)
{
    select_region(REGION_CONTENTS_TEXT); draw_region_at(187, 158);
    if (card_is_empty()) print_at(362, 178, "EMPTY\nCARD!");
    else if (!card_signature_matches(&card_game_signature)) print_at(352, 178, "ANOTHER\n GAME!");
    else {
        select_region(REGION_CONTENTS_INFO); draw_region_at(321, 158);
        card_read_words((int *)&saved_game, SIGNATURE_WORDS, SCENE_WORDS);
        print_score(370, 197, saved_game.score);
    }
}

static void load_game_scene(void)
{
    if (!card_is_connected()) { show_message("NO CARD CONNECTED"); return; }
    if (card_is_empty()) { show_message("CARD IS EMPTY"); return; }
    if (!card_signature_matches(&card_game_signature)) { show_message("CARD CONTENTS INVALID"); return; }
    select_region(REGION_WINDOW); draw_region_at(183, 116);
    select_region(REGION_LOAD); draw_region_at(236, 129);
    select_region(REGION_CANCEL); draw_region_at(323, 129);
    draw_card_contents();
    end_frame();
    for (;;) {
        if (gamepad_button_start() == 1) return;
        if (gamepad_button_a() == 1) break;
        end_frame();
    }
    card_read_words((int *)&game_state, SIGNATURE_WORDS, SCENE_WORDS);
    show_message("GAME LOADED");
}

static void save_game_scene(void)
{
    if (!card_is_connected()) { show_message("NO CARD CONNECTED"); return; }
    select_region(REGION_WINDOW); draw_region_at(183, 116);
    select_region(REGION_SAVE); draw_region_at(236, 129);
    select_region(REGION_CANCEL); draw_region_at(323, 129);
    draw_card_contents();
    end_frame();
    for (;;) {
        if (gamepad_button_start() == 1) return;
        if (gamepad_button_b() == 1) break;
        end_frame();
    }
    card_write_signature(&card_game_signature);
    card_write_words((const int *)&game_state, SIGNATURE_WORDS, SCENE_WORDS);
    show_message("GAME SAVED");
}

static void draw_game_scene(void)
{
    int y;
    clear_screen(0xFF000000);
    select_region(REGION_LOAD); draw_region_at(33, 116);
    select_region(REGION_SAVE); draw_region_at(33, 143);
    select_region(REGION_DOG_HOUSE); draw_region_at(0, 187);
    select_region(REGION_TILE);
    for (y = 0; y < TILES_Y; ++y) {
        int x;
        for (x = 0; x < TILES_X; ++x)
            draw_region_at(142 + TILE_SIZE * x, 22 + TILE_SIZE * y);
    }
    print_score(19, 228, game_state.score);
    select_region(REGION_DOG);
    draw_region_at(142 + TILE_SIZE * game_state.player.x, 22 + TILE_SIZE * game_state.player.y);
    select_region(REGION_BONE);
    draw_region_at(142 + TILE_SIZE * game_state.item.x, 22 + TILE_SIZE * game_state.item.y);
}

static void define_regions(void)
{
    static const struct vircon_region_matrix tiles = { REGION_TILE, 277, 1, 356, 80, 277, 1, 3, 1, 1 };
    static const struct vircon_region_matrix actions = { REGION_LOAD, 420, 82, 506, 101, 420, 82, 1, 4, 1 };
    select_texture(0);
    define_region_matrix(&tiles);
    select_region(REGION_DOG_HOUSE); set_region_minimum(277, 82); set_region_maximum(418, 144); set_region_hotspot(277, 82);
    define_region_matrix(&actions);
    select_region(REGION_WINDOW); set_region_minimum(1, 1); set_region_maximum(275, 127); set_region_hotspot(1, 1);
    select_region(REGION_CONTENTS_TEXT); set_region_minimum(1, 129); set_region_maximum(134, 209); set_region_hotspot(1, 129);
    select_region(REGION_CONTENTS_INFO); set_region_minimum(136, 129); set_region_maximum(268, 209); set_region_hotspot(136, 129);
}

int main(void)
{
    define_regions(); select_gamepad(0); srand(get_time()); reset_game_scene();
    for (;;) {
        if (gamepad_left() == 1) --game_state.player.x;
        if (gamepad_right() == 1) ++game_state.player.x;
        if (gamepad_up() == 1) --game_state.player.y;
        if (gamepad_down() == 1) ++game_state.player.y;
        clamp(&game_state.player.x, 0, TILES_X - 1);
        clamp(&game_state.player.y, 0, TILES_Y - 1);
        if (game_state.player.x == game_state.item.x && game_state.player.y == game_state.item.y) {
            ++game_state.score; create_new_item();
        }
        if (gamepad_button_a() == 1) load_game_scene();
        else if (gamepad_button_b() == 1) save_game_scene();
        draw_game_scene(); end_frame();
    }
}
