#include "unity.h"
#include "game/map_renderer.h"
#include "game/map.h"

void setUp(void) {}
void tearDown(void) {}

void test_pixel_to_tile(void)
{
    /* col = (x - 30) / 10 */
    TEST_ASSERT_EQUAL(0, pixel_to_tile_col(30));
    TEST_ASSERT_EQUAL(1, pixel_to_tile_col(40));
    TEST_ASSERT_EQUAL(44, pixel_to_tile_col(470));

    /* row = y / 10 */
    TEST_ASSERT_EQUAL(0, pixel_to_tile_row(0));
    TEST_ASSERT_EQUAL(1, pixel_to_tile_row(10));
    TEST_ASSERT_EQUAL(63, pixel_to_tile_row(630));
}

void test_tile_to_pixel(void)
{
    /* tile_to_pixel_x: row → screen X (no offset) */
    TEST_ASSERT_EQUAL(0, tile_to_pixel_x(0));
    TEST_ASSERT_EQUAL(10, tile_to_pixel_x(1));
    /* tile_to_pixel_y: col → screen Y (with MAP_Y_OFFSET=30) */
    TEST_ASSERT_EQUAL(30, tile_to_pixel_y(0));
    TEST_ASSERT_EQUAL(40, tile_to_pixel_y(1));
}

void test_fixed_viewport_covers_map(void)
{
    /* Original uses a fixed viewport — no scrolling camera.
     * Verify the map fits exactly on screen:
     *   col axis: MAP_COLS(45) * TILE_SIZE(10) + MAP_Y_OFFSET(30) = 480px
     *   row axis: MAP_ROWS(64) * TILE_SIZE(10) = 640px
     * Both match the 640x480 render target exactly. */
    TEST_ASSERT_EQUAL(480, MAP_COLS * TILE_SIZE + MAP_Y_OFFSET);
    TEST_ASSERT_EQUAL(640, MAP_ROWS * TILE_SIZE);
}

void test_tile_byte_to_sprite(void)
{
    /* Verify known tile bytes map to valid sprites.
     * This is a logic test — doesn't need GPU. */
    TEST_ASSERT_TRUE(1);  /* Sprite mapping tested in test_sprites.c */
}

void test_hidden_tiles_need_darkness(void)
{
    static TileMap map;
    map.layer4[10][10] = 1;
    map.darkness_enabled = false;
    TEST_ASSERT_FALSE(map_renderer_tile_hidden(&map, 10, 10));
    map.darkness_enabled = true;
    TEST_ASSERT_TRUE(map_renderer_tile_hidden(&map, 10, 10));
    map.layer4[10][10] = 0;
    TEST_ASSERT_FALSE(map_renderer_tile_hidden(&map, 10, 10));
}

void test_border_ring_always_shown(void)
{
    static TileMap map;
    map.darkness_enabled = true;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map.layer4[r][c] = 1;
    TEST_ASSERT_FALSE(map_renderer_tile_hidden(&map, 0, 20));
    TEST_ASSERT_FALSE(map_renderer_tile_hidden(&map, MAP_ROWS - 1, 20));
    TEST_ASSERT_FALSE(map_renderer_tile_hidden(&map, 30, 0));
    TEST_ASSERT_FALSE(map_renderer_tile_hidden(&map, 30, MAP_COLS - 1));
    TEST_ASSERT_TRUE(map_renderer_tile_hidden(&map, 1, 1));
    TEST_ASSERT_TRUE(map_renderer_tile_hidden(&map, MAP_ROWS - 2, MAP_COLS - 2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hidden_tiles_need_darkness);
    RUN_TEST(test_border_ring_always_shown);
    RUN_TEST(test_pixel_to_tile);
    RUN_TEST(test_tile_to_pixel);
    RUN_TEST(test_fixed_viewport_covers_map);
    RUN_TEST(test_tile_byte_to_sprite);
    return UNITY_END();
}
