#include "unity.h"
#include "game/player_db.h"
#include "game/player.h"
#include "game/config.h"
#include <string.h>
#include <stdio.h>

static PlayerDatabase db;
static const char *TEST_DB_PATH = "test_players.dat";

void setUp(void)
{
    memset(&db, 0, sizeof(db));
    config_set_defaults();  /* num_players=2, starting_cash=750 */
}
void tearDown(void) { remove(TEST_DB_PATH); }

void test_db_record_size(void)
{
    TEST_ASSERT_EQUAL(PLAYER_RECORD_SIZE, sizeof(PlayerRecord));
}

void test_db_init_defaults(void)
{
    player_db_init_defaults(&db);

    for (int i = 0; i < PLAYER_DB_SLOTS; i++) {
        TEST_ASSERT_EQUAL(1, db.records[i].exists);
        /* Name should be all zeros (Pascal string: length=0, all chars=0) */
        for (int j = 0; j < 25; j++) {
            TEST_ASSERT_EQUAL(0, db.records[i].name[j]);
        }
    }
}

/* Helper: write a Pascal-format name into a record's name field.
 * name[0] = length, name[1..] = characters. */
static void set_pascal_name(PlayerRecord *rec, const char *str)
{
    int len = (int)strlen(str);
    if (len > 24) len = 24;
    rec->name[0] = (char)len;
    memcpy(&rec->name[1], str, len);
}

void test_db_save_load_roundtrip(void)
{
    player_db_init_defaults(&db);

    /* Set some data in a record (Pascal string format) */
    db.records[0].exists = 1;
    set_pascal_name(&db.records[0], "TestPlayer");
    db.records[5].exists = 2;
    set_pascal_name(&db.records[5], "AnotherPlayer");

    TEST_ASSERT_TRUE(player_db_save(&db, TEST_DB_PATH));

    PlayerDatabase db2;
    memset(&db2, 0xFF, sizeof(db2));
    TEST_ASSERT_TRUE(player_db_load(&db2, TEST_DB_PATH));

    /* Compare byte-for-byte */
    TEST_ASSERT_EQUAL_MEMORY(&db, &db2, sizeof(PlayerDatabase));
}

void test_db_load_missing_file(void)
{
    bool result = player_db_load(&db, "nonexistent_file.dat");
    TEST_ASSERT_FALSE(result);

    /* Should have initialized defaults */
    for (int i = 0; i < PLAYER_DB_SLOTS; i++) {
        TEST_ASSERT_EQUAL(1, db.records[i].exists);
    }
}

void test_db_load_wrong_size(void)
{
    /* Write a file of wrong size */
    FILE *f = fopen(TEST_DB_PATH, "wb");
    TEST_ASSERT_NOT_NULL(f);
    uint8_t dummy[100] = {0};
    fwrite(dummy, 1, 100, f);
    fclose(f);

    bool result = player_db_load(&db, TEST_DB_PATH);
    TEST_ASSERT_FALSE(result);

    /* Should have initialized defaults */
    TEST_ASSERT_EQUAL(1, db.records[0].exists);
}

void test_init_from_record(void)
{
    player_db_init_defaults(&db);
    set_pascal_name(&db.records[3], "Hero");

    Player p;
    player_init_from_record(&p, &db.records[3], 2);

    TEST_ASSERT_EQUAL_STRING("Hero", p.name);
    TEST_ASSERT_EQUAL(2, p.player_num);
    TEST_ASSERT_EQUAL(100, p.health);  /* from defaults (original: DAT_1038_1c07 = 100) */
}

void test_record_name_extraction(void)
{
    player_db_init_defaults(&db);
    set_pascal_name(&db.records[0], "TestName");

    char name[25];
    player_record_name(name, sizeof(name), &db.records[0]);
    TEST_ASSERT_EQUAL_STRING("TestName", name);

    /* Empty name (length=0) */
    char name2[25];
    player_record_name(name2, sizeof(name2), &db.records[1]);
    TEST_ASSERT_EQUAL_STRING("", name2);
}

void test_stat_offsets(void)
{
    /* Verify stats[0] starts at byte 26 of the record (matching decompiled offset -0x4B) */
    PlayerRecord rec;
    memset(&rec, 0, sizeof(rec));
    uint8_t *raw = (uint8_t *)&rec;

    /* Set stats[0] to a known value */
    rec.stats[0] = 0x12345678;
    TEST_ASSERT_EQUAL_HEX8(0x78, raw[26]);
    TEST_ASSERT_EQUAL_HEX8(0x56, raw[27]);
    TEST_ASSERT_EQUAL_HEX8(0x34, raw[28]);
    TEST_ASSERT_EQUAL_HEX8(0x12, raw[29]);

    /* Verify weapons start at byte 66 */
    rec.weapons[0] = 0xAB;
    TEST_ASSERT_EQUAL_HEX8(0xAB, raw[66]);
}

void test_init_from_record_slot(void)
{
    player_db_init_defaults(&db);
    set_pascal_name(&db.records[7], "SlotTest");

    Player p;
    player_init_from_record_slot(&p, &db.records[7], 1, 7);

    TEST_ASSERT_EQUAL_STRING("SlotTest", p.name);
    TEST_ASSERT_EQUAL(1, p.player_num);
    TEST_ASSERT_EQUAL(7, p.record_slot);
}

void test_merge_match_stats(void)
{
    /* FUN_1000_15c7: record.stats[i] += match_stats[i], dword-for-dword,
     * once per match. */
    player_db_init_defaults(&db);
    set_pascal_name(&db.records[2], "StatsTest");
    db.records[2].stats[STAT_ROUNDS] = 7;     /* pre-existing history */

    Player p;
    player_init_from_record_slot(&p, &db.records[2], 0, 2);
    TEST_ASSERT_EQUAL(0, p.match_stats[STAT_MATCHES]);  /* zeroed at init */

    /* Simulate a 3-round match the player won */
    p.match_stats[STAT_MATCHES]    = 1;
    p.match_stats[STAT_MATCH_WINS] = 1;
    p.match_stats[STAT_ROUNDS]     = 3;
    p.match_stats[STAT_TREASURES]  = 12;
    p.match_stats[STAT_MONEY]      = 1450;

    player_db_merge_match_stats(&db, &p);

    TEST_ASSERT_EQUAL(1, db.records[2].stats[STAT_MATCHES]);
    TEST_ASSERT_EQUAL(1, db.records[2].stats[STAT_MATCH_WINS]);
    TEST_ASSERT_EQUAL(10, db.records[2].stats[STAT_ROUNDS]);   /* 7 + 3 */
    TEST_ASSERT_EQUAL(12, db.records[2].stats[STAT_TREASURES]);
    TEST_ASSERT_EQUAL(1450, db.records[2].stats[STAT_MONEY]);
    TEST_ASSERT_EQUAL(0, db.records[2].stats[STAT_ROUND_WINS]);
}

void test_update_record_invalid_slot(void)
{
    /* Ensure no crash with invalid slot index */
    player_db_init_defaults(&db);

    Player p;
    player_init_defaults(&p, 0);
    p.record_slot = -1;  /* invalid */
    player_db_merge_match_stats(&db, &p);  /* should not crash */

    p.record_slot = 99;  /* out of range */
    player_db_merge_match_stats(&db, &p);  /* should not crash */

    /* All records should remain unchanged */
    TEST_ASSERT_EQUAL(0, db.records[0].stats[STAT_ROUNDS]);
}

void test_cheat_invis_sets_visual_flag(void)
{
    /* Invis cheat should set cheat_visual to CHEAT_INVIS so the renderer
     * skips drawing the player sprite (FUN_1000_3095, seg_1000:1984-2014). */
    Player p;
    player_init_defaults(&p, 0);
    TEST_ASSERT_EQUAL(0, p.cheat_visual);

    player_apply_cheat(&p, CHEAT_INVIS);
    TEST_ASSERT_EQUAL(CHEAT_INVIS, p.cheat_visual);
}

void test_cheat_mutation_sets_visual_flag(void)
{
    /* Mutation cheat should set cheat_visual to CHEAT_MUTATION so the renderer
     * uses monster sprite set 10 instead of normal player sprites
     * (FUN_1000_3129, seg_1000:2028-2038 copies DAT_1038_2352 monster data). */
    Player p;
    player_init_defaults(&p, 0);
    TEST_ASSERT_EQUAL(0, p.cheat_visual);

    player_apply_cheat(&p, CHEAT_MUTATION);
    TEST_ASSERT_EQUAL(CHEAT_MUTATION, p.cheat_visual);
}

void test_cheat_detect_and_apply_via_record(void)
{
    /* End-to-end: creating a player from a record named "Invis" should
     * detect the cheat and set the visual flag automatically. */
    player_db_init_defaults(&db);
    set_pascal_name(&db.records[0], "Invis");

    Player p;
    player_init_from_record(&p, &db.records[0], 0);
    TEST_ASSERT_EQUAL_STRING("Invis", p.name);
    TEST_ASSERT_EQUAL(CHEAT_INVIS, p.cheat_visual);

    /* Mutation via record */
    set_pascal_name(&db.records[1], "Mutation");
    Player p2;
    player_init_from_record(&p2, &db.records[1], 1);
    TEST_ASSERT_EQUAL_STRING("Mutation", p2.name);
    TEST_ASSERT_EQUAL(CHEAT_MUTATION, p2.cheat_visual);
}

/* The original seeds wallets in process_menu_selection at PLAY
 * (seg_1010:7169-7183). MP (num_players >= 2): all wallets = Starting Cash
 * option, sign-extended (factory default 750 = 0x2EE, seg_1010:5485;
 * options item 0 steps +-100 over [0, 2650], seg_1000:351-361/426-440).
 * SP: P1 wallet = fixed 250 (0xFA), option ignored. */
void test_starting_cash_mp_from_option(void)
{
    player_db_init_defaults(&db);
    g_config.num_players = 2;

    Player p;
    player_init_from_record(&p, &db.records[0], 0);
    TEST_ASSERT_EQUAL_INT32(750, p.cash);  /* factory default */

    g_config.starting_cash = 2650;  /* options max */
    player_init_from_record(&p, &db.records[1], 1);
    TEST_ASSERT_EQUAL_INT32(2650, p.cash);

    g_config.starting_cash = 0;     /* options min */
    player_init_from_record(&p, &db.records[2], 2);
    TEST_ASSERT_EQUAL_INT32(0, p.cash);
}

void test_starting_cash_sp_fixed_250(void)
{
    player_db_init_defaults(&db);
    g_config.num_players = 1;
    g_config.starting_cash = 2650;  /* must be ignored in SP */

    Player p;
    player_init_from_record(&p, &db.records[0], 0);
    TEST_ASSERT_EQUAL_INT32(250, p.cash);
}

void test_starting_cash_lottery_overrides(void)
{
    /* Ordering: wallets are seeded at PLAY, cheats apply later at player
     * select — a Lottery name overwrites the starting cash with 50000. */
    player_db_init_defaults(&db);
    g_config.num_players = 2;
    set_pascal_name(&db.records[0], "Lottery");

    Player p;
    player_init_from_record(&p, &db.records[0], 0);
    TEST_ASSERT_EQUAL_INT32(50000, p.cash);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_db_record_size);
    RUN_TEST(test_db_init_defaults);
    RUN_TEST(test_db_save_load_roundtrip);
    RUN_TEST(test_db_load_missing_file);
    RUN_TEST(test_db_load_wrong_size);
    RUN_TEST(test_init_from_record);
    RUN_TEST(test_record_name_extraction);
    RUN_TEST(test_stat_offsets);
    RUN_TEST(test_init_from_record_slot);
    RUN_TEST(test_merge_match_stats);
    RUN_TEST(test_update_record_invalid_slot);
    RUN_TEST(test_cheat_invis_sets_visual_flag);
    RUN_TEST(test_cheat_mutation_sets_visual_flag);
    RUN_TEST(test_cheat_detect_and_apply_via_record);
    RUN_TEST(test_starting_cash_mp_from_option);
    RUN_TEST(test_starting_cash_sp_fixed_250);
    RUN_TEST(test_starting_cash_lottery_overrides);
    return UNITY_END();
}
