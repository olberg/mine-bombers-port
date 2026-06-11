#include "unity.h"
#include "game/sprites.h"

void setUp(void) {}
void tearDown(void) {}

void test_tile_to_sprite_mapping(void)
{
    /* Verify common tile bytes have valid sprite mappings after init.
     * We need a window for LoadTextureFromImage, so we init one briefly. */
    InitWindow(1, 1, "test");
    bool ok = sprites_init();
    if (!ok) {
        CloseWindow();
        TEST_IGNORE_MESSAGE("SIKA.SPY not available in assets/");
        return;
    }

    /* Basic terrain tiles must have mappings */
    TEST_ASSERT_TRUE(sprites_has_tile('0'));
    TEST_ASSERT_TRUE(sprites_has_tile('1'));
    TEST_ASSERT_TRUE(sprites_has_tile('2'));
    TEST_ASSERT_TRUE(sprites_has_tile('7'));
    TEST_ASSERT_TRUE(sprites_has_tile('A'));
    TEST_ASSERT_TRUE(sprites_has_tile('F'));

    /* Bomb tiles */
    TEST_ASSERT_TRUE(sprites_has_tile('W'));
    TEST_ASSERT_TRUE(sprites_has_tile('X'));
    TEST_ASSERT_TRUE(sprites_has_tile('Y'));

    /* Bomb stages */
    TEST_ASSERT_TRUE(sprites_has_tile('w'));
    TEST_ASSERT_TRUE(sprites_has_tile('x'));

    /* Pickups/items */
    TEST_ASSERT_TRUE(sprites_has_tile(0x92));
    TEST_ASSERT_TRUE(sprites_has_tile('s'));
    TEST_ASSERT_TRUE(sprites_has_tile('m'));
    TEST_ASSERT_TRUE(sprites_has_tile('k'));

    /* Floor variant */
    TEST_ASSERT_TRUE(sprites_has_tile(0xAF));

    /* Mine */
    TEST_ASSERT_TRUE(sprites_has_tile(0x9D));

    /* Invalid tile should return false */
    TEST_ASSERT_FALSE(sprites_has_tile(0xFF));

    sprites_cleanup();
    CloseWindow();
}

void test_sprite_dimensions(void)
{
    InitWindow(1, 1, "test");
    bool ok = sprites_init();
    if (!ok) {
        CloseWindow();
        TEST_IGNORE_MESSAGE("SIKA.SPY not available in assets/");
        return;
    }

    /* All mapped tiles should have 10x10 source rects */
    Rectangle r = sprites_get_tile_rect('0');
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.width);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.height);

    r = sprites_get_tile_rect('W');
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.width);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.height);

    /* Unknown tile should return floor rect (also 10x10) */
    r = sprites_get_tile_rect(0xFF);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.width);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.height);

    sprites_cleanup();
    CloseWindow();
}

void test_sprite_atlas_loads(void)
{
    InitWindow(1, 1, "test");
    bool ok = sprites_init();
    if (!ok) {
        CloseWindow();
        TEST_IGNORE_MESSAGE("SIKA.SPY not available in assets/");
        return;
    }

    Texture2D tex = sprites_get_atlas();
    TEST_ASSERT_GREATER_THAN(0, tex.id);
    TEST_ASSERT_EQUAL(640, tex.width);
    TEST_ASSERT_EQUAL(480, tex.height);

    sprites_cleanup();
    CloseWindow();
}

void test_player_sprite_rects(void)
{
    /* Verify player sprite rects match the decompiled coordinates.
     * Formula: X = x_base + direction * 40 + frame * 10, Y = y_base */

    /* Set 0 (P1 STD): x_base=160, y_base=10 */
    Rectangle r = sprites_get_player_rect(0, 0, 0);
    TEST_ASSERT_EQUAL_FLOAT(160.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.y);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.width);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.height);

    /* Set 0, band 1 (left), frame 2 */
    r = sprites_get_player_rect(0, 1, 2);
    TEST_ASSERT_EQUAL_FLOAT(160.0f + 40.0f + 20.0f, r.x);  /* 220 */
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.y);

    /* Set 0, band 3 (down), frame 3 */
    r = sprites_get_player_rect(0, 3, 3);
    TEST_ASSERT_EQUAL_FLOAT(160.0f + 120.0f + 30.0f, r.x);  /* 310 */
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.y);

    /* Set 1 (P2 STD): x_base=160, y_base=0 */
    r = sprites_get_player_rect(1, 0, 0);
    TEST_ASSERT_EQUAL_FLOAT(160.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.y);

    /* Set 5 (P2 ALT): x_base=0, y_base=200 */
    r = sprites_get_player_rect(5, 0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, r.y);

    /* Set 11 (Monster 4): x_base=0, y_base=80 */
    r = sprites_get_player_rect(11, 2, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.0f + 80.0f + 10.0f, r.x);  /* 90 */
    TEST_ASSERT_EQUAL_FLOAT(80.0f, r.y);

    /* Out-of-range clamping */
    r = sprites_get_player_rect(99, 0, 0);
    TEST_ASSERT_EQUAL_FLOAT(160.0f, r.x);  /* clamped to set 0 */
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.y);
}

void test_tile_sprite_coordinates(void)
{
    /* Verify key tile sprite coordinates match decompiled load_tile_resources()
     * (seg_1010:4609-4694) traced through draw_map_tile (seg_1010:4880-5250).
     * Format: load_sprite_from_sheet(stack, Y, X) → rect at (X, Y). */
    InitWindow(1, 1, "test");
    bool ok = sprites_init();
    if (!ok) {
        CloseWindow();
        TEST_IGNORE_MESSAGE("SIKA.SPY not available in assets/");
        return;
    }

    Rectangle r;

    /* Wall degradation tiles */
    r = sprites_get_tile_rect('5');  /* DAT_04ac: Y=0, X=50 (damaged wall group1) */
    TEST_ASSERT_EQUAL_FLOAT(50.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.y);

    r = sprites_get_tile_rect('6');  /* DAT_04b0: Y=0, X=60 (cracked wall group1) */
    TEST_ASSERT_EQUAL_FLOAT(60.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.y);

    r = sprites_get_tile_rect('p');  /* DAT_0534: Y=30, X=100 (damaged wall group2) */
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r.y);

    r = sprites_get_tile_rect('q');  /* DAT_0538: Y=30, X=90 (heavily damaged group2) */
    TEST_ASSERT_EQUAL_FLOAT(90.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r.y);

    /* Reinforced wall degradation stages */
    r = sprites_get_tile_rect(0xAC);  /* DAT_05d0: Y=70, X=0 */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(70.0f, r.y);

    r = sprites_get_tile_rect(0xAD);  /* DAT_05d4: Y=70, X=10 */
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(70.0f, r.y);

    r = sprites_get_tile_rect(0xAE);  /* DAT_05d8: Y=70, X=20 */
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(70.0f, r.y);

    /* Weapon signature tiles */
    r = sprites_get_tile_rect('a');  /* DAT_04fc: Y=10, X=100 */
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.y);

    r = sprites_get_tile_rect('g');  /* DAT_0514: Y=30, X=20 */
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r.y);

    /* Pickup tiles (stat gems) */
    r = sprites_get_tile_rect(0x8F);  /* DAT_0564: Y=20, X=100 */
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.y);

    r = sprites_get_tile_rect(0x92);  /* DAT_0570: Y=20, X=130 */
    TEST_ASSERT_EQUAL_FLOAT(130.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.y);

    /* Proximity mine, treasure, teleporter */
    r = sprites_get_tile_rect('o');  /* DAT_0530: Y=30, X=70 */
    TEST_ASSERT_EQUAL_FLOAT(70.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r.y);

    r = sprites_get_tile_rect('s');  /* DAT_053c: Y=30, X=150 */
    TEST_ASSERT_EQUAL_FLOAT(150.0f, r.x);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r.y);

    sprites_cleanup();
    CloseWindow();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sprite_atlas_loads);
    RUN_TEST(test_tile_to_sprite_mapping);
    RUN_TEST(test_sprite_dimensions);
    RUN_TEST(test_player_sprite_rects);
    RUN_TEST(test_tile_sprite_coordinates);
    return UNITY_END();
}
