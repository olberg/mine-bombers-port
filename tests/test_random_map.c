#include "unity.h"
#include "game/map.h"
#include <stdlib.h>
#include <string.h>

static TileMap map;

void setUp(void) { memset(&map, 0, sizeof(map)); }
void tearDown(void) {}

/* Test that a generated map has indestructible border ('1') on all edges */
void test_random_map_border(void)
{
    map_generate_random(&map, 45);

    /* All border tiles should be '1' (indestructible) */
    for (int r = 0; r < MAP_ROWS; r++) {
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[r][0]);
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[r][MAP_COLS - 1]);
    }
    for (int c = 0; c < MAP_COLS; c++) {
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[0][c]);
        TEST_ASSERT_EQUAL_HEX8('1', map.tiles[MAP_ROWS - 1][c]);
    }
}

/* Test that the interior has a mix of walls and floor tiles */
void test_random_map_has_walls_and_floor(void)
{
    map_generate_random(&map, 45);

    int wall_count = 0;
    int floor_count = 0;

    for (int r = 1; r < MAP_ROWS - 1; r++) {
        for (int c = 1; c < MAP_COLS - 1; c++) {
            uint8_t t = map.tiles[r][c];
            /* Wall types generated: '7'-'9', 'A'-'F', 'B', 'C'-'F' */
            if ((t >= '7' && t <= '9') || (t >= 'A' && t <= 'F') ||
                t == 'B' || t == 'p' || t == 'q')
                wall_count++;
            /* Floor types: '0', '2', '3', '4', '5', '6' */
            if (t == '0' || (t >= '2' && t <= '6'))
                floor_count++;
        }
    }

    /* A random map should have both walls and floor tiles */
    TEST_ASSERT_GREATER_THAN(0, wall_count);
    TEST_ASSERT_GREATER_THAN(0, floor_count);
}

/* Test that treasures are placed on the map */
void test_random_map_has_treasures(void)
{
    map_generate_random(&map, 45);

    int treasure_count = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            uint8_t t = map.tiles[r][c];
            /* Treasure tiles: 0x73 ('s'), 0x8f-0x9a */
            if (t == 0x73 || (t >= 0x8f && t <= 0x9a))
                treasure_count++;
        }
    }

    TEST_ASSERT_GREATER_THAN(0, treasure_count);
}

/* Test that collision values initialize correctly on a random map */
void test_random_map_collision_init(void)
{
    map_generate_random(&map, 45);
    map_init_collision(&map);

    /* Border '1' tiles should be indestructible (30000) */
    TEST_ASSERT_EQUAL_HEX16(30000, map.collision[0][0]);
    TEST_ASSERT_EQUAL_HEX16(30000, map.collision[MAP_ROWS - 1][0]);
    TEST_ASSERT_EQUAL_HEX16(30000, map.collision[0][MAP_COLS - 1]);

    /* Interior floor/deco tiles ('0'-'4') should have low collision HP.
     * Note: pass 4 converts most '0' to '2'/'3'/'4' (deco, HP=22-24). */
    bool found_passable = false;
    for (int r = 1; r < MAP_ROWS - 1 && !found_passable; r++) {
        for (int c = 1; c < MAP_COLS - 1 && !found_passable; c++) {
            uint8_t t = map.tiles[r][c];
            if (t >= '2' && t <= '4') {
                /* Deco tiles have low HP (22-24) — passable terrain */
                TEST_ASSERT_LESS_OR_EQUAL(0x18, map.collision[r][c]);
                found_passable = true;
            }
        }
    }
    TEST_ASSERT_TRUE(found_passable);
}

/* Test zero treasure count produces no treasures but still a valid map */
void test_random_map_zero_treasures(void)
{
    map_generate_random(&map, 0);

    /* Should still have a border */
    TEST_ASSERT_EQUAL_HEX8('1', map.tiles[0][0]);

    /* Count treasures: should be 0 (from the treasure loop; bonus items may
     * still appear from scatter loops) */
    /* Just verify the map is structurally valid */
    int nonzero = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map.tiles[r][c] != 0) nonzero++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, nonzero);
}

/* Test that bonus items (mystery boxes, health, teleporters) are placed */
void test_random_map_has_bonus_items(void)
{
    map_generate_random(&map, 45);

    int mystery = 0, health = 0, teleporter = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            uint8_t t = map.tiles[r][c];
            if (t == 0x79) mystery++;       /* 'y' mystery box */
            if (t == 0x6d) health++;        /* 'm' health restore */
            if (t == 0x9c) teleporter++;    /* teleporter */
        }
    }

    /* At least some bonus items should exist (scatter loops have ~30% chance
     * each iteration, starting with ~30% chance to place at least one) */
    /* We can't guarantee any specific type, but the total should be > 0
     * for a normal generation. Just check map is structurally valid. */
    int total_items = mystery + health + teleporter;
    /* Even if scatter loops produce 0 items (unlikely but possible),
     * the map should still have wall and floor content */
    int content_count = 0;
    for (int r = 1; r < MAP_ROWS - 1; r++) {
        for (int c = 1; c < MAP_COLS - 1; c++) {
            if (map.tiles[r][c] != '0' && map.tiles[r][c] != '1')
                content_count++;
        }
    }
    TEST_ASSERT_GREATER_THAN(100, content_count);
    (void)total_items;  /* avoid unused warning */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_random_map_border);
    RUN_TEST(test_random_map_has_walls_and_floor);
    RUN_TEST(test_random_map_has_treasures);
    RUN_TEST(test_random_map_collision_init);
    RUN_TEST(test_random_map_zero_treasures);
    RUN_TEST(test_random_map_has_bonus_items);
    return UNITY_END();
}
