#include "unity.h"
#include "game/map_picker.h"
#include "util/prng.h"

void setUp(void) {}
void tearDown(void) {}

void test_default_selections_are_random(void)
{
    map_picker_reset();
    const int *sel = map_picker_get_selections();
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        TEST_ASSERT_EQUAL_INT(MAP_PICK_RANDOM, sel[i]);
    }
}

void test_has_selection_default_false(void)
{
    map_picker_reset();
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        TEST_ASSERT_FALSE(map_picker_has_selection(i));
    }
}

void test_get_map_index_default_negative(void)
{
    map_picker_reset();
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        TEST_ASSERT_EQUAL_INT(-1, map_picker_get_map_index(i));
    }
}

void test_out_of_range_returns_safe_values(void)
{
    map_picker_reset();
    TEST_ASSERT_FALSE(map_picker_has_selection(-1));
    TEST_ASSERT_FALSE(map_picker_has_selection(MAP_PICKER_MAX_ROUNDS));
    TEST_ASSERT_EQUAL_INT(-1, map_picker_get_map_index(-1));
    TEST_ASSERT_EQUAL_INT(-1, map_picker_get_map_index(MAP_PICKER_MAX_ROUNDS));
}

void test_max_rounds_constant(void)
{
    /* Original uses 56 rounds max (0x37 + 1 = 56),
     * matching g_high_score_table initialization at seg_1010:5560 */
    TEST_ASSERT_EQUAL_INT(56, MAP_PICKER_MAX_ROUNDS);
}

void test_random_sentinel_value(void)
{
    /* Original uses 32000 as the "random map" sentinel,
     * matching g_high_score_table init at seg_1010:5560 */
    TEST_ASSERT_EQUAL_INT(32000, MAP_PICK_RANDOM);
}

/* ---- Original picker session semantics (FUN_1010_e231/dfee) ---- */

/* Picker entry zeroes all slots (seg_1010:8563-8566); reopening discards
 * previous selections. */
void test_session_begin_zeroes_slots(void)
{
    map_picker_session_begin();
    map_picker_assign_grid(3, 10);
    map_picker_session_begin();

    const int *sel = map_picker_get_selections();
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        TEST_ASSERT_EQUAL_INT(0, sel[i]);
    }
    TEST_ASSERT_EQUAL_INT(0, map_picker_assigned_count());
}

/* Enter/Space appends the grid index sequentially; slots are 1-BASED grid
 * indices (cell 0 = "Random", cells 1..N = maps). */
void test_assign_is_sequential_and_one_based(void)
{
    map_picker_session_begin();
    map_picker_assign_grid(3, 10);   /* round 0 <- map_list index 2 */
    map_picker_assign_grid(1, 10);   /* round 1 <- map_list index 0 */

    TEST_ASSERT_EQUAL_INT(2, map_picker_assigned_count());
    TEST_ASSERT_EQUAL_INT(3, map_picker_get_selections()[0]);
    map_picker_finalize();
    TEST_ASSERT_EQUAL_INT(2, map_picker_get_map_index(0));
    TEST_ASSERT_EQUAL_INT(0, map_picker_get_map_index(1));
    TEST_ASSERT_TRUE(map_picker_has_selection(0));
}

/* Assignments stop at total_rounds (seg_1010:8494). */
void test_assign_gated_by_total_rounds(void)
{
    map_picker_session_begin();
    map_picker_assign_grid(1, 2);
    map_picker_assign_grid(2, 2);
    map_picker_assign_grid(3, 2);    /* ignored: 2 rounds only */

    TEST_ASSERT_EQUAL_INT(2, map_picker_assigned_count());
    TEST_ASSERT_EQUAL_INT(0, map_picker_get_selections()[2]);
}

/* Picking the "Random" cell (grid 0) stores 0, which finalize converts to
 * 32000 — an explicit random round, indistinguishable from unassigned
 * (original behavior: exit loop seg_1010:8601-8608). */
void test_random_cell_pick_becomes_random(void)
{
    map_picker_session_begin();
    map_picker_assign_grid(0, 10);   /* round 0 <- "Random" */
    map_picker_assign_grid(5, 10);   /* round 1 <- map_list index 4 */
    map_picker_finalize();

    TEST_ASSERT_FALSE(map_picker_has_selection(0));
    TEST_ASSERT_EQUAL_INT(-1, map_picker_get_map_index(0));
    TEST_ASSERT_EQUAL_INT(MAP_PICK_RANDOM, map_picker_get_selections()[0]);
    TEST_ASSERT_EQUAL_INT(4, map_picker_get_map_index(1));
}

/* Finalize converts ONLY still-0 slots (unassigned tail included). */
void test_finalize_converts_unassigned(void)
{
    map_picker_session_begin();
    map_picker_assign_grid(7, 10);
    map_picker_finalize();

    const int *sel = map_picker_get_selections();
    TEST_ASSERT_EQUAL_INT(7, sel[0]);
    for (int i = 1; i < MAP_PICKER_MAX_ROUNDS; i++) {
        TEST_ASSERT_EQUAL_INT(MAP_PICK_RANDOM, sel[i]);
    }
}

/* Random fill OVERWRITES all round slots from 0 — including manual picks —
 * draws from Random(map_count + 1) (0..count, including the Random cell),
 * keeps picks unique while possible, and marks all rounds assigned
 * (seg_1010:8501-8523). */
void test_fill_random_overwrites_all_and_is_unique(void)
{
    mb_prng_set_seed(424242);

    map_picker_session_begin();
    map_picker_assign_grid(9, 10);   /* manual pick to be overwritten */

    int map_count = 20;
    int rounds = 10;
    map_picker_fill_random(rounds, map_count);

    TEST_ASSERT_EQUAL_INT(rounds, map_picker_assigned_count());

    const int *sel = map_picker_get_selections();
    for (int r = 0; r < rounds; r++) {
        TEST_ASSERT_TRUE(sel[r] >= 0 && sel[r] <= map_count);
        for (int prev = 0; prev < r; prev++) {
            TEST_ASSERT_NOT_EQUAL(sel[prev], sel[r]);
        }
    }
}

/* With fewer distinct values than rounds, fill still terminates (the
 * 100-attempt retry gives up and accepts a duplicate). */
void test_fill_random_more_rounds_than_maps(void)
{
    mb_prng_set_seed(7);

    map_picker_session_begin();
    map_picker_fill_random(10, 3);   /* only 4 distinct values (0..3) */

    TEST_ASSERT_EQUAL_INT(10, map_picker_assigned_count());
    const int *sel = map_picker_get_selections();
    for (int r = 0; r < 10; r++) {
        TEST_ASSERT_TRUE(sel[r] >= 0 && sel[r] <= 3);
    }
}

/* The round gate mirrors the original's `slot < 30000` (seg_1000:7082):
 * 32000 and the in-session 0 are both "random". */
void test_gate_thresholds(void)
{
    map_picker_session_begin();
    map_picker_assign_grid(1, 5);
    /* In-session (before finalize): slot 1.. = picked, slot 0 value = 0 */
    TEST_ASSERT_TRUE(map_picker_has_selection(0));
    TEST_ASSERT_FALSE(map_picker_has_selection(1));  /* still 0 */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_selections_are_random);
    RUN_TEST(test_has_selection_default_false);
    RUN_TEST(test_get_map_index_default_negative);
    RUN_TEST(test_out_of_range_returns_safe_values);
    RUN_TEST(test_max_rounds_constant);
    RUN_TEST(test_random_sentinel_value);
    RUN_TEST(test_session_begin_zeroes_slots);
    RUN_TEST(test_assign_is_sequential_and_one_based);
    RUN_TEST(test_assign_gated_by_total_rounds);
    RUN_TEST(test_random_cell_pick_becomes_random);
    RUN_TEST(test_finalize_converts_unassigned);
    RUN_TEST(test_fill_random_overwrites_all_and_is_unique);
    RUN_TEST(test_fill_random_more_rounds_than_maps);
    RUN_TEST(test_gate_thresholds);
    return UNITY_END();
}
