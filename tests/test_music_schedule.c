/*
 * Characterization tests for the original music scheduling.
 *
 * Ground truth:
 *  - 14-byte gameplay order table at 0x1038:0x0010 (MB.EXE file offset
 *    195856): 1 5 15 22 32 39 43 53 56 62 68 76 80 83 (1-based orders).
 *  - Shop jump target 0x54 = 84 (seg_1000:7100).
 *  - Order pick: table[Random(14)] with the Borland Pascal LCG
 *    (seg_1000:7124, FUN_1030_19de).
 *  - OEKU.S3M has 92 orders (90 real + 2 end markers), so order 84 is
 *    reachable; HUIPPE.S3M has 24, so a stray shop jump while the menu
 *    module is loaded must be rejected (FUN_1018_0855 range check).
 */
#include "unity.h"
#include "game/music_schedule.h"
#include "util/prng.h"
#include <xmp.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static const unsigned char EXPECTED_TABLE[14] = {
    1, 5, 15, 22, 32, 39, 43, 53, 56, 62, 68, 76, 80, 83
};

static void test_game_order_table_matches_data_segment(void)
{
    const unsigned char *t = music_schedule_game_orders();
    TEST_ASSERT_EQUAL_UINT8_ARRAY(EXPECTED_TABLE, t, 14);
}

static void test_shop_order_is_0x54(void)
{
    TEST_ASSERT_EQUAL_INT(0x54, music_schedule_shop_order());
}

/* Independent reimplementation of Borland Random(n) so the schedule cannot
 * "agree with itself" through mb_random. */
static uint32_t bp_seed;
static int bp_random(int n)
{
    bp_seed = bp_seed * 0x08088405u + 1;
    return (int)(((uint64_t)bp_seed * (uint32_t)n) >> 32);
}

static void test_gameplay_order_consumes_borland_random14(void)
{
    static const uint32_t seeds[] = { 1, 2, 0xDEADBEEF, 12345, 0 };
    for (unsigned s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
        mb_prng_set_seed(seeds[s]);
        bp_seed = seeds[s];
        for (int i = 0; i < 50; i++) {
            int expected = EXPECTED_TABLE[bp_random(14)];
            TEST_ASSERT_EQUAL_INT(expected, music_schedule_gameplay_order());
        }
    }
}

static void test_same_seed_same_order_sequence(void)
{
    int first[20], second[20];
    mb_prng_set_seed(777);
    for (int i = 0; i < 20; i++) first[i] = music_schedule_gameplay_order();
    mb_prng_set_seed(777);
    for (int i = 0; i < 20; i++) second[i] = music_schedule_gameplay_order();
    TEST_ASSERT_EQUAL_INT_ARRAY(first, second, 20);
}

/* Asset ground truth: every scheduled order must be valid for OEKU.S3M
 * (1 <= order <= order count) and the shop jump must be reachable in OEKU
 * but NOT in HUIPPE (menu module). Uses libxmp, same loader as the port. */
static void test_orders_valid_against_shipped_modules(void)
{
    xmp_context ctx = xmp_create_context();
    struct xmp_module_info mi;

    TEST_ASSERT_EQUAL_INT(0, xmp_load_module(ctx, "assets/OEKU.S3M"));
    xmp_get_module_info(ctx, &mi);
    int oeku_len = mi.mod->len;
    /* 92 raw orders; libxmp may strip the two trailing 0xFF markers. */
    TEST_ASSERT_TRUE(oeku_len >= 90 && oeku_len <= 92);
    TEST_ASSERT_TRUE(music_schedule_shop_order() <= oeku_len);
    for (int i = 0; i < MUSIC_GAME_ORDER_COUNT; i++) {
        TEST_ASSERT_TRUE(music_schedule_game_orders()[i] >= 1);
        TEST_ASSERT_TRUE(music_schedule_game_orders()[i] <= oeku_len);
    }
    xmp_release_module(ctx);

    TEST_ASSERT_EQUAL_INT(0, xmp_load_module(ctx, "assets/HUIPPE.S3M"));
    xmp_get_module_info(ctx, &mi);
    TEST_ASSERT_TRUE(mi.mod->len < music_schedule_shop_order());
    xmp_release_module(ctx);

    xmp_free_context(ctx);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_game_order_table_matches_data_segment);
    RUN_TEST(test_shop_order_is_0x54);
    RUN_TEST(test_gameplay_order_consumes_borland_random14);
    RUN_TEST(test_same_seed_same_order_sequence);
    RUN_TEST(test_orders_valid_against_shipped_modules);
    return UNITY_END();
}
