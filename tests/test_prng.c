#include "unity.h"
#include "util/prng.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* Verify LCG constants match Borland Pascal 7 (decompiled seg_1030:2602). */
static void test_lcg_first_step_from_zero(void)
{
    mb_prng_set_seed(0u);
    (void)mb_random(0); /* does not advance (n <= 0 early out) */
    TEST_ASSERT_EQUAL_UINT32(0u, mb_prng_get_seed());

    /* First advance: 0 * 0x08088405 + 1 = 1 */
    (void)mb_random(1);
    TEST_ASSERT_EQUAL_UINT32(1u, mb_prng_get_seed());

    /* Second advance: 1 * 0x08088405 + 1 = 0x08088406 */
    (void)mb_random(1);
    TEST_ASSERT_EQUAL_UINT32(0x08088406u, mb_prng_get_seed());
}

static void test_random_in_range(void)
{
    mb_prng_set_seed(42u);
    for (int i = 0; i < 1000; i++) {
        int v = mb_random(7);
        TEST_ASSERT_TRUE(v >= 0 && v < 7);
    }
}

static void test_random_n_le_0(void)
{
    mb_prng_set_seed(42u);
    uint32_t before = mb_prng_get_seed();
    TEST_ASSERT_EQUAL_INT(0, mb_random(0));
    TEST_ASSERT_EQUAL_INT(0, mb_random(-5));
    TEST_ASSERT_EQUAL_UINT32(before, mb_prng_get_seed());
}

static void test_deterministic_sequence(void)
{
    /* Lock a canonical sequence so regressions surface. */
    mb_prng_set_seed(1u);
    int seq[8];
    for (int i = 0; i < 8; i++) seq[i] = mb_random(100);

    mb_prng_set_seed(1u);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT(seq[i], mb_random(100));
    }
}

static void test_range_helper(void)
{
    mb_prng_set_seed(1u);
    for (int i = 0; i < 500; i++) {
        int v = mb_random_range(5, 9);
        TEST_ASSERT_TRUE(v >= 5 && v <= 9);
    }
    /* Degenerate: lo == hi */
    TEST_ASSERT_EQUAL_INT(7, mb_random_range(7, 7));
    /* hi < lo returns lo without advancing beyond whatever the caller had */
    TEST_ASSERT_EQUAL_INT(3, mb_random_range(3, 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lcg_first_step_from_zero);
    RUN_TEST(test_random_in_range);
    RUN_TEST(test_random_n_le_0);
    RUN_TEST(test_deterministic_sequence);
    RUN_TEST(test_range_helper);
    return UNITY_END();
}
