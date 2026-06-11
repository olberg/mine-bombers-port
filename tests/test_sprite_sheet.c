#include "unity.h"
#include "loaders/spy_loader.h"
#include "loaders/sprite_sheet.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

void test_extract_cursor_region(void)
{
    uint8_t palette[768];
    uint8_t *indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    TEST_ASSERT_NOT_NULL(indexed);

    Image sheet = LoadSPY("assets/SIKA.SPY", palette, indexed);
    TEST_ASSERT_EQUAL(640, sheet.width);

    /* Extract cursor region: (150, 140) size 65x20 */
    Image cursor = ExtractSprite(indexed, SPY_WIDTH, SPY_HEIGHT, 150, 140, 65, 20, palette);
    TEST_ASSERT_EQUAL(65, cursor.width);
    TEST_ASSERT_EQUAL(20, cursor.height);
    TEST_ASSERT_NOT_NULL(cursor.data);

    /* Should have some non-zero pixels */
    uint8_t *rgba = (uint8_t *)cursor.data;
    int nonzero = 0;
    for (int i = 0; i < 65 * 20 * 4; i++) {
        if (rgba[i] != 0) { nonzero++; break; }
    }
    TEST_ASSERT_GREATER_THAN(0, nonzero);

    UnloadImage(cursor);
    free(indexed);
    UnloadImage(sheet);
}

void test_extract_bounds_check(void)
{
    uint8_t pixels[100];
    uint8_t palette[768];
    memset(pixels, 0, sizeof(pixels));
    memset(palette, 0, sizeof(palette));

    /* Out-of-bounds extraction should return empty image */
    Image img = ExtractSprite(pixels, 10, 10, 5, 5, 10, 10, palette);
    TEST_ASSERT_EQUAL(0, img.width);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_extract_cursor_region);
    RUN_TEST(test_extract_bounds_check);
    return UNITY_END();
}
