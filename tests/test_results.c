#include "unity.h"
#include "game/results.h"
#include "game/config.h"
#include "game/player_db.h"
#include <string.h>

/* Tests the REAL results_compute_rank (results.c), which switches on the
 * winner-by option (option_toggle[3]): 0 = wallet (FUN_1000_96c6),
 * nonzero = round wins (FUN_1000_9640). */

void setUp(void)
{
    config_set_defaults();
    g_config.option_toggle[3] = 0;  /* winner by money */
}
void tearDown(void) {}

/* Clear winner in 2-player game (by money) */
void test_rank_2p_clear_winner(void)
{
    Player players[2];
    memset(players, 0, sizeof(players));
    players[0].cash = 1000;
    players[1].cash = 500;

    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(0, players, 2));
    TEST_ASSERT_EQUAL_INT(3, results_compute_rank(1, players, 2));
}

/* Tie in 2-player game */
void test_rank_2p_tie(void)
{
    Player players[2];
    memset(players, 0, sizeof(players));
    players[0].cash = 500;
    players[1].cash = 500;

    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(0, players, 2));
    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(1, players, 2));
}

/* 4-player game with distinct scores */
void test_rank_4p_distinct(void)
{
    Player players[4];
    memset(players, 0, sizeof(players));
    players[0].cash = 400;
    players[1].cash = 300;
    players[2].cash = 200;
    players[3].cash = 100;

    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(0, players, 4));
    TEST_ASSERT_EQUAL_INT(1, results_compute_rank(1, players, 4));
    TEST_ASSERT_EQUAL_INT(2, results_compute_rank(2, players, 4));
    TEST_ASSERT_EQUAL_INT(3, results_compute_rank(3, players, 4));
}

/* 3-player game, two tied for first */
void test_rank_3p_tied_first(void)
{
    Player players[3];
    memset(players, 0, sizeof(players));
    players[0].cash = 500;
    players[1].cash = 500;
    players[2].cash = 200;

    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(0, players, 3));
    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(1, players, 3));
    TEST_ASSERT_EQUAL_INT(3, results_compute_rank(2, players, 3));
}

/* Winner-by = wins (option_toggle[3] != 0): cash must be IGNORED.
 * Fixture is asymmetric so ranking by the wrong metric cannot pass. */
void test_rank_by_wins_ignores_cash(void)
{
    g_config.option_toggle[3] = 1;

    Player players[3];
    memset(players, 0, sizeof(players));
    players[0].cash = 100;  players[0].round_wins = 5;   /* poor but winning */
    players[1].cash = 9000; players[1].round_wins = 2;
    players[2].cash = 5000; players[2].round_wins = 2;

    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(0, players, 3));
    TEST_ASSERT_EQUAL_INT(1, results_compute_rank(1, players, 3));
    TEST_ASSERT_EQUAL_INT(1, results_compute_rank(2, players, 3));
}

/* Winner-by = money (default): round wins must be IGNORED. */
void test_rank_by_money_ignores_wins(void)
{
    Player players[2];
    memset(players, 0, sizeof(players));
    players[0].cash = 100;  players[0].round_wins = 9;
    players[1].cash = 2000; players[1].round_wins = 0;

    TEST_ASSERT_EQUAL_INT(3, results_compute_rank(0, players, 2));
    TEST_ASSERT_EQUAL_INT(0, results_compute_rank(1, players, 2));
}

/* Results screen match-stats writes: matches +1 for everyone, match wins
 * +1 for rank-0 only (seg_1000:6093/6115/6157). */
void test_accumulate_match_stats(void)
{
    Player players[3];
    memset(players, 0, sizeof(players));
    players[0].cash = 700;
    players[1].cash = 700;
    players[2].cash = 100;

    results_accumulate_match_stats(players, 3);

    TEST_ASSERT_EQUAL_UINT32(1, players[0].match_stats[STAT_MATCHES]);
    TEST_ASSERT_EQUAL_UINT32(1, players[1].match_stats[STAT_MATCHES]);
    TEST_ASSERT_EQUAL_UINT32(1, players[2].match_stats[STAT_MATCHES]);
    TEST_ASSERT_EQUAL_UINT32(1, players[0].match_stats[STAT_MATCH_WINS]);
    TEST_ASSERT_EQUAL_UINT32(1, players[1].match_stats[STAT_MATCH_WINS]);
    TEST_ASSERT_EQUAL_UINT32(0, players[2].match_stats[STAT_MATCH_WINS]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rank_2p_clear_winner);
    RUN_TEST(test_rank_2p_tie);
    RUN_TEST(test_rank_4p_distinct);
    RUN_TEST(test_rank_3p_tied_first);
    RUN_TEST(test_rank_by_wins_ignores_cash);
    RUN_TEST(test_rank_by_money_ignores_wins);
    RUN_TEST(test_accumulate_match_stats);
    return UNITY_END();
}
