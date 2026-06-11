#include "unity.h"
#include "game/sound_config.h"
#include "audio/music.h"
#include "audio/sfx.h"
#include <stdio.h>
#include <string.h>

/* Stubs for music/sfx enable/disable (real implementations need Raylib audio) */
static bool stub_music_enabled = true;
static bool stub_sfx_enabled = true;

void music_init(void) {}
bool music_load(const char *path) { (void)path; return true; }
void music_play(void) {}
void music_stop(void) {}
void music_update(void) {}
void music_shutdown(void) {}

void music_set_enabled(bool flag) { stub_music_enabled = flag; }
bool music_is_enabled(void) { return stub_music_enabled; }

void sfx_init(void) {}
void sfx_play(SfxId id) { (void)id; }
void sfx_shutdown(void) {}

void sfx_set_enabled(bool flag) { stub_sfx_enabled = flag; }
bool sfx_is_enabled(void) { return stub_sfx_enabled; }

void setUp(void)
{
    stub_music_enabled = true;
    stub_sfx_enabled = true;
}

void tearDown(void) {}

static const char *test_file = "test_soundcfg.dat";

static void cleanup_test_file(void)
{
    remove(test_file);
}

/* Test: save and load round-trip preserves music/sfx state */
void test_save_load_both_enabled(void)
{
    stub_music_enabled = true;
    stub_sfx_enabled = true;

    TEST_ASSERT_TRUE(sound_config_save(test_file));

    /* Change state before loading */
    stub_music_enabled = false;
    stub_sfx_enabled = false;

    TEST_ASSERT_TRUE(sound_config_load(test_file));
    TEST_ASSERT_TRUE(stub_music_enabled);
    TEST_ASSERT_TRUE(stub_sfx_enabled);

    cleanup_test_file();
}

void test_save_load_music_off(void)
{
    stub_music_enabled = false;
    stub_sfx_enabled = true;

    TEST_ASSERT_TRUE(sound_config_save(test_file));

    stub_music_enabled = true;
    stub_sfx_enabled = false;

    TEST_ASSERT_TRUE(sound_config_load(test_file));
    TEST_ASSERT_FALSE(stub_music_enabled);
    TEST_ASSERT_TRUE(stub_sfx_enabled);

    cleanup_test_file();
}

void test_save_load_sfx_off(void)
{
    stub_music_enabled = true;
    stub_sfx_enabled = false;

    TEST_ASSERT_TRUE(sound_config_save(test_file));

    stub_music_enabled = false;
    stub_sfx_enabled = true;

    TEST_ASSERT_TRUE(sound_config_load(test_file));
    TEST_ASSERT_TRUE(stub_music_enabled);
    TEST_ASSERT_FALSE(stub_sfx_enabled);

    cleanup_test_file();
}

void test_save_load_both_off(void)
{
    stub_music_enabled = false;
    stub_sfx_enabled = false;

    TEST_ASSERT_TRUE(sound_config_save(test_file));

    stub_music_enabled = true;
    stub_sfx_enabled = true;

    TEST_ASSERT_TRUE(sound_config_load(test_file));
    TEST_ASSERT_FALSE(stub_music_enabled);
    TEST_ASSERT_FALSE(stub_sfx_enabled);

    cleanup_test_file();
}

/* Test: loading nonexistent file defaults to both enabled */
void test_load_missing_file_defaults(void)
{
    stub_music_enabled = false;
    stub_sfx_enabled = false;

    TEST_ASSERT_FALSE(sound_config_load("nonexistent_sound.dat"));
    TEST_ASSERT_TRUE(stub_music_enabled);
    TEST_ASSERT_TRUE(stub_sfx_enabled);
}

/* Test: file format is exactly 6 bytes matching original format */
void test_file_format_size(void)
{
    stub_music_enabled = true;
    stub_sfx_enabled = true;

    TEST_ASSERT_TRUE(sound_config_save(test_file));

    FILE *f = fopen(test_file, "rb");
    TEST_ASSERT_NOT_NULL(f);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    TEST_ASSERT_EQUAL_INT(6, size);

    cleanup_test_file();
}

/* Test: features byte (offset 4) has correct bitmask */
void test_features_byte_bitmask(void)
{
    stub_music_enabled = true;
    stub_sfx_enabled = true;
    sound_config_save(test_file);

    FILE *f = fopen(test_file, "rb");
    uint8_t data[6];
    fread(data, 1, 6, f);
    fclose(f);

    /* bit 0 = music (1), bit 1 = sfx (1) → 3 */
    TEST_ASSERT_EQUAL_UINT8(3, data[4]);

    /* mix rate multiplier = 22 */
    TEST_ASSERT_EQUAL_UINT8(22, data[5]);

    cleanup_test_file();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_save_load_both_enabled);
    RUN_TEST(test_save_load_music_off);
    RUN_TEST(test_save_load_sfx_off);
    RUN_TEST(test_save_load_both_off);
    RUN_TEST(test_load_missing_file_defaults);
    RUN_TEST(test_file_format_size);
    RUN_TEST(test_features_byte_bitmask);
    return UNITY_END();
}
