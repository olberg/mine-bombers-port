#include "unity.h"
#include "game/weapons.h"

void setUp(void) {}
void tearDown(void) {}

void test_all_weapons_unique_ids(void)
{
    for (int i = 0; i < WEAPON_COUNT; i++) {
        for (int j = i + 1; j < WEAPON_COUNT; j++) {
            TEST_ASSERT_NOT_EQUAL(g_weapon_defs[i].tile_id, g_weapon_defs[j].tile_id);
        }
    }
}

void test_weapon_lookup(void)
{
    const WeaponDef *w;

    w = weapon_get_def(0x57);  /* Small bomb */
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL_HEX8(0x57, w->tile_id);
    TEST_ASSERT_EQUAL_STRING("Small bomb", w->name);

    w = weapon_get_def(0x9D);  /* Mine */
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL_HEX8(0x9D, w->tile_id);

    /* Invalid ID */
    w = weapon_get_def(0xFF);
    TEST_ASSERT_NULL(w);
}

void test_inv_index_range(void)
{
    for (int i = 0; i < WEAPON_COUNT; i++) {
        int idx = weapon_inv_index(g_weapon_defs[i].tile_id);
        TEST_ASSERT_GREATER_OR_EQUAL(0, idx);
        TEST_ASSERT_LESS_THAN(WEAPON_COUNT, idx);
    }

    /* Invalid returns -1 */
    TEST_ASSERT_EQUAL(-1, weapon_inv_index(0xFF));
}

void test_known_fuse_values(void)
{
    const WeaponDef *w;

    w = weapon_get_def(0x57);  /* Small bomb: 100 */
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL(100, w->fuse_frames);

    w = weapon_get_def(0x59);  /* Large bomb: 80 */
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL(80, w->fuse_frames);

    w = weapon_get_def(0x7F);  /* Mega bomb: 260 */
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL(260, w->fuse_frames);

    w = weapon_get_def(0x9D);  /* Mine: 280 */
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_EQUAL(280, w->fuse_frames);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_weapons_unique_ids);
    RUN_TEST(test_weapon_lookup);
    RUN_TEST(test_inv_index_range);
    RUN_TEST(test_known_fuse_values);
    return UNITY_END();
}
