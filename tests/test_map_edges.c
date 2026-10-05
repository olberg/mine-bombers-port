#include "unity.h"
#include "game/map_edges.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static EdgeState st;
static TileMap map;

static void fill(uint8_t tile)
{
    memset(&map, 0, sizeof(map));
    memset(map.tiles, tile, sizeof(map.tiles));
    map_edges_reset(&st);
}

static int layers_at(int row, int col, uint8_t *out)
{
    return map_edges_tile_layers(&st, row, col, out);
}

static bool has_layer(int row, int col, int sprite)
{
    uint8_t l[EDGE_MAX_LAYERS];
    int n = layers_at(row, col, l);
    for (int i = 0; i < n; i++)
        if (l[i] == sprite) return true;
    return false;
}

static int count_layers(int row, int col)
{
    uint8_t l[EDGE_MAX_LAYERS];
    return layers_at(row, col, l);
}

#define PLAIN_SAND(side) (EDGE_GROUP_PLAIN_SAND * 4 + (side))
#define PLAIN_ROCK(side) (EDGE_GROUP_PLAIN_ROCK * 4 + (side))
#define LIT_SAND(side)   (EDGE_GROUP_LIT_SAND * 4 + (side))
#define LIT_ROCK(side)   (EDGE_GROUP_LIT_ROCK * 4 + (side))

void test_opening_set(void)
{
    TEST_ASSERT_TRUE(map_edges_tile_opens('0'));
    TEST_ASSERT_TRUE(map_edges_tile_opens('f'));
    TEST_ASSERT_TRUE(map_edges_tile_opens('B'));
    TEST_ASSERT_TRUE(map_edges_tile_opens(0xAF));
    TEST_ASSERT_TRUE(map_edges_tile_opens(0x7F));   /* placed bomb */
    TEST_ASSERT_TRUE(map_edges_tile_opens(0x92));   /* treasure */
    TEST_ASSERT_FALSE(map_edges_tile_opens('1'));
    TEST_ASSERT_FALSE(map_edges_tile_opens('2'));
    TEST_ASSERT_FALSE(map_edges_tile_opens('C'));
    TEST_ASSERT_FALSE(map_edges_tile_opens(0x84));  /* fire */
    TEST_ASSERT_FALSE(map_edges_tile_opens('q'));
}

void test_plain_sprite_classes(void)
{
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_LEFT),   map_edges_plain_sprite('2', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_RIGHT),  map_edges_plain_sprite('4', EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_TOP),    map_edges_plain_sprite('6', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_BOTTOM), map_edges_plain_sprite('3', EDGE_BOTTOM));
    TEST_ASSERT_EQUAL_INT(PLAIN_ROCK(EDGE_LEFT),   map_edges_plain_sprite('C', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(PLAIN_ROCK(EDGE_BOTTOM), map_edges_plain_sprite('F', EDGE_BOTTOM));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_plain_sprite('0', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_plain_sprite('1', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_plain_sprite('B', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_plain_sprite(0x84, EDGE_RIGHT));
}

void test_plain_sprite_corner_pieces(void)
{
    /* '7'-'A' only take strips on the sides that are not their solid corner */
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_LEFT),   map_edges_plain_sprite('7', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('7', EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_TOP),    map_edges_plain_sprite('7', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('7', EDGE_BOTTOM));

    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('8', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_RIGHT),  map_edges_plain_sprite('8', EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_TOP),    map_edges_plain_sprite('8', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('8', EDGE_BOTTOM));

    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('9', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_RIGHT),  map_edges_plain_sprite('9', EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('9', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_BOTTOM), map_edges_plain_sprite('9', EDGE_BOTTOM));

    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_LEFT),   map_edges_plain_sprite('A', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('A', EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(-1,                      map_edges_plain_sprite('A', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(PLAIN_SAND(EDGE_BOTTOM), map_edges_plain_sprite('A', EDGE_BOTTOM));
}

void test_lit_sprite_classes(void)
{
    TEST_ASSERT_EQUAL_INT(LIT_SAND(EDGE_LEFT),   map_edges_lit_sprite('2', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(LIT_SAND(EDGE_BOTTOM), map_edges_lit_sprite('6', EDGE_BOTTOM));
    TEST_ASSERT_EQUAL_INT(LIT_ROCK(EDGE_RIGHT),  map_edges_lit_sprite('7', EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(LIT_ROCK(EDGE_TOP),    map_edges_lit_sprite('A', EDGE_TOP));
    TEST_ASSERT_EQUAL_INT(LIT_ROCK(EDGE_LEFT),   map_edges_lit_sprite('E', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_lit_sprite('B', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_lit_sprite('0', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_lit_sprite('1', EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(-1, map_edges_lit_sprite('q', EDGE_LEFT));
}

void test_sprite_rectangles(void)
{
    const EdgeSpriteInfo *s = map_edges_sprite_info(PLAIN_SAND(EDGE_LEFT));
    TEST_ASSERT_EQUAL_INT(194, s->src_x);
    TEST_ASSERT_EQUAL_INT(98,  s->src_y);
    TEST_ASSERT_EQUAL_INT(4,   s->w);
    TEST_ASSERT_EQUAL_INT(10,  s->h);
    TEST_ASSERT_EQUAL_INT(0,   s->dx);

    s = map_edges_sprite_info(PLAIN_ROCK(EDGE_BOTTOM));
    TEST_ASSERT_EQUAL_INT(148, s->src_x);
    TEST_ASSERT_EQUAL_INT(75,  s->src_y);
    TEST_ASSERT_EQUAL_INT(10,  s->w);
    TEST_ASSERT_EQUAL_INT(3,   s->h);
    TEST_ASSERT_EQUAL_INT(7,   s->dy);

    s = map_edges_sprite_info(LIT_ROCK(EDGE_RIGHT));
    TEST_ASSERT_EQUAL_INT(211, s->src_x);
    TEST_ASSERT_EQUAL_INT(6,   s->dx);

    TEST_ASSERT_NULL(map_edges_sprite_info(-1));
    TEST_ASSERT_NULL(map_edges_sprite_info(EDGE_SPRITE_COUNT));
}

void test_start_round_paints_around_openings(void)
{
    fill('C');
    map.tiles[10][10] = '0';
    map_edges_start_round(&st, &map);

    /* row+1 neighbour gets its low-row side, row-1 the high-row side,
     * col+1 the low-col side, col-1 the high-col side */
    TEST_ASSERT_TRUE(has_layer(11, 10, PLAIN_ROCK(EDGE_LEFT)));
    TEST_ASSERT_TRUE(has_layer(9, 10,  PLAIN_ROCK(EDGE_RIGHT)));
    TEST_ASSERT_TRUE(has_layer(10, 11, PLAIN_ROCK(EDGE_TOP)));
    TEST_ASSERT_TRUE(has_layer(10, 9,  PLAIN_ROCK(EDGE_BOTTOM)));
    TEST_ASSERT_EQUAL_INT(1, count_layers(11, 10));
    TEST_ASSERT_EQUAL_INT(0, count_layers(11, 11));
    TEST_ASSERT_EQUAL_INT(0, count_layers(10, 10));
}

void test_start_round_sand_next_to_floor(void)
{
    fill('0');
    map.tiles[5][5] = '3';
    map_edges_start_round(&st, &map);
    TEST_ASSERT_TRUE(has_layer(5, 5, PLAIN_SAND(EDGE_LEFT)));
    TEST_ASSERT_TRUE(has_layer(5, 5, PLAIN_SAND(EDGE_RIGHT)));
    TEST_ASSERT_TRUE(has_layer(5, 5, PLAIN_SAND(EDGE_TOP)));
    TEST_ASSERT_TRUE(has_layer(5, 5, PLAIN_SAND(EDGE_BOTTOM)));
    TEST_ASSERT_EQUAL_INT(4, count_layers(5, 5));
}

void test_start_round_nothing_in_darkness(void)
{
    fill('C');
    map.tiles[10][10] = '0';
    map.darkness_enabled = true;
    map_edges_start_round(&st, &map);
    TEST_ASSERT_EQUAL_INT(0, count_layers(11, 10));
    TEST_ASSERT_EQUAL_INT(0, count_layers(9, 10));
}

void test_start_round_bounds(void)
{
    fill('0');
    map.tiles[0][0] = '2';
    map.tiles[MAP_ROWS - 1][MAP_COLS - 1] = '2';
    map_edges_start_round(&st, &map);
    TEST_ASSERT_TRUE(has_layer(0, 0, PLAIN_SAND(EDGE_RIGHT)));
    TEST_ASSERT_TRUE(has_layer(0, 0, PLAIN_SAND(EDGE_BOTTOM)));
    TEST_ASSERT_EQUAL_INT(2, count_layers(0, 0));
    TEST_ASSERT_TRUE(has_layer(MAP_ROWS - 1, MAP_COLS - 1, PLAIN_SAND(EDGE_LEFT)));
    TEST_ASSERT_TRUE(has_layer(MAP_ROWS - 1, MAP_COLS - 1, PLAIN_SAND(EDGE_TOP)));
    TEST_ASSERT_EQUAL_INT(2, count_layers(MAP_ROWS - 1, MAP_COLS - 1));
    TEST_ASSERT_EQUAL_INT(0, map_edges_tile_layers(&st, -1, 0, (uint8_t[8]){0}));
    TEST_ASSERT_EQUAL_INT(0, map_edges_tile_layers(&st, 0, MAP_COLS, (uint8_t[8]){0}));
}

void test_update_primes_on_first_call(void)
{
    fill('C');
    map.tiles[10][10] = '0';
    map_edges_update(&st, &map);
    TEST_ASSERT_TRUE(has_layer(11, 10, PLAIN_ROCK(EDGE_LEFT)));
}

void test_update_dig_paints_neighbours(void)
{
    fill('0');
    map.tiles[10][10] = '2';
    map.tiles[10][11] = 'C';
    map_edges_update(&st, &map);
    /* the sand tile had floor around it; dig the rock tile below it */
    map.tiles[10][11] = '0';
    map_edges_update(&st, &map);
    TEST_ASSERT_TRUE(has_layer(10, 10, PLAIN_SAND(EDGE_BOTTOM)));
}

void test_update_dig_of_sand_clears_own_strips(void)
{
    fill('0');
    map.tiles[10][10] = '2';
    map_edges_update(&st, &map);
    TEST_ASSERT_EQUAL_INT(4, count_layers(10, 10));
    map.tiles[10][10] = '0';
    map_edges_update(&st, &map);
    TEST_ASSERT_EQUAL_INT(0, count_layers(10, 10));
}

void test_update_burnt_out_fire_paints_nothing(void)
{
    fill('0');
    map.tiles[10][10] = 0x84;
    map.tiles[11][10] = '2';
    map_edges_update(&st, &map);
    /* primed with fire already on the map: the sand has no strip on that side */
    TEST_ASSERT_FALSE(has_layer(11, 10, PLAIN_SAND(EDGE_LEFT)));
    map.tiles[10][10] = '0';
    map_edges_update(&st, &map);
    TEST_ASSERT_FALSE(has_layer(11, 10, PLAIN_SAND(EDGE_LEFT)));
}

void test_update_fire_paints_lit_strips(void)
{
    fill('0');
    map.tiles[11][10] = '2';   /* row+1 */
    map.tiles[9][10]  = 'C';   /* row-1 */
    map.tiles[10][11] = '5';   /* col+1 */
    map.tiles[10][9]  = '1';   /* col-1: not a lit class */
    map_edges_update(&st, &map);
    map.tiles[10][10] = 0x84;
    map_edges_update(&st, &map);

    TEST_ASSERT_TRUE(has_layer(11, 10, LIT_SAND(EDGE_LEFT)));
    TEST_ASSERT_TRUE(has_layer(9, 10,  LIT_ROCK(EDGE_RIGHT)));
    TEST_ASSERT_TRUE(has_layer(10, 11, LIT_SAND(EDGE_TOP)));
    TEST_ASSERT_EQUAL_INT(0, count_layers(10, 9));
}

void test_update_lit_strip_goes_on_top_of_plain(void)
{
    fill('0');
    map.tiles[11][10] = '2';
    map_edges_update(&st, &map);
    uint8_t l[EDGE_MAX_LAYERS];
    int n = layers_at(11, 10, l);
    TEST_ASSERT_EQUAL_INT(4, n);

    map.tiles[10][10] = 0x84;
    map_edges_update(&st, &map);
    n = layers_at(11, 10, l);
    TEST_ASSERT_EQUAL_INT(5, n);
    TEST_ASSERT_EQUAL_UINT8(LIT_SAND(EDGE_LEFT), l[n - 1]);
    TEST_ASSERT_TRUE(has_layer(11, 10, PLAIN_SAND(EDGE_LEFT)));
}

void test_update_repainting_moves_strip_to_top(void)
{
    fill('0');
    map.tiles[10][10] = '2';
    map_edges_update(&st, &map);
    uint8_t l[EDGE_MAX_LAYERS];
    TEST_ASSERT_EQUAL_INT(4, layers_at(10, 10, l));
    TEST_ASSERT_NOT_EQUAL(PLAIN_SAND(EDGE_LEFT), l[3]);

    /* dig the tile at row-1 again: its plain LEFT strip is repainted */
    map.tiles[9][10] = 'C';
    map_edges_update(&st, &map);
    map.tiles[9][10] = '0';
    map_edges_update(&st, &map);
    int n = layers_at(10, 10, l);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_UINT8(PLAIN_SAND(EDGE_LEFT), l[n - 1]);
}

void test_update_changed_neighbour_wipes_strips_in_same_frame(void)
{
    fill('0');
    map.tiles[10][10] = '2';
    map.tiles[10][11] = '2';
    map_edges_update(&st, &map);
    /* both sand tiles turn to fire together: neither keeps lit strips from
     * the other */
    map.tiles[10][10] = 0x84;
    map.tiles[10][11] = 0x84;
    map_edges_update(&st, &map);
    TEST_ASSERT_EQUAL_INT(0, count_layers(10, 10));
    TEST_ASSERT_EQUAL_INT(0, count_layers(10, 11));
}

void test_update_fire_from_mine_centre_is_not_lit(void)
{
    fill('0');
    map.tiles[10][10] = 0xA4;
    map.tiles[11][10] = '2';
    map_edges_update(&st, &map);
    map.tiles[10][10] = 0x84;
    map_edges_update(&st, &map);
    TEST_ASSERT_FALSE(has_layer(11, 10, LIT_SAND(EDGE_LEFT)));
}

void test_update_second_fire_tile_does_not_relight(void)
{
    fill('0');
    map.tiles[11][10] = '2';
    map.tiles[10][10] = 0x85;
    map_edges_update(&st, &map);
    map.tiles[10][10] = 0x84;
    map_edges_update(&st, &map);
    TEST_ASSERT_FALSE(has_layer(11, 10, LIT_SAND(EDGE_LEFT)));
}

void test_update_reveal_wipes_strips(void)
{
    fill('0');
    map.darkness_enabled = true;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map.layer4[r][c] = 1;
    map.tiles[11][10] = '2';
    map_edges_update(&st, &map);

    /* dug tile paints onto the hidden neighbour... */
    map.tiles[10][10] = 'C';
    map_edges_update(&st, &map);
    map.tiles[10][10] = '0';
    map_edges_update(&st, &map);
    TEST_ASSERT_TRUE(has_layer(11, 10, PLAIN_SAND(EDGE_LEFT)));

    /* ...and revealing the neighbour redraws it without strips */
    map.layer4[11][10] = 0;
    map_edges_update(&st, &map);
    TEST_ASSERT_EQUAL_INT(0, count_layers(11, 10));
}

void test_update_hidden_flag_ignored_without_darkness(void)
{
    fill('0');
    map.tiles[11][10] = '2';
    map_edges_update(&st, &map);
    map.layer4[11][10] = 1;
    map_edges_update(&st, &map);
    map.layer4[11][10] = 0;
    map_edges_update(&st, &map);
    TEST_ASSERT_EQUAL_INT(4, count_layers(11, 10));
}

void test_dig_source_tiles(void)
{
    TEST_ASSERT_TRUE(map_edges_tile_is_dig_source('2'));
    TEST_ASSERT_TRUE(map_edges_tile_is_dig_source('q'));
    TEST_ASSERT_TRUE(map_edges_tile_is_dig_source('p'));
    TEST_ASSERT_TRUE(map_edges_tile_is_dig_source('C'));
    TEST_ASSERT_FALSE(map_edges_tile_is_dig_source('B'));
    TEST_ASSERT_FALSE(map_edges_tile_is_dig_source('l'));
    TEST_ASSERT_FALSE(map_edges_tile_is_dig_source(0x84));
    TEST_ASSERT_FALSE(map_edges_tile_is_dig_source('0'));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_opening_set);
    RUN_TEST(test_plain_sprite_classes);
    RUN_TEST(test_plain_sprite_corner_pieces);
    RUN_TEST(test_lit_sprite_classes);
    RUN_TEST(test_sprite_rectangles);
    RUN_TEST(test_start_round_paints_around_openings);
    RUN_TEST(test_start_round_sand_next_to_floor);
    RUN_TEST(test_start_round_nothing_in_darkness);
    RUN_TEST(test_start_round_bounds);
    RUN_TEST(test_update_primes_on_first_call);
    RUN_TEST(test_update_dig_paints_neighbours);
    RUN_TEST(test_update_dig_of_sand_clears_own_strips);
    RUN_TEST(test_update_burnt_out_fire_paints_nothing);
    RUN_TEST(test_update_fire_paints_lit_strips);
    RUN_TEST(test_update_lit_strip_goes_on_top_of_plain);
    RUN_TEST(test_update_repainting_moves_strip_to_top);
    RUN_TEST(test_update_changed_neighbour_wipes_strips_in_same_frame);
    RUN_TEST(test_update_fire_from_mine_centre_is_not_lit);
    RUN_TEST(test_update_second_fire_tile_does_not_relight);
    RUN_TEST(test_update_reveal_wipes_strips);
    RUN_TEST(test_update_hidden_flag_ignored_without_darkness);
    RUN_TEST(test_dig_source_tiles);
    return UNITY_END();
}
