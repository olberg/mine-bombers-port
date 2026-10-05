#include "unity.h"
#include "game/visibility.h"
#include "game/movement.h"
#include "game/map.h"
#include "game/player.h"
#include <string.h>

/* Stub: player globals needed by visibility.c */
int g_num_active_players = 1;

void setUp(void) {}
void tearDown(void) {}

/* Helper: create a minimal map with all floor tiles */
static void init_floor_map(TileMap *map)
{
    memset(map, 0, sizeof(TileMap));
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            map->tiles[row][col] = '0';
        }
    }
}

/* Helper: create a player at a tile position.
 * VGA convention: x_pos = row * TILE_SIZE, y_pos = col * TILE_SIZE + MAP_Y_OFFSET. */
static Player make_player(int tile_col, int tile_row, int direction)
{
    Player p;
    memset(&p, 0, sizeof(Player));
    p.x_pos = tile_row * TILE_SIZE;
    p.y_pos = tile_col * TILE_SIZE + MAP_Y_OFFSET;
    /* The fan reads the CURRENT direction (+0xA4), not facing */
    p.direction = (uint8_t)direction;
    p.last_direction = (uint8_t)direction;
    p.dead = 0;
    return p;
}

/* Test: visibility_init marks all tiles as hidden */
void test_visibility_init_all_hidden(void)
{
    TileMap map;
    init_floor_map(&map);

    visibility_init(&map);

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            TEST_ASSERT_FALSE_MESSAGE(
                visibility_is_revealed(&map, row, col),
                "All tiles should be hidden after visibility_init");
        }
    }
}

/* Test: visibility_reveal_tile clears hidden bit */
void test_visibility_reveal_tile(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 10, 10));

    visibility_reveal_tile(&map, 10, 10);

    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 10));
    /* Neighboring tiles still hidden */
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 10, 11));
}

/* Test: visibility_reveal_tile preserves other layer4 bits (e.g., bit 2 = gate marker) */
void test_visibility_preserves_other_bits(void)
{
    TileMap map;
    init_floor_map(&map);
    map.layer4[5][5] = 0x05;  /* bit 0 (hidden) + bit 2 (gate marker) */

    visibility_reveal_tile(&map, 5, 5);

    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 5, 5));
    /* bit 2 should still be set */
    TEST_ASSERT_EQUAL_HEX8(0x04, map.layer4[5][5]);
}

/* Test: visibility_reveal_player reveals tiles around the player */
void test_visibility_reveal_player_reveals_nearby(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    Player p = make_player(22, 32, DIR_DOWN);
    visibility_reveal_player(&map, &p);

    /* Player's own tile should be revealed */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 32, 22));

    /* Immediate neighbors should be revealed */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 31, 22));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 33, 22));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 32, 21));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 32, 23));
}

/* Test: visibility rays extend in the facing direction */
void test_visibility_rays_extend_in_facing_direction(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    /* Place player at row=10, col=22, facing DOWN.
     * DOWN extends along +col axis (higher col values). */
    Player p = make_player(22, 10, DIR_DOWN);
    visibility_reveal_player(&map, &p);

    /* Tiles ahead (higher col, same row) should be revealed */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 27));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 32));

    /* Tiles behind (lower col, beyond immediate neighbors) should NOT be revealed */
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 10, 15));
}

/* Test: walls block line-of-sight */
void test_visibility_walls_block_los(void)
{
    TileMap map;
    init_floor_map(&map);

    /* Place a wall at row=10, col=30 (ahead of player along +col axis) */
    map.tiles[10][30] = '1';  /* indestructible wall */

    visibility_init(&map);

    /* Player at row=10, col=22, facing DOWN (+col) */
    Player p = make_player(22, 10, DIR_DOWN);
    visibility_reveal_player(&map, &p);

    /* Tiles between player and wall should be revealed */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 26));

    /* The wall tile itself should be revealed (you can see the wall) */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 30));

    /* Tiles beyond the wall should NOT be revealed (wall blocks LOS) */
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 10, 38));
}

/* Test: visibility_tile_blocks_los returns correct values */
void test_visibility_tile_blocks_los(void)
{
    /* Non-blocking tiles */
    TEST_ASSERT_FALSE(visibility_tile_blocks_los('0'));
    TEST_ASSERT_FALSE(visibility_tile_blocks_los('f'));
    TEST_ASSERT_FALSE(visibility_tile_blocks_los(0xAF));
    TEST_ASSERT_FALSE(visibility_tile_blocks_los('k'));

    /* Blocking tiles */
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('1'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('7'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('8'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('A'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('B'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los(0xAC));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('l'));

    /* The set at seg_1000:4A31: sand, monsters, fire and the two gaps in
     * the pickup range stop a ray; bombs and treasures do not. */
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('2'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('3'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('4'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los('G'));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los(0x84));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los(0x9B));
    TEST_ASSERT_TRUE(visibility_tile_blocks_los(0xA4));
    TEST_ASSERT_FALSE(visibility_tile_blocks_los('W'));
    TEST_ASSERT_FALSE(visibility_tile_blocks_los(0x8A));
    TEST_ASSERT_FALSE(visibility_tile_blocks_los(0xB5));
}

/* Helper: a map of solid wall with the listed tiles opened to floor */
static void init_wall_map(TileMap *map, const int (*floor)[2], int count)
{
    memset(map, 0, sizeof(TileMap));
    memset(map->tiles, '1', sizeof(map->tiles));
    for (int i = 0; i < count; i++)
        map->tiles[floor[i][0]][floor[i][1]] = '0';
}

/* Test: sand is revealed by the ray that hits it and hides what is behind */
void test_visibility_sand_stops_the_ray(void)
{
    static const int corridor[][2] = {
        {32, 22}, {33, 22}, {34, 22}, {35, 22}, {36, 22}, {37, 22},
    };
    TileMap map;
    init_wall_map(&map, corridor, 6);
    map.tiles[35][22] = '2';
    visibility_init(&map);

    Player p = make_player(22, 32, DIR_RIGHT);
    visibility_reveal_player(&map, &p);

    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 34, 22));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 35, 22));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 36, 22));
}

/* Test: the line walk of FUN_1000_4a51. Toward a target 20 tiles ahead
 * and 10 to the side the numerator starts at 10, so the steps alternate
 * diagonal, straight, diagonal, straight. */
void test_visibility_ray_follows_the_original_line_walk(void)
{
    static const int path[][2] = {
        {32, 22}, {33, 21}, {34, 21}, {35, 20}, {36, 20}, {37, 19},
    };
    TileMap map;
    init_wall_map(&map, path, 6);
    visibility_init(&map);

    Player p = make_player(22, 32, DIR_RIGHT);
    visibility_reveal_player(&map, &p);

    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 37, 19));
    TEST_ASSERT_TRUE_MESSAGE(visibility_is_revealed(&map, 38, 19),
        "The wall that ends the path is revealed");
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 39, 19));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 39, 18));
}

/* Test: ray targets past the far map edge are not pulled back onto it,
 * so the fan keeps its 45-degree sides next to the edge */
void test_visibility_fan_keeps_its_angle_at_the_far_edge(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    Player p = make_player(22, 55, DIR_RIGHT);
    visibility_reveal_player(&map, &p);

    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 62, 15));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 62, 29));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 62, 6));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 62, 38));
}

/* Test: out-of-bounds tile access returns not revealed */
void test_visibility_out_of_bounds(void)
{
    TileMap map;
    init_floor_map(&map);

    TEST_ASSERT_FALSE(visibility_is_revealed(&map, -1, 0));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, MAP_ROWS, 0));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 0, -1));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 0, MAP_COLS));
}

/* Test: dead player doesn't reveal tiles */
void test_visibility_dead_player_no_reveal(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    Player p = make_player(22, 32, DIR_DOWN);
    p.dead = 1;

    visibility_reveal_player(&map, &p);

    /* Player's tile should still be hidden */
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 32, 22));
}

/* Test: stopped player casts no fan — the original returns without
 * revealing when +0xA4 is not 1-4 (seg_1000:3212-3214) */
void test_visibility_stopped_player_no_reveal(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    Player p = make_player(22, 32, DIR_STOP);
    visibility_reveal_player(&map, &p);

    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 32, 22));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 33, 22));
}

/* Two players' fans accumulate in the SAME shared layer4 — the
 * original has no per-player visibility; MP combination is the union of
 * everyone's reveals over time. */
void test_visibility_multiplayer_union(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    Player p1 = make_player(10, 10, DIR_RIGHT);  /* fan toward +row */
    Player p2 = make_player(30, 50, DIR_LEFT);   /* fan toward -row */

    visibility_reveal_player(&map, &p1);
    visibility_reveal_player(&map, &p2);

    /* Both players' own tiles and forward tiles are revealed in the one map */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 10));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 15, 10));   /* ahead of P1 */
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 50, 30));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 45, 30));   /* ahead of P2 */

    /* A tile far from both fans stays hidden */
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 60, 5));
}

/* Test: a tile whose contents change while dark is revealed; the rest stay hidden */
void test_visibility_reveal_changed_reveals_only_changed_tiles(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    visibility_reveal_changed(&map);
    TEST_ASSERT_FALSE_MESSAGE(visibility_is_revealed(&map, 10, 10),
        "Nothing changed, so nothing is revealed");

    map.tiles[10][10] = 0x84;   /* explosion */
    map.tiles[30][20] = 'W';    /* bomb placed */
    visibility_reveal_changed(&map);

    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 10, 10));
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 30, 20));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 10, 11));
    TEST_ASSERT_FALSE(visibility_is_revealed(&map, 31, 20));
}

/* Test: a change is reported once; later changes to the same tile reveal again */
void test_visibility_reveal_changed_tracks_latest_contents(void)
{
    TileMap map;
    init_floor_map(&map);
    visibility_init(&map);

    map.tiles[5][5] = 0x84;
    visibility_reveal_changed(&map);
    map.layer4[5][5] |= 0x01;   /* hide it again by hand */

    visibility_reveal_changed(&map);
    TEST_ASSERT_FALSE_MESSAGE(visibility_is_revealed(&map, 5, 5),
        "An unchanged tile is not revealed again");

    map.tiles[5][5] = 0x61;     /* next decay stage */
    visibility_reveal_changed(&map);
    TEST_ASSERT_TRUE(visibility_is_revealed(&map, 5, 5));
}

/* Test: revealing changed tiles keeps the other layer4 bits */
void test_visibility_reveal_changed_preserves_other_bits(void)
{
    TileMap map;
    init_floor_map(&map);
    map.layer4[7][7] = 0x02;    /* shop gate marker */
    visibility_init(&map);

    map.tiles[7][7] = 'f';
    visibility_reveal_changed(&map);

    TEST_ASSERT_EQUAL_HEX8(0x02, map.layer4[7][7]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_visibility_init_all_hidden);
    RUN_TEST(test_visibility_reveal_tile);
    RUN_TEST(test_visibility_preserves_other_bits);
    RUN_TEST(test_visibility_reveal_player_reveals_nearby);
    RUN_TEST(test_visibility_rays_extend_in_facing_direction);
    RUN_TEST(test_visibility_walls_block_los);
    RUN_TEST(test_visibility_tile_blocks_los);
    RUN_TEST(test_visibility_sand_stops_the_ray);
    RUN_TEST(test_visibility_ray_follows_the_original_line_walk);
    RUN_TEST(test_visibility_fan_keeps_its_angle_at_the_far_edge);
    RUN_TEST(test_visibility_out_of_bounds);
    RUN_TEST(test_visibility_dead_player_no_reveal);
    RUN_TEST(test_visibility_stopped_player_no_reveal);
    RUN_TEST(test_visibility_multiplayer_union);
    RUN_TEST(test_visibility_reveal_changed_reveals_only_changed_tiles);
    RUN_TEST(test_visibility_reveal_changed_tracks_latest_contents);
    RUN_TEST(test_visibility_reveal_changed_preserves_other_bits);
    return UNITY_END();
}
