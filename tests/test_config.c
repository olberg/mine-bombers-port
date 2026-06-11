#include "unity.h"
#include "game/config.h"
#include <stdio.h>
#include <string.h>

static const char *test_file = "test_config.dat";

void setUp(void) {}
void tearDown(void) { remove(test_file); }

void test_defaults(void)
{
    config_set_defaults();

    TEST_ASSERT_EQUAL(2, g_config.num_players);
    TEST_ASSERT_EQUAL_HEX8(0x2D, g_config.treasures);
    TEST_ASSERT_EQUAL(15, g_config.total_rounds);
    TEST_ASSERT_EQUAL(750, g_config.starting_cash);
    TEST_ASSERT_EQUAL(8, g_config.frame_delay);
    TEST_ASSERT_EQUAL(100, g_config.bomb_damage_pct);
    TEST_ASSERT_EQUAL(0, g_config.option_toggle[0]);
    TEST_ASSERT_EQUAL(0, g_config.option_toggle[1]);
    TEST_ASSERT_EQUAL(0, g_config.option_toggle[2]);
    TEST_ASSERT_EQUAL(0, g_config.option_toggle[3]);
}

void test_save_load_roundtrip(void)
{
    config_set_defaults();
    g_config.num_players = 3;
    g_config.starting_cash = 1500;
    g_config.option_toggle[2] = 1;
    g_config.bomb_damage_pct = 50;

    TEST_ASSERT_TRUE(config_save(test_file));

    /* Clear and reload */
    memset(&g_config, 0, sizeof(g_config));
    TEST_ASSERT_TRUE(config_load(test_file));

    TEST_ASSERT_EQUAL(3, g_config.num_players);
    TEST_ASSERT_EQUAL(1500, g_config.starting_cash);
    TEST_ASSERT_EQUAL(1, g_config.option_toggle[2]);
    TEST_ASSERT_EQUAL(50, g_config.bomb_damage_pct);
}

void test_load_missing_file(void)
{
    memset(&g_config, 0xFF, sizeof(g_config));
    TEST_ASSERT_FALSE(config_load("nonexistent_file.dat"));

    /* Should fall back to defaults */
    TEST_ASSERT_EQUAL(2, g_config.num_players);
    TEST_ASSERT_EQUAL(15, g_config.total_rounds);
}

void test_file_size_17_bytes(void)
{
    config_set_defaults();
    TEST_ASSERT_TRUE(config_save(test_file));

    FILE *f = fopen(test_file, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    TEST_ASSERT_EQUAL(17, size);
}

void test_binary_layout(void)
{
    /* Verify specific byte offsets match the original format */
    config_set_defaults();
    g_config.num_players = 4;
    g_config.treasures = 0xAB;
    g_config.total_rounds = 0x1234;
    g_config.starting_cash = 0x5678;
    g_config.option_toggle[0] = 0x11;
    g_config.option_toggle[3] = 0x44;
    g_config.bomb_damage_pct = 0xEE;

    TEST_ASSERT_TRUE(config_save(test_file));

    FILE *f = fopen(test_file, "rb");
    uint8_t buf[17];
    fread(buf, 1, 17, f);
    fclose(f);

    TEST_ASSERT_EQUAL_HEX8(4, buf[0]);       /* num_players */
    TEST_ASSERT_EQUAL_HEX8(0xAB, buf[1]);    /* treasures */
    TEST_ASSERT_EQUAL_HEX8(0x34, buf[2]);    /* total_rounds lo */
    TEST_ASSERT_EQUAL_HEX8(0x12, buf[3]);    /* total_rounds hi */
    TEST_ASSERT_EQUAL_HEX8(0x78, buf[4]);    /* starting_cash lo */
    TEST_ASSERT_EQUAL_HEX8(0x56, buf[5]);    /* starting_cash hi */
    TEST_ASSERT_EQUAL_HEX8(0x11, buf[12]);   /* option_toggle[0] (darkness) */
    TEST_ASSERT_EQUAL_HEX8(0x44, buf[15]);   /* option_toggle[3] (winner_by) */
    TEST_ASSERT_EQUAL_HEX8(0xEE, buf[16]);   /* bomb_damage_pct */
}

void test_total_rounds_min_1(void)
{
    config_set_defaults();
    g_config.total_rounds = 0;
    TEST_ASSERT_TRUE(config_save(test_file));
    TEST_ASSERT_TRUE(config_load(test_file));
    TEST_ASSERT_EQUAL(1, g_config.total_rounds);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults);
    RUN_TEST(test_save_load_roundtrip);
    RUN_TEST(test_load_missing_file);
    RUN_TEST(test_file_size_17_bytes);
    RUN_TEST(test_binary_layout);
    RUN_TEST(test_total_rounds_min_1);
    return UNITY_END();
}
