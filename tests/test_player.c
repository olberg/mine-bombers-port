#include "unity.h"
#include "game/player.h"
#include "game/bombs.h"
#include <string.h>

static Player p;

void setUp(void) { memset(&p, 0, sizeof(p)); }
void tearDown(void) {}

void test_init_defaults(void)
{
    player_init_defaults(&p, 0);

    TEST_ASSERT_EQUAL(0, p.player_num);
    TEST_ASSERT_EQUAL(100, p.health);
    TEST_ASSERT_EQUAL(100, p.max_health);
    TEST_ASSERT_EQUAL(0, p.cash);
    TEST_ASSERT_EQUAL(0, p.dead);
    TEST_ASSERT_EQUAL(1, p.active);
    TEST_ASSERT_EQUAL(WEAPON_SMALL_BOMB, p.selected_weapon);
    TEST_ASSERT_EQUAL(3, p.lives);

    /* No starting inventory (original-verified: first shop shows ITEMS 0
     * for a fresh player; the old 5-small-bomb grant was a port invention) */
    int idx = player_weapon_index(WEAPON_SMALL_BOMB);
    TEST_ASSERT_GREATER_OR_EQUAL(0, idx);
    TEST_ASSERT_EQUAL(0, p.weapons[idx]);
}

void test_weapon_cycle_forward(void)
{
    player_init_defaults(&p, 0);

    /* Give player some small, medium and large bombs (defaults start at 0) */
    int sml_idx = player_weapon_index(WEAPON_SMALL_BOMB);
    int med_idx = player_weapon_index(WEAPON_MEDIUM_BOMB);
    int lrg_idx = player_weapon_index(WEAPON_LARGE_BOMB);
    p.weapons[sml_idx] = 5;
    p.weapons[med_idx] = 3;
    p.weapons[lrg_idx] = 2;

    /* Currently on small bomb, cycle forward should go to medium */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(WEAPON_MEDIUM_BOMB, p.selected_weapon);

    /* Forward again -> large */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(WEAPON_LARGE_BOMB, p.selected_weapon);

    /* Forward again -> wraps back to small */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(WEAPON_SMALL_BOMB, p.selected_weapon);
}

void test_weapon_cycle_skip_empty(void)
{
    player_init_defaults(&p, 0);

    /* Only have small bombs and mines */
    int sml_idx = player_weapon_index(WEAPON_SMALL_BOMB);
    int mine_idx = player_weapon_index(WEAPON_MINE);
    p.weapons[sml_idx] = 5;
    p.weapons[mine_idx] = 1;

    /* Cycle forward from small bomb -> should skip to mine */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(WEAPON_MINE, p.selected_weapon);

    /* Cycle forward from mine -> should wrap to small bomb */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(WEAPON_SMALL_BOMB, p.selected_weapon);
}

void test_weapon_index_mapping(void)
{
    /* Verify all 21 weapon IDs map to valid indices */
    for (int i = 0; i < WEAPON_CYCLE_USED; i++) {
        int idx = player_weapon_index(WEAPON_CYCLE_ORDER[i]);
        TEST_ASSERT_EQUAL(i, idx);
    }

    /* Invalid weapon ID returns -1 */
    TEST_ASSERT_EQUAL(-1, player_weapon_index(0xFF));
    TEST_ASSERT_EQUAL(-1, player_weapon_index(0x00));
}

void test_bomb_sig_per_player(void)
{
    /* Verify each player gets correct bomb signature tiles
     * matching decompiled seg_1010:5562-5567 */
    Player players[4];
    for (int i = 0; i < 4; i++)
        player_init_defaults(&players[i], i);

    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P1_A, players[0].bomb_sig[0]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P1_B, players[0].bomb_sig[1]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P2_A, players[1].bomb_sig[0]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P2_B, players[1].bomb_sig[1]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P3_A, players[2].bomb_sig[0]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P3_B, players[2].bomb_sig[1]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P4_A, players[3].bomb_sig[0]);
    TEST_ASSERT_EQUAL_HEX8(BOMB_SIG_P4_B, players[3].bomb_sig[1]);
}

void test_bomb_sig_weapon_index(void)
{
    /* All bomb signature tiles should map to WEAPON_SIG0_SLOT or SIG1_SLOT */
    TEST_ASSERT_EQUAL(WEAPON_SIG0_SLOT, player_weapon_index(0x63));
    TEST_ASSERT_EQUAL(WEAPON_SIG0_SLOT, player_weapon_index(0x67));
    TEST_ASSERT_EQUAL(WEAPON_SIG0_SLOT, player_weapon_index(0x82));
    TEST_ASSERT_EQUAL(WEAPON_SIG0_SLOT, player_weapon_index(0x69));

    TEST_ASSERT_EQUAL(WEAPON_SIG1_SLOT, player_weapon_index(0x64));
    TEST_ASSERT_EQUAL(WEAPON_SIG1_SLOT, player_weapon_index(0x68));
    TEST_ASSERT_EQUAL(WEAPON_SIG1_SLOT, player_weapon_index(0x83));
    TEST_ASSERT_EQUAL(WEAPON_SIG1_SLOT, player_weapon_index(0x6A));
}

void test_bomb_sig_in_weapon_cycle(void)
{
    /* Player 3 (uses 0x82/0x83) with sig weapons should cycle through them */
    player_init_defaults(&p, 2);

    /* Give player only 0x81 (super bomb) and sig[0] weapon */
    int idx81 = player_weapon_index(0x81);
    p.weapons[idx81] = 1;
    p.weapons[WEAPON_SIG0_SLOT] = 3;
    /* Clear starting small bombs */
    int idx_small = player_weapon_index(WEAPON_SMALL_BOMB);
    p.weapons[idx_small] = 0;

    /* Start on super bomb */
    p.selected_weapon = 0x81;

    /* Cycle forward: 0x81 -> bomb_sig[0] (0x82 for player 3) */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(0x82, p.selected_weapon);

    /* Cycle forward: 0x82 -> wraps (no more stock) -> back to 0x81 */
    player_cycle_weapon(&p, +1);
    TEST_ASSERT_EQUAL_HEX8(0x81, p.selected_weapon);
}

void test_reset_for_round_base(void)
{
    /* Base case: no upgrades, health should be 100 */
    player_init_defaults(&p, 0);
    p.dead = 1;
    p.active = 0;
    p.health = 0;

    player_reset_for_round(&p);

    TEST_ASSERT_EQUAL(0, p.dead);
    TEST_ASSERT_EQUAL(1, p.active);
    TEST_ASSERT_EQUAL(100, p.health);
    TEST_ASSERT_EQUAL(100, p.max_health);
}

void test_reset_for_round_with_steel_plates(void)
{
    /* With steel plates, health should be steel_plates * 100 + 100
     * Decompiled ref: FUN_1010_c15c (seg_1010:7090) */
    player_init_defaults(&p, 0);
    p.steel_plates = 3;
    p.dead = 1;
    p.health = 0;

    player_reset_for_round(&p);

    TEST_ASSERT_EQUAL(400, p.health);     /* 3 * 100 + 100 */
    TEST_ASSERT_EQUAL(400, p.max_health);
    TEST_ASSERT_EQUAL(0, p.dead);
}

void test_reset_preserves_persistent_fields(void)
{
    /* Cash, weapons, round_wins should persist across rounds.
     * kills, deaths, bombs_placed are per-round accumulators (reset to 0).
     * digging_power resets to 1 per round (seg_1010:7073). */
    player_init_defaults(&p, 1);
    p.cash = 500;
    p.kills = 7;
    p.deaths = 3;
    p.bombs_placed = 15;
    p.round_wins = 2;
    p.digging_power = 150;
    p.steel_plates = 1;
    int idx = player_weapon_index(WEAPON_MEDIUM_BOMB);
    p.weapons[idx] = 10;
    p.dead = 1;

    player_reset_for_round(&p);

    TEST_ASSERT_EQUAL(500, p.cash);
    TEST_ASSERT_EQUAL(0, p.kills);        /* per-round: reset */
    TEST_ASSERT_EQUAL(0, p.deaths);       /* per-round: reset */
    TEST_ASSERT_EQUAL(0, p.bombs_placed); /* per-round: reset */
    TEST_ASSERT_EQUAL(2, p.round_wins);   /* persistent */
    TEST_ASSERT_EQUAL(1, p.digging_power);  /* reset to 1 per round */
    TEST_ASSERT_EQUAL(10, p.weapons[idx]);
    TEST_ASSERT_EQUAL(200, p.health);  /* 1 * 100 + 100 */
}

void test_digging_power_init_value(void)
{
    /* Initial digging_power must be 1, matching original (seg_1010:7073). */
    player_init_defaults(&p, 0);
    TEST_ASSERT_EQUAL(1, p.digging_power);
}

void test_weapon_persists_across_reset(void)
{
    /* selected_weapon and weapons[] persist across player_reset_for_round.
     * Only digging_power and per-round fields (dead, health, etc.) reset.
     * Decompiled ref: process_menu_selection (seg_1010:7203-7214) resets
     * weapons at game start, NOT per round. */
    player_init_defaults(&p, 0);
    int med_idx = player_weapon_index(WEAPON_MEDIUM_BOMB);
    p.weapons[med_idx] = 10;
    p.selected_weapon = WEAPON_MEDIUM_BOMB;
    p.dead = 1;

    player_reset_for_round(&p);

    TEST_ASSERT_EQUAL_HEX8(WEAPON_MEDIUM_BOMB, p.selected_weapon);
    TEST_ASSERT_EQUAL(10, p.weapons[med_idx]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_defaults);
    RUN_TEST(test_weapon_cycle_forward);
    RUN_TEST(test_weapon_cycle_skip_empty);
    RUN_TEST(test_weapon_index_mapping);
    RUN_TEST(test_bomb_sig_per_player);
    RUN_TEST(test_bomb_sig_weapon_index);
    RUN_TEST(test_bomb_sig_in_weapon_cycle);
    RUN_TEST(test_reset_for_round_base);
    RUN_TEST(test_reset_for_round_with_steel_plates);
    RUN_TEST(test_reset_preserves_persistent_fields);
    RUN_TEST(test_digging_power_init_value);
    RUN_TEST(test_weapon_persists_across_reset);
    return UNITY_END();
}
