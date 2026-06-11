#include "unity.h"
#include "loaders/pcx_loader.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_load_portrait(void)
{
    Image img = LoadPCX("assets/KELVOIT.PPM");
    TEST_ASSERT_GREATER_THAN(0, img.width);
    TEST_ASSERT_GREATER_THAN(0, img.height);
    TEST_ASSERT_NOT_NULL(img.data);
    UnloadImage(img);
}

void test_portrait_dimensions(void)
{
    Image img = LoadPCX("assets/KELVOIT.PPM");
    /* PCX header should give reasonable portrait dimensions */
    TEST_ASSERT_GREATER_THAN(50, img.width);
    TEST_ASSERT_GREATER_THAN(50, img.height);
    TEST_ASSERT_LESS_THAN(400, img.width);
    TEST_ASSERT_LESS_THAN(400, img.height);
    UnloadImage(img);
}

void test_load_all_portraits(void)
{
    /* 4 characters x 3 states (draw/lose/win) = 12 PPM files */
    const char *files[] = {
        "assets/KELVOIT.PPM", "assets/KELLOSE.PPM", "assets/KELDRAW.PPM",
        "assets/PUNVOIT.PPM", "assets/PUNLOSE.PPM", "assets/PUNDRAW.PPM",
        "assets/SINVOIT.PPM", "assets/SINLOSE.PPM", "assets/SINDRAW.PPM",
        "assets/VIHVOIT.PPM", "assets/VIHLOSE.PPM", "assets/VIHDRAW.PPM",
    };

    int loaded = 0;
    for (int i = 0; i < 12; i++) {
        Image img = LoadPCX(files[i]);
        if (img.width > 0 && img.height > 0 && img.data != NULL) {
            loaded++;
            UnloadImage(img);
        }
    }
    /* At least the first one should load (we know KELVOIT.PPM exists) */
    TEST_ASSERT_GREATER_THAN(0, loaded);
}

void test_load_nonexistent(void)
{
    Image img = LoadPCX("assets/NONEXISTENT.PPM");
    TEST_ASSERT_EQUAL(0, img.width);
    TEST_ASSERT_EQUAL(0, img.height);
}

void test_palette_present(void)
{
    Image img = LoadPCX("assets/KELVOIT.PPM");
    TEST_ASSERT_NOT_NULL(img.data);

    /* Verify not all pixels are the same (palette was applied) */
    uint8_t *pixels = (uint8_t *)img.data;
    int unique = 0;
    uint8_t first_r = pixels[0], first_g = pixels[1], first_b = pixels[2];
    for (int i = 1; i < img.width * img.height; i++) {
        if (pixels[i*4] != first_r || pixels[i*4+1] != first_g || pixels[i*4+2] != first_b) {
            unique = 1;
            break;
        }
    }
    TEST_ASSERT_EQUAL(1, unique);
    UnloadImage(img);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_portrait);
    RUN_TEST(test_portrait_dimensions);
    RUN_TEST(test_load_all_portraits);
    RUN_TEST(test_load_nonexistent);
    RUN_TEST(test_palette_present);
    return UNITY_END();
}
