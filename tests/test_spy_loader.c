#include "unity.h"
#include "loaders/spy_loader.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

void test_load_titlebe_spy(void)
{
    uint8_t palette[768];
    Image img = LoadSPY("assets/TITLEBE.SPY", palette, NULL);

    TEST_ASSERT_EQUAL(640, img.width);
    TEST_ASSERT_EQUAL(480, img.height);
    TEST_ASSERT_NOT_NULL(img.data);

    /* Palette should not be all zeros */
    int nonzero = 0;
    for (int i = 0; i < 768; i++) {
        if (palette[i] != 0) nonzero++;
    }
    TEST_ASSERT_GREATER_THAN(0, nonzero);

    UnloadImage(img);
}

void test_load_main3_spy(void)
{
    uint8_t palette[768];
    Image img = LoadSPY("assets/MAIN3.SPY", palette, NULL);

    TEST_ASSERT_EQUAL(640, img.width);
    TEST_ASSERT_EQUAL(480, img.height);
    TEST_ASSERT_NOT_NULL(img.data);

    UnloadImage(img);
}

void test_load_shapet_spy(void)
{
    uint8_t palette[768];
    Image img = LoadSPY("assets/SHAPET.SPY", palette, NULL);

    TEST_ASSERT_EQUAL(640, img.width);
    TEST_ASSERT_EQUAL(480, img.height);
    TEST_ASSERT_NOT_NULL(img.data);

    UnloadImage(img);
}

void test_indexed_output(void)
{
    uint8_t palette[768];
    uint8_t *indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    TEST_ASSERT_NOT_NULL(indexed);

    Image img = LoadSPY("assets/TITLEBE.SPY", palette, indexed);
    TEST_ASSERT_EQUAL(640, img.width);

    /* Indexed pixels should have some non-zero values */
    int nonzero = 0;
    for (int i = 0; i < SPY_WIDTH * SPY_HEIGHT; i++) {
        if (indexed[i] != 0) { nonzero++; break; }
    }
    TEST_ASSERT_GREATER_THAN(0, nonzero);

    /* All indices should be 0-15 (4-bit color from bitplanes) */
    for (int i = 0; i < SPY_WIDTH * SPY_HEIGHT; i++) {
        TEST_ASSERT_LESS_OR_EQUAL(15, indexed[i]);
    }

    free(indexed);
    UnloadImage(img);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_titlebe_spy);
    RUN_TEST(test_load_main3_spy);
    RUN_TEST(test_load_shapet_spy);
    RUN_TEST(test_indexed_output);
    return UNITY_END();
}
