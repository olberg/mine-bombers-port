#include "unity.h"
#include "game/map_list.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_map_list_load_finds_mne_files(void)
{
    int count = map_list_load("assets");
    /* The original distribution has 46 .MNE files */
    TEST_ASSERT_EQUAL_INT(46, count);
    TEST_ASSERT_EQUAL_INT(46, map_list_count());
}

void test_map_list_sorted_alphabetically(void)
{
    map_list_load("assets");
    int count = map_list_count();
    TEST_ASSERT_TRUE(count > 1);

    /* Verify sorted order */
    for (int i = 1; i < count; i++) {
        const char *prev = map_list_name(i - 1);
        const char *curr = map_list_name(i);
        TEST_ASSERT_NOT_NULL(prev);
        TEST_ASSERT_NOT_NULL(curr);
        /* Case-insensitive: prev should come before or equal curr */
        int cmp = 0;
        for (int j = 0; prev[j] || curr[j]; j++) {
            int a = (prev[j] >= 'a' && prev[j] <= 'z') ? prev[j] - 32 : prev[j];
            int b = (curr[j] >= 'a' && curr[j] <= 'z') ? curr[j] - 32 : curr[j];
            if (a != b) { cmp = a - b; break; }
        }
        TEST_ASSERT_TRUE_MESSAGE(cmp <= 0, "Map list not sorted alphabetically");
    }
}

void test_map_list_first_is_anzulaby(void)
{
    map_list_load("assets");
    /* ANZULABY should be first alphabetically among the 46 maps */
    const char *first = map_list_name(0);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_STRING("ANZULABY", first);
}

void test_map_list_contains_komplex(void)
{
    map_list_load("assets");
    int count = map_list_count();
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(map_list_name(i), "KOMPLEX") == 0) {
            found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "KOMPLEX not found in map list");
}

void test_map_list_build_path(void)
{
    map_list_load("assets");
    char buf[64];
    bool ok = map_list_build_path(buf, sizeof(buf), "assets", 0);
    TEST_ASSERT_TRUE(ok);
    /* First map alphabetically is ANZULABY */
    TEST_ASSERT_EQUAL_STRING("assets/ANZULABY.MNE", buf);
}

void test_map_list_build_path_out_of_range(void)
{
    map_list_load("assets");
    char buf[64];
    bool ok = map_list_build_path(buf, sizeof(buf), "assets", -1);
    TEST_ASSERT_FALSE(ok);
    ok = map_list_build_path(buf, sizeof(buf), "assets", 9999);
    TEST_ASSERT_FALSE(ok);
}

void test_map_list_name_out_of_range(void)
{
    map_list_load("assets");
    TEST_ASSERT_NULL(map_list_name(-1));
    TEST_ASSERT_NULL(map_list_name(9999));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_map_list_load_finds_mne_files);
    RUN_TEST(test_map_list_sorted_alphabetically);
    RUN_TEST(test_map_list_first_is_anzulaby);
    RUN_TEST(test_map_list_contains_komplex);
    RUN_TEST(test_map_list_build_path);
    RUN_TEST(test_map_list_build_path_out_of_range);
    RUN_TEST(test_map_list_name_out_of_range);
    return UNITY_END();
}
