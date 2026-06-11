#include "unity.h"
#include "game/map.h"
#include <string.h>

static TileMap map;

void setUp(void) { memset(&map, 0, sizeof(map)); }
void tearDown(void) {}

void test_load_boulder(void)
{
    TEST_ASSERT_TRUE(map_load(&map, "assets/BOULDER.MNE"));

    /* Verify dimensions loaded (spot-check some tiles exist) */
    int nonzero = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map.tiles[r][c] != 0) nonzero++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, nonzero);
}

void test_border_tiles(void)
{
    TEST_ASSERT_TRUE(map_load(&map, "assets/BOULDER.MNE"));

    /* All border tiles should be '1' (solid wall) */
    for (int r = 0; r < MAP_ROWS; r++) {
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[r][0]);              /* left column */
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[r][MAP_COLS - 1]);   /* right column */
    }
    for (int c = 0; c < MAP_COLS; c++) {
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[0][c]);              /* top row */
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[MAP_ROWS - 1][c]);   /* bottom row */
    }
}

void test_load_level0(void)
{
    TEST_ASSERT_TRUE(map_load(&map, "assets/LEVEL0.MNL"));

    /* Should also have border walls */
    TEST_ASSERT_EQUAL_HEX8('1', map.tiles[0][0]);
    TEST_ASSERT_EQUAL_HEX8('1', map.tiles[MAP_ROWS - 1][MAP_COLS - 1]);
}

void test_get_tile_bounds(void)
{
    TEST_ASSERT_TRUE(map_load(&map, "assets/BOULDER.MNE"));

    /* In-bounds should return actual tile */
    TEST_ASSERT_EQUAL_HEX8('1', map_get_tile(&map, 0, 0));

    /* Out-of-bounds should return '0' */
    TEST_ASSERT_EQUAL_HEX8('0', map_get_tile(&map, -1, 0));
    TEST_ASSERT_EQUAL_HEX8('0', map_get_tile(&map, 0, -1));
    TEST_ASSERT_EQUAL_HEX8('0', map_get_tile(&map, MAP_ROWS, 0));
    TEST_ASSERT_EQUAL_HEX8('0', map_get_tile(&map, 0, MAP_COLS));
}

void test_collision_init(void)
{
    TEST_ASSERT_TRUE(map_load(&map, "assets/BOULDER.MNE"));
    map_init_collision(&map);

    /* Border walls ('1') should be indestructible (30000 in original) */
    TEST_ASSERT_EQUAL_HEX16(30000, map.collision[0][0]);

    /* Find a '0' (floor) tile and check it has 0 collision */
    bool found_floor = false;
    for (int r = 1; r < MAP_ROWS - 1 && !found_floor; r++) {
        for (int c = 1; c < MAP_COLS - 1 && !found_floor; c++) {
            if (map.tiles[r][c] == '0') {
                TEST_ASSERT_EQUAL_HEX16(0, map.collision[r][c]);
                found_floor = true;
            }
        }
    }
    TEST_ASSERT_TRUE(found_floor);

    /* All overlay values should be 0 */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            TEST_ASSERT_EQUAL_HEX16(0, map.overlay[r][c]);
        }
    }
}

void test_load_wrong_size(void)
{
    /* Try loading a file that's not 2970 bytes */
    TEST_ASSERT_FALSE(map_load(&map, "assets/FONTTI.FON"));
}

void test_load_nonexistent(void)
{
    TEST_ASSERT_FALSE(map_load(&map, "assets/NONEXISTENT.MNE"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_boulder);
    RUN_TEST(test_border_tiles);
    RUN_TEST(test_load_level0);
    RUN_TEST(test_get_tile_bounds);
    RUN_TEST(test_collision_init);
    RUN_TEST(test_load_wrong_size);
    RUN_TEST(test_load_nonexistent);
    return UNITY_END();
}
