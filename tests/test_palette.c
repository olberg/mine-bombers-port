#include "unity.h"
#include "gfx/palette.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_fade_in_reaches_target(void)
{
    uint8_t pal[768];
    memset(pal, 0, 768);
    /* Set a few known 8-bit values */
    pal[0] = 255; pal[1] = 128; pal[2] = 64;
    pal[3] = 0;   pal[4] = 255; pal[5] = 200;

    palette_init(pal);
    palette_start_fade_in(7);

    TEST_ASSERT_TRUE(palette_is_fading());

    /* Step 7 times */
    for (int i = 0; i < 7; i++) {
        palette_update();
    }

    TEST_ASSERT_FALSE(palette_is_fading());

    /* After full fade in, color 0 should match target (8-bit direct) */
    Color c0 = palette_get_color(0);
    TEST_ASSERT_EQUAL_UINT8(255, c0.r);
    TEST_ASSERT_EQUAL_UINT8(128, c0.g);
    TEST_ASSERT_EQUAL_UINT8(64, c0.b);
}

void test_fade_out_reaches_black(void)
{
    uint8_t pal[768];
    memset(pal, 0, 768);
    pal[0] = 255; pal[1] = 200; pal[2] = 100;

    palette_init(pal);
    palette_start_fade_out(7);

    for (int i = 0; i < 7; i++) {
        palette_update();
    }

    TEST_ASSERT_FALSE(palette_is_fading());

    Color c0 = palette_get_color(0);
    TEST_ASSERT_EQUAL_UINT8(0, c0.r);
    TEST_ASSERT_EQUAL_UINT8(0, c0.g);
    TEST_ASSERT_EQUAL_UINT8(0, c0.b);
}

void test_fade_in_intermediate_step(void)
{
    uint8_t pal[768];
    memset(pal, 0, 768);
    pal[0] = 210; /* target R for color 0 */

    palette_init(pal);
    palette_start_fade_in(7);

    /* After 1 step: (210/7)*1 = 30 */
    palette_update();
    Color c = palette_get_color(0);
    TEST_ASSERT_EQUAL_UINT8(30, c.r);
}

void test_apply_to_pixels(void)
{
    uint8_t pal[768];
    memset(pal, 0, 768);
    pal[3] = 255; /* color 1 R = 255 */
    pal[4] = 0;
    pal[5] = 0;

    palette_init(pal);

    uint8_t indexed[4] = {0, 1, 1, 0};
    uint8_t rgba[16];
    palette_apply_to_pixels(indexed, rgba, 4);

    /* Pixel 0 = color 0 = (0,0,0) */
    TEST_ASSERT_EQUAL_UINT8(0, rgba[0]);
    /* Pixel 1 = color 1 = (255,0,0) */
    TEST_ASSERT_EQUAL_UINT8(255, rgba[4]);
    TEST_ASSERT_EQUAL_UINT8(0, rgba[5]);
    /* Alpha always 255 */
    TEST_ASSERT_EQUAL_UINT8(255, rgba[3]);
    TEST_ASSERT_EQUAL_UINT8(255, rgba[7]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fade_in_reaches_target);
    RUN_TEST(test_fade_out_reaches_black);
    RUN_TEST(test_fade_in_intermediate_step);
    RUN_TEST(test_apply_to_pixels);
    return UNITY_END();
}
