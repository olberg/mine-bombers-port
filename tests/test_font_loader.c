#include "unity.h"
#include "loaders/font_loader.h"

void setUp(void) {}
void tearDown(void) {}

void test_load_fon(void)
{
    BitmapFont font = LoadFON("assets/FONTTI.FON", false);

    TEST_ASSERT_EQUAL(128, font.atlas_img.width);
    TEST_ASSERT_EQUAL(128, font.atlas_img.height);
    TEST_ASSERT_EQUAL(8, font.glyph_w);
    TEST_ASSERT_EQUAL(8, font.glyph_h);
    TEST_ASSERT_NOT_NULL(font.atlas_img.data);

    UnloadFON(&font);
}

void test_font_has_visible_glyphs(void)
{
    BitmapFont font = LoadFON("assets/FONTTI.FON", false);

    /* 'A' (0x41) should have some non-transparent pixels */
    uint8_t *rgba = (uint8_t *)font.atlas_img.data;
    int ch = 'A';
    int grid_x = (ch % 16) * 8;
    int grid_y = (ch / 16) * 8;

    int nonzero = 0;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int px = grid_x + col;
            int py = grid_y + row;
            int idx = (py * 128 + px) * 4;
            if (rgba[idx + 3] != 0) nonzero++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, nonzero);

    UnloadFON(&font);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_fon);
    RUN_TEST(test_font_has_visible_glyphs);
    return UNITY_END();
}
