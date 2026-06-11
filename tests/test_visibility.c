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
    RUN_TEST(test_visibility_out_of_bounds);
    RUN_TEST(test_visibility_dead_player_no_reveal);
    RUN_TEST(test_visibility_stopped_player_no_reveal);
    RUN_TEST(test_visibility_multiplayer_union);
    return UNITY_END();
}
