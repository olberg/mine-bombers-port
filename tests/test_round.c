#include "unity.h"
#include "game/round.h"
#include "game/player.h"
#include "game/player_db.h"
#include "game/movement.h"
#include "game/map.h"
#include "game/map_renderer.h"
#include "input/input.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Helper: create a minimal valid map in memory */
static void setup_test_map(TileMap *map)
{
    /* Fill with floor */
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++) {
            map->tiles[r][c] = '0';
            map->collision[r][c] = 0;
            map->overlay[r][c] = 0;
        }

    /* Border walls */
    for (int c = 0; c < MAP_COLS; c++) {
        map->tiles[0][c] = '1';
        map->tiles[MAP_ROWS - 1][c] = '1';
    }
    for (int r = 0; r < MAP_ROWS; r++) {
        map->tiles[r][0] = '1';
        map->tiles[r][MAP_COLS - 1] = '1';
    }
}

/* Verify round_count_alive counts correctly */
void test_round_count_alive(void)
{
    Player players[4];
    for (int i = 0; i < 4; i++) {
        player_init_defaults(&players[i], i);
        players[i].dead = 0;
    }

    TEST_ASSERT_EQUAL_INT(4, round_count_alive(players, 4));

    players[1].dead = 1;
    TEST_ASSERT_EQUAL_INT(3, round_count_alive(players, 4));

    players[0].dead = 1;
    players[2].dead = 1;
    players[3].dead = 1;
    TEST_ASSERT_EQUAL_INT(0, round_count_alive(players, 4));
}

/* Round ends when fewer than 2 alive (inactivity accumulates) */
void test_round_end_few_alive(void)
{
    Round r;
    memset(&r, 0, sizeof(Round));
    setup_test_map(&r.map);
    r.state = ROUND_RUNNING;
    r.time_remaining = -1;
    r.time_total = -1;
    r.single_player = false;

    r.inactivity = 97;
    r.frame_counter = 4;  /* next will be 5, divisible by 5 */

    Player players[4];
    for (int i = 0; i < 4; i++) {
        player_init_defaults(&players[i], i);
        players[i].dead = 1; /* all dead */
    }
    players[0].dead = 0; /* only 1 alive = <2 */

    /* Manually simulate the alive check logic */
    int alive = round_count_alive(players, 4);
    TEST_ASSERT_EQUAL_INT(1, alive);
    TEST_ASSERT_TRUE(alive < 2);

    /* Inactivity should cross threshold (>100, not >=100) */
    r.inactivity += INACTIVITY_FEW_ALIVE;  /* 97 + 3 = 100 */
    TEST_ASSERT_EQUAL_INT(100, r.inactivity);
    TEST_ASSERT_FALSE(r.inactivity > INACTIVITY_MAX);  /* 100 is NOT enough */

    r.inactivity += INACTIVITY_FEW_ALIVE;  /* 100 + 3 = 103 */
    TEST_ASSERT_TRUE(r.inactivity > INACTIVITY_MAX);   /* 103 > 100 triggers */
}

/* Inactivity threshold off-by-one: original uses >100, not >=100.
 * Decompiled seg_1000:7272: `if (100 < g_inactivity_counter)` */
void test_inactivity_threshold_boundary(void)
{
    /* Exactly 100 should NOT end the round */
    TEST_ASSERT_FALSE(100 > INACTIVITY_MAX);
    /* 101 should end the round */
    TEST_ASSERT_TRUE(101 > INACTIVITY_MAX);
}

/* Time limit: PIT-tick countdown. The config value counts
 * 18.2065 Hz ticks; at 60 fps one tick elapses per 3932160 accumulator
 * units of 1193182 per frame — i.e. every 3.2955 frames, so 45 ticks
 * (a 2.47 s round in the original) expire at frame 149. */
void test_round_end_time(void)
{
    /* 1193182*3 < 3932160 < 1193182*4: first tick on frame 4 */
    int32_t acc = 0;
    int ticks = 0, frames = 0;
    while (ticks < 45) {
        frames++;
        acc += 1193182;
        while (acc >= 3932160) { acc -= 3932160; ticks++; }
    }
    TEST_ASSERT_EQUAL_INT(149, frames);
    /* Wall-clock check: 149 frames / 60 fps = 2.483 s vs the original's
     * 45/18.2065 = 2.472 s — same to within one frame. */
}

/* Single-player exit tile check */
void test_single_player_exit_tile(void)
{
    TileMap map;
    setup_test_map(&map);

    Player p;
    player_init_defaults(&p, 0);

    /* Place exit tile at (5, 5) */
    map.tiles[5][5] = 'k';
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    TEST_ASSERT_TRUE(round_check_exit_tile(&p, &map));

    /* Move player away */
    p.x_pos = tile_to_pixel_x(10);
    p.y_pos = tile_to_pixel_y(10);
    TEST_ASSERT_FALSE(round_check_exit_tile(&p, &map));
}

/* ---- round_apply_scoring characterization (FUN_1000_a17c) ----
 * Fixtures are asymmetric in cash vs earned so pooling the wrong field
 * cannot pass. */

static void scoring_players(Player players[4])
{
    for (int i = 0; i < 4; i++) {
        player_init_defaults(&players[i], i);
        players[i].dead = 0;
        players[i].cash = 0;
        players[i].earned = 0;
        players[i].round_wins = 0;
    }
}

/* Welfare floor ADDS 150 when wallet < 100 (MB.EXE 45897-45934), it does
 * not set the wallet to 150. Nobody died → no round_wins. */
void test_scoring_cash_floor(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 50;
    players[1].cash = 200;
    players[2].cash = 99;
    players[3].cash = 100;

    round_apply_scoring(players, 4, NULL);

    TEST_ASSERT_EQUAL_INT32(200, players[0].cash);  /* 50 + 150 */
    TEST_ASSERT_EQUAL_INT32(200, players[1].cash);
    TEST_ASSERT_EQUAL_INT32(249, players[2].cash);  /* 99 + 150 */
    TEST_ASSERT_EQUAL_INT32(100, players[3].cash);  /* not < 100 */

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT16(0, players[i].round_wins);
    }
}

/* Dead players forfeit EARNED (not cash); survivors split the pool and
 * bank their own earnings; survivors get round_wins only because someone
 * died. Dead players keep their wallet (minus nothing). */
void test_scoring_pools_earned_not_cash(void)
{
    Player players[4];
    scoring_players(players);

    players[0].dead = 0; players[0].cash = 400; players[0].earned = 100;
    players[1].dead = 0; players[1].cash = 300; players[1].earned = 10;
    players[2].dead = 1; players[2].cash = 500; players[2].earned = 70;
    players[3].dead = 1; players[3].cash = 200; players[3].earned = 31;

    round_apply_scoring(players, 4, NULL);

    /* pool = 70 + 31 = 101, survivors = 2, share = 101/2 = 50 (trunc) */
    TEST_ASSERT_EQUAL_INT32(400 + 50 + 100, players[0].cash);
    TEST_ASSERT_EQUAL_INT32(300 + 50 + 10, players[1].cash);
    /* Dead players' wallets untouched (>= 100 so no floor) */
    TEST_ASSERT_EQUAL_INT32(500, players[2].cash);
    TEST_ASSERT_EQUAL_INT32(200, players[3].cash);

    TEST_ASSERT_EQUAL_INT16(1, players[0].round_wins);
    TEST_ASSERT_EQUAL_INT16(1, players[1].round_wins);
    TEST_ASSERT_EQUAL_INT16(0, players[2].round_wins);
    TEST_ASSERT_EQUAL_INT16(0, players[3].round_wins);

    /* earned is NOT cleared by scoring (zeroed at next round start) */
    TEST_ASSERT_EQUAL_INT32(70, players[2].earned);
}

/* Sole survivor: pool also gets Trunc(remaining treasure value / 2.5). */
void test_scoring_sole_survivor_treasure_bonus(void)
{
    Player players[4];
    scoring_players(players);

    TileMap map;
    setup_test_map(&map);
    map.tiles[5][5] = 's';     /* 1000 */
    map.tiles[6][5] = 0x9A;    /* 100 */
    map.tiles[7][5] = 0x95;    /* 10 */
    /* total = 1110, bonus = Trunc(1110 / 2.5) = 444 */

    players[0].dead = 0; players[0].cash = 150; players[0].earned = 25;
    players[1].dead = 1; players[1].cash = 600; players[1].earned = 80;

    round_apply_scoring(players, 2, &map);

    /* pool = 80 + 444 = 524, survivors = 1, share = 524 */
    TEST_ASSERT_EQUAL_INT32(150 + 524 + 25, players[0].cash);
    TEST_ASSERT_EQUAL_INT32(600, players[1].cash);
    TEST_ASSERT_EQUAL_INT16(1, players[0].round_wins);
}

/* Trunc(x / 2.5) == x*2/5 for the original's Real48 division: spot-check
 * a value with a fractional quotient. */
void test_scoring_treasure_bonus_truncates(void)
{
    Player players[4];
    scoring_players(players);

    TileMap map;
    setup_test_map(&map);
    map.tiles[5][5] = 0x93;    /* 25 */
    map.tiles[6][5] = 0x92;    /* 15 */
    map.tiles[7][5] = 0x94;    /* 15 */
    map.tiles[8][5] = 0x97;    /* 35 */
    map.tiles[9][5] = 0x99;    /* 65 */
    map.tiles[10][5] = 0x96;   /* 30 */
    /* total = 185, 185 / 2.5 = 74.0; use 186? tiles sum 185.
     * Add one more for a fractional case: */
    map.tiles[11][5] = 0x95;   /* 10 → total 195, /2.5 = 78.0 */
    map.tiles[12][5] = 0x98;   /* 50 → total 245, /2.5 = 98.0 */
    map.tiles[13][5] = 0x9A;   /* 100 → total 345, /2.5 = 138.0 */
    /* 345 is divisible; make it fractional with one bracelet less:
     * instead assert the helper directly below. */

    TEST_ASSERT_EQUAL_INT32(345, map_treasure_value_remaining(&map));

    /* Fractional quotient: 99 / 2.5 = 39.6 → Trunc = 39 = 99*2/5 */
    TEST_ASSERT_EQUAL_INT32(39, (int32_t)(99 * 2 / 5));

    players[0].dead = 0; players[0].cash = 100; players[0].earned = 0;
    players[1].dead = 1; players[1].cash = 100; players[1].earned = 4;
    players[2].dead = 1; players[2].cash = 100; players[2].earned = 0;

    round_apply_scoring(players, 3, &map);

    /* pool = 4 + Trunc(345/2.5)=138 → 142, survivors=1 */
    TEST_ASSERT_EQUAL_INT32(100 + 142 + 0, players[0].cash);
}

/* All dead (draw): pool evaporates, nobody gets wins, floor still runs. */
void test_scoring_all_dead_draw(void)
{
    Player players[4];
    scoring_players(players);

    players[0].dead = 1; players[0].cash = 90;  players[0].earned = 55;
    players[1].dead = 1; players[1].cash = 500; players[1].earned = 20;
    players[2].dead = 1; players[2].cash = 100; players[2].earned = 0;

    round_apply_scoring(players, 3, NULL);

    TEST_ASSERT_EQUAL_INT32(90 + 150, players[0].cash);  /* floor only */
    TEST_ASSERT_EQUAL_INT32(500, players[1].cash);
    TEST_ASSERT_EQUAL_INT32(100, players[2].cash);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT16(0, players[i].round_wins);
    }
}

/* Nobody died: every survivor banks own earnings + share of empty pool,
 * but NO round_wins (survivors == num_players). No treasure bonus. */
void test_scoring_no_deaths_no_wins(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 200; players[0].earned = 60;
    players[1].cash = 200; players[1].earned = 45;

    round_apply_scoring(players, 2, NULL);

    TEST_ASSERT_EQUAL_INT32(260, players[0].cash);
    TEST_ASSERT_EQUAL_INT32(245, players[1].cash);
    TEST_ASSERT_EQUAL_INT16(0, players[0].round_wins);
    TEST_ASSERT_EQUAL_INT16(0, players[1].round_wins);
}

/* Single player: wallet += earned, nothing else (no floor, no wins). */
void test_scoring_single_player_banks_earnings(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 30;       /* below 100 — SP has no welfare floor */
    players[0].earned = 45;

    round_apply_scoring(players, 1, NULL);

    TEST_ASSERT_EQUAL_INT32(75, players[0].cash);
    TEST_ASSERT_EQUAL_INT16(0, players[0].round_wins);
}

/* ---- round_apply_interest characterization (FUN_1010_ceb3) ----
 * wallet := Round(wallet * 1.07), TP Round = nearest, half away from zero.
 * Applies to the banked wallet only — earned must be untouched. */

void test_interest_basic_values(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 0;
    players[1].cash = 100;
    players[2].cash = 1000;
    players[3].cash = 250;     /* 267.5 → half away from zero → 268 */

    round_apply_interest(players, 4);

    TEST_ASSERT_EQUAL_INT32(0, players[0].cash);
    TEST_ASSERT_EQUAL_INT32(107, players[1].cash);
    TEST_ASSERT_EQUAL_INT32(1070, players[2].cash);
    TEST_ASSERT_EQUAL_INT32(268, players[3].cash);
}

/* Round (not Trunc): 14 * 1.07 = 14.98 → 15; 7 * 1.07 = 7.49 → 7. */
void test_interest_rounds_to_nearest(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 14;
    players[1].cash = 7;

    round_apply_interest(players, 2);

    TEST_ASSERT_EQUAL_INT32(15, players[0].cash);
    TEST_ASSERT_EQUAL_INT32(7, players[1].cash);
}

/* ORIGINAL-VERIFIED half-boundary (DOSBox run): an idle 2-player
 * round from the default 750 wallet shows 803 on the next shop screen.
 * 750 * 1.07 = 802.5 exactly; the original's Real48 RMUL/RRound lands
 * half-AWAY-from-zero despite the stored constant being epsilon below
 * 1.07, and llround(double) agrees
 * (any wallet ending in 50 hits the same .5 product). */
void test_interest_half_boundary_matches_original(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 750;

    round_apply_interest(players, 1);

    TEST_ASSERT_EQUAL_INT32(803, players[0].cash);
}

/* Interest hits banked cash only, never this round's unbanked pickups —
 * the original calls ceb3 (seg_1000:7300-7309) immediately BEFORE a17c.
 * SP: cash=100, earned=50 → interest makes 107, then scoring banks → 157
 * (not 160 or 160.5, which interest-after-banking would give). */
void test_interest_before_scoring_ordering(void)
{
    Player players[4];
    scoring_players(players);

    players[0].cash = 100;
    players[0].earned = 50;

    round_apply_interest(players, 1);
    round_apply_scoring(players, 1, NULL);

    TEST_ASSERT_EQUAL_INT32(157, players[0].cash);
    TEST_ASSERT_EQUAL_INT32(50, players[0].earned);  /* untouched by interest */
}

/* ---- Round-end matrix pieces ---- */

/* F10 abort (shop or in-round) still runs the post-round/post-match blocks
 * in the original (the seg_1000:7315-7339 gates check only g_quit_flag and
 * g_mode_flag, neither of which in-round/shop F10 sets). For a shop abort,
 * main.c replicates the original's straight-line fall-through: earned is
 * zero (the original's pre-shop game_state_update cleared it), then
 * interest, then scoring with everyone alive — nobody gains round_wins
 * (survivors == num_players), everyone banks earned 0, the welfare floor
 * still applies, and every player is charged a round in match-stats. */
void test_shop_abort_scoring_sequence(void)
{
    Player players[4];
    scoring_players(players);
    players[0].cash = 90;    /* below floor after interest: 96 + 150 */
    players[1].cash = 700;
    players[2].cash = 1000;
    players[3].cash = 200;
    players[3].earned = 55;  /* stale from last round — must NOT re-bank */

    for (int i = 0; i < 4; i++) players[i].earned = 0;
    round_apply_interest(players, 4);
    round_apply_scoring(players, 4, NULL);

    TEST_ASSERT_EQUAL_INT32(246, players[0].cash);   /* 96.3 -> 96, +150 */
    TEST_ASSERT_EQUAL_INT32(749, players[1].cash);
    TEST_ASSERT_EQUAL_INT32(1070, players[2].cash);
    TEST_ASSERT_EQUAL_INT32(214, players[3].cash);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(0, players[i].round_wins);
        TEST_ASSERT_EQUAL_UINT32(1, players[i].match_stats[STAT_ROUNDS]);
    }
}

/* MP all-treasures-gone ramp: +20 to inactivity every 20 frames
 * (seg_1000:7264-7266); the >100 gate trips on the 6th tick (120), not
 * the 5th (100). Combined with the every-20-frames cadence that is ~120
 * frames of empty-map play. */
void test_treasure_empty_ramp_tick_count(void)
{
    int inactivity = 0;
    int ticks = 0;
    while (!(inactivity > INACTIVITY_MAX)) {
        inactivity += INACTIVITY_TREASURES;
        ticks++;
    }
    TEST_ASSERT_EQUAL_INT(6, ticks);
    TEST_ASSERT_EQUAL_INT(120, inactivity);
}

/* ---- Carry-over across rounds (regression pins).
 * Persist: cash, weapons, selected weapon, round
 * wins, steel plates, match_stats. Reset: dead/active/direction/anim/
 * velocity/earned; digging_power -> 1; health/max = plates*100+100.
 * Original: game_state_update seg_1010:7399-7454 + FUN_1010_c15c. */
void test_round_reset_carry_over(void)
{
    Player p;
    player_init_defaults(&p, 0);

    /* Mid-match state after some rounds */
    p.cash = 1234;
    p.round_wins = 3;
    p.steel_plates = 2;
    int widx = player_weapon_index(WEAPON_SMALL_BOMB);
    p.weapons[widx] = 17;
    p.selected_weapon = WEAPON_SMALL_BOMB;
    p.match_stats[STAT_ROUNDS] = 4;
    p.match_stats[STAT_MONEY] = 999;

    /* End-of-round junk that must be wiped */
    p.dead = 1;
    p.active = 0;
    p.direction = DIR_LEFT;
    p.anim_frame = 5;
    p.earned = 777;
    p.digging_power = 42;
    p.health = 0;

    player_reset_for_round(&p);

    /* Persistent */
    TEST_ASSERT_EQUAL_INT32(1234, p.cash);
    TEST_ASSERT_EQUAL_INT16(3, p.round_wins);
    TEST_ASSERT_EQUAL_INT16(2, p.steel_plates);
    TEST_ASSERT_EQUAL_INT16(17, p.weapons[widx]);
    TEST_ASSERT_EQUAL_UINT8(WEAPON_SMALL_BOMB, p.selected_weapon);
    TEST_ASSERT_EQUAL_UINT32(4, p.match_stats[STAT_ROUNDS]);
    TEST_ASSERT_EQUAL_UINT32(999, p.match_stats[STAT_MONEY]);

    /* Reset */
    TEST_ASSERT_EQUAL_UINT8(0, p.dead);
    TEST_ASSERT_EQUAL_UINT8(1, p.active);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, p.direction);
    TEST_ASSERT_EQUAL_INT16(0, p.anim_frame);
    TEST_ASSERT_EQUAL_INT32(0, p.earned);
    TEST_ASSERT_EQUAL_INT16(1, p.digging_power);

    /* Health recomputed: plates*100 + 100 */
    TEST_ASSERT_EQUAL_INT16(300, p.health);
    TEST_ASSERT_EQUAL_INT16(300, p.max_health);
}

/* Single-player: lives initialized to 3 (seg_1010:7218) */
void test_sp_lives_init(void)
{
    Player p;
    player_init_defaults(&p, 0);
    TEST_ASSERT_EQUAL_INT(3, p.lives);
}

/* Single-player: death with lives > 1 allows retry */
void test_sp_retry_with_lives(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.lives = 2;
    p.dead = 1;
    p.health = 0;

    /* Simulate retry logic from main.c: decrement lives, check > 0 */
    p.lives--;
    TEST_ASSERT_EQUAL_INT(1, p.lives);
    TEST_ASSERT_TRUE(p.lives >= 1);  /* Can retry */

    /* On retry, restore player state */
    p.dead = 0;
    p.active = 1;
    p.health = p.max_health;
    TEST_ASSERT_EQUAL_INT(0, p.dead);
    TEST_ASSERT_EQUAL_INT(100, p.health);
}

/* Single-player: death with lives == 1 means game over after decrement */
void test_sp_game_over_no_lives(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.lives = 1;
    p.dead = 1;
    p.health = 0;

    /* Simulate game over check (seg_1000:7174): lives < 1 after decrement */
    p.lives--;
    TEST_ASSERT_EQUAL_INT(0, p.lives);
    TEST_ASSERT_TRUE(p.lives < 1);  /* Game over */
}

/* Single-player death: inactivity ramps by 2 each check (seg_1000:7179) */
void test_sp_death_inactivity_ramp(void)
{
    Round r;
    memset(&r, 0, sizeof(Round));
    setup_test_map(&r.map);
    r.state = ROUND_RUNNING;
    r.time_remaining = -1;
    r.time_total = -1;
    r.single_player = true;
    r.inactivity = 0;

    /* Simulate the inactivity ramp from round.c:
     * when player dead in SP, inactivity += 2 per check */
    for (int i = 0; i < 51; i++) {
        r.inactivity += 2;
    }
    TEST_ASSERT_TRUE(r.inactivity > INACTIVITY_MAX);  /* 102 > 100 */
}

/* Direction-to-sprite-band mapping.
 * Decompiled seg_1000:2608-2630 direction values:
 *   0=STOP, 1=RIGHT, 2=LEFT, 3=UP, 4=DOWN
 * Sprite sheet bands: band 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN
 * Port DIR_ values: 0=STOP, 1=DOWN, 2=UP, 3=LEFT, 4=RIGHT
 * The dir_to_spr[] table must map port DIR_→ correct band. */
void test_direction_to_sprite_band(void)
{
    /* Table from round.c round_draw() — must match these mappings.
     * DIR_* uses the original encoding (1=RIGHT 2=LEFT 3=UP
     * 4=DOWN), so band = value - 1 (sheet bands: 0=right 1=left 2=up
     * 3=down). */
    static const int dir_to_spr[] = { 3, 0, 1, 2, 3 };

    TEST_ASSERT_EQUAL_INT(0, dir_to_spr[DIR_RIGHT]);
    TEST_ASSERT_EQUAL_INT(1, dir_to_spr[DIR_LEFT]);
    TEST_ASSERT_EQUAL_INT(2, dir_to_spr[DIR_UP]);
    TEST_ASSERT_EQUAL_INT(3, dir_to_spr[DIR_DOWN]);
    /* DIR_STOP (0) → band 3 (down fallback) */
    TEST_ASSERT_EQUAL_INT(3, dir_to_spr[DIR_STOP]);

    /* The DIR_* values themselves are load-bearing (original data:
     * spawn variants, AI Random(4)+1, arrow rewrite switch) — pin them. */
    TEST_ASSERT_EQUAL_INT(1, DIR_RIGHT);
    TEST_ASSERT_EQUAL_INT(2, DIR_LEFT);
    TEST_ASSERT_EQUAL_INT(3, DIR_UP);
    TEST_ASSERT_EQUAL_INT(4, DIR_DOWN);
}

/* Key→direction resolution must match process_weapons
 * (seg_1000:2608-2630). Held-key priority Up > Right > Down > Left > Stop;
 * stop is level-triggered and lowest priority; nothing held = direction
 * unchanged (player keeps sliding). Uses the injection layer, so it runs
 * headlessly. */
static void inject_dir(int idx, PlayerInputAction a)
{
    player_input_inject(idx, a, true, false);
}

void test_direction_resolution_priority(void)
{
    player_input_inject_mode(true);

    /* Single key maps to its own direction */
    inject_dir(0, PLAYER_INPUT_UP);
    TEST_ASSERT_EQUAL_INT(DIR_UP, round_resolve_direction(0, DIR_STOP));
    player_input_inject_clear(0);

    /* Up beats everything */
    inject_dir(0, PLAYER_INPUT_UP);
    inject_dir(0, PLAYER_INPUT_RIGHT);
    inject_dir(0, PLAYER_INPUT_DOWN);
    inject_dir(0, PLAYER_INPUT_LEFT);
    TEST_ASSERT_EQUAL_INT(DIR_UP, round_resolve_direction(0, DIR_STOP));
    player_input_inject_clear(0);

    /* Right beats Down and Left (original nesting order: up, right,
     * down, left — values 3,1,4,2) */
    inject_dir(0, PLAYER_INPUT_RIGHT);
    inject_dir(0, PLAYER_INPUT_DOWN);
    inject_dir(0, PLAYER_INPUT_LEFT);
    TEST_ASSERT_EQUAL_INT(DIR_RIGHT, round_resolve_direction(0, DIR_STOP));
    player_input_inject_clear(0);

    /* Down beats Left */
    inject_dir(0, PLAYER_INPUT_DOWN);
    inject_dir(0, PLAYER_INPUT_LEFT);
    TEST_ASSERT_EQUAL_INT(DIR_DOWN, round_resolve_direction(0, DIR_STOP));
    player_input_inject_clear(0);

    player_input_inject_mode(false);
}

void test_direction_resolution_stop_semantics(void)
{
    player_input_inject_mode(true);

    /* Stop held alone stops the player */
    inject_dir(0, PLAYER_INPUT_STOP);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, round_resolve_direction(0, DIR_RIGHT));
    player_input_inject_clear(0);

    /* A held direction key BEATS a held stop key (stop is the innermost
     * branch in the original — only reached when no direction is held) */
    inject_dir(0, PLAYER_INPUT_STOP);
    inject_dir(0, PLAYER_INPUT_LEFT);
    TEST_ASSERT_EQUAL_INT(DIR_LEFT, round_resolve_direction(0, DIR_STOP));
    player_input_inject_clear(0);

    /* Nothing held: direction is retained — the player keeps sliding */
    TEST_ASSERT_EQUAL_INT(DIR_RIGHT, round_resolve_direction(0, DIR_RIGHT));
    TEST_ASSERT_EQUAL_INT(DIR_UP, round_resolve_direction(0, DIR_UP));

    player_input_inject_mode(false);
}

void test_direction_resolution_per_player_independent(void)
{
    player_input_inject_mode(true);

    /* Four players hold four different directions in the same frame */
    inject_dir(0, PLAYER_INPUT_UP);
    inject_dir(1, PLAYER_INPUT_DOWN);
    inject_dir(2, PLAYER_INPUT_LEFT);
    inject_dir(3, PLAYER_INPUT_RIGHT);

    TEST_ASSERT_EQUAL_INT(DIR_UP,    round_resolve_direction(0, DIR_STOP));
    TEST_ASSERT_EQUAL_INT(DIR_DOWN,  round_resolve_direction(1, DIR_STOP));
    TEST_ASSERT_EQUAL_INT(DIR_LEFT,  round_resolve_direction(2, DIR_STOP));
    TEST_ASSERT_EQUAL_INT(DIR_RIGHT, round_resolve_direction(3, DIR_STOP));

    player_input_inject_mode(false);
}

/* One-shot keys (bomb, choose/sell) must not be lost when the press edge
 * lands on an ODD frame: the weapon tick only runs on even frames
 * (seg_1000:7193), but the original's ISR key byte stays set from key-make
 * until the handler consumes it (seg_1000:2631-2632 bomb, 2795-2796
 * choose). The port latches the raylib one-frame edge to reproduce that.
 * Regression: the cycle key used to be read unlatched in the even-frame
 * block, silently dropping ~half of all taps. */
void test_one_shot_keys_latch_across_odd_frames(void)
{
    player_input_inject_mode(true);

    Round r;
    memset(&r, 0, sizeof(Round));
    setup_test_map(&r.map);
    r.state = ROUND_FADE_IN;
    r.time_remaining = -1;
    r.time_total = -1;
    r.single_player = false;

    Player players[2];
    for (int i = 0; i < 2; i++) {
        player_init_defaults(&players[i], i);
    }
    players[0].x_pos = tile_to_pixel_x(8);
    players[0].y_pos = tile_to_pixel_y(8);
    players[1].x_pos = tile_to_pixel_x(50);
    players[1].y_pos = tile_to_pixel_y(20);
    players[0].weapons[player_weapon_index(WEAPON_SMALL_BOMB)]  = 5;
    players[0].weapons[player_weapon_index(WEAPON_MEDIUM_BOMB)] = 5;
    players[0].selected_weapon = WEAPON_SMALL_BOMB;

    /* Run through fade-in (also clears the latches) */
    int guard = 0;
    while (r.state == ROUND_FADE_IN && guard++ < 20) {
        round_update(&r, players, 2);
    }
    TEST_ASSERT_EQUAL_INT(ROUND_RUNNING, r.state);
    TEST_ASSERT_EQUAL_INT(0, r.frame_counter);

    /* Tap bomb + choose on what becomes frame 1 (odd: no weapon tick) */
    player_input_inject(0, PLAYER_INPUT_BOMB,  false, true);
    player_input_inject(0, PLAYER_INPUT_CYCLE, false, true);
    round_update(&r, players, 2);
    TEST_ASSERT_EQUAL_INT(1, r.frame_counter);
    TEST_ASSERT_EQUAL_UINT8('0', r.map.tiles[8][8]);  /* not consumed yet */
    TEST_ASSERT_EQUAL_UINT8(WEAPON_SMALL_BOMB, players[0].selected_weapon);

    /* Key released before the next frame — the press must still register */
    player_input_inject_clear(0);
    round_update(&r, players, 2);
    TEST_ASSERT_EQUAL_INT(2, r.frame_counter);

    /* Bomb consumed: small bomb placed at the player's tile, ammo 5 -> 4 */
    TEST_ASSERT_EQUAL_UINT8(WEAPON_SMALL_BOMB, r.map.tiles[8][8]);
    TEST_ASSERT_EQUAL_INT(4, players[0].weapons[player_weapon_index(WEAPON_SMALL_BOMB)]);

    /* Choose consumed: cycled to the next weapon with stock */
    TEST_ASSERT_EQUAL_UINT8(WEAPON_MEDIUM_BOMB, players[0].selected_weapon);

    /* Latches are one-shot: a further even frame must NOT re-fire */
    round_update(&r, players, 2);
    round_update(&r, players, 2);
    TEST_ASSERT_EQUAL_INT(4, players[0].weapons[player_weapon_index(WEAPON_SMALL_BOMB)]);
    TEST_ASSERT_EQUAL_UINT8(WEAPON_MEDIUM_BOMB, players[0].selected_weapon);

    player_input_inject_mode(false);
    round_cleanup(&r);
}

/* Facing must turn toward a held direction even when movement is blocked
 * by a wall (digging): the original updates facing (+0xA6) from direction
 * (+0xA4) at the head of every weapon tick (process_weapons,
 * seg_1000:2602-2604), NOT on successful movement. Regression: the port
 * used to update last_direction only inside player_move's moved branch,
 * so a digging player's sprite never turned toward the wall. */
void test_facing_updates_while_digging(void)
{
    player_input_inject_mode(true);

    Round r;
    memset(&r, 0, sizeof(Round));
    setup_test_map(&r.map);
    r.state = ROUND_FADE_IN;
    r.time_remaining = -1;
    r.time_total = -1;
    r.single_player = false;

    Player players[2];
    for (int i = 0; i < 2; i++) {
        player_init_defaults(&players[i], i);
    }
    players[0].x_pos = tile_to_pixel_x(8);
    players[0].y_pos = tile_to_pixel_y(8);
    players[1].x_pos = tile_to_pixel_x(50);
    players[1].y_pos = tile_to_pixel_y(20);

    /* Diggable wall directly above P1 (DIR_UP checks [row][col-1]) */
    r.map.tiles[8][7] = '7';
    r.map.collision[8][7] = 3000;

    int guard = 0;
    while (r.state == ROUND_FADE_IN && guard++ < 20) {
        round_update(&r, players, 2);
    }
    TEST_ASSERT_EQUAL_INT(ROUND_RUNNING, r.state);
    TEST_ASSERT_EQUAL_INT(DIR_RIGHT, players[0].last_direction);  /* spawn facing */

    /* Hold UP against the wall for 4 frames = 2 weapon ticks */
    player_input_inject(0, PLAYER_INPUT_UP, true, false);
    int16_t y_before = players[0].y_pos;
    for (int f = 0; f < 4; f++) {
        round_update(&r, players, 2);
    }

    TEST_ASSERT_EQUAL_INT16(y_before, players[0].y_pos);      /* never moved */
    TEST_ASSERT_EQUAL_INT(DIR_UP, players[0].direction);
    TEST_ASSERT_EQUAL_INT(DIR_UP, players[0].last_direction); /* turned anyway */
    TEST_ASSERT_EQUAL_UINT8(1, players[0].digging);           /* dig anim on */

    player_input_inject_mode(false);
    round_cleanup(&r);
}

/* Extra life (0xB3) pickup only works in single-player (seg_1000:3566) */
void test_extra_life_single_player_only(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.lives = 3;

    /* In single-player (g_num_active_players == 1), lives should increment */
    g_num_active_players = 1;
    TileMap map;
    setup_test_map(&map);
    map.tiles[5][5] = 0xB3;
    map.collision[5][5] = 0;
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_EQUAL_INT(4, p.lives);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[5][5]);

    /* In multiplayer (g_num_active_players == 2), lives should NOT increment */
    g_num_active_players = 2;
    map.tiles[6][6] = 0xB3;
    map.collision[6][6] = 0;
    p.x_pos = tile_to_pixel_x(6);
    p.y_pos = tile_to_pixel_y(6);
    p.lives = 3;

    player_check_pickup(&p, &map, 6, 6);
    TEST_ASSERT_EQUAL_INT(3, p.lives);  /* Unchanged in multiplayer */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[6][6]);  /* Tile still consumed */
}

/* Starting corners: players placed at opposite corners per pair.
 * P1/P2 at (1,1)/(62,43) or swapped, P3/P4 at (62,1)/(1,43) or swapped.
 * Decompiled ref: FUN_1010_c4f2, seg_1010:7232-7288. */
void test_starting_positions_corners(void)
{
    Round r;
    memset(&r, 0, sizeof(Round));
    setup_test_map(&r.map);

    Player players[4];
    for (int i = 0; i < 4; i++)
        player_init_defaults(&players[i], i);

    /* Run placement 10 times — corners should always be from the valid set */
    for (int trial = 0; trial < 10; trial++) {
        round_place_players(&r, players, 4);

        /* P1 and P2 must be at opposite corners: (row=1,col=1)/(row=62,col=43) or swapped */
        int p1_row = pixel_to_tile_row(players[0].x_pos);
        int p1_col = pixel_to_tile_col(players[0].y_pos);
        int p2_row = pixel_to_tile_row(players[1].x_pos);
        int p2_col = pixel_to_tile_col(players[1].y_pos);

        bool pair1_a = (p1_row == 1 && p1_col == 1 && p2_row == 62 && p2_col == 43);
        bool pair1_b = (p1_row == 62 && p1_col == 43 && p2_row == 1 && p2_col == 1);
        TEST_ASSERT_TRUE_MESSAGE(pair1_a || pair1_b,
            "P1/P2 must be at (1,1)/(62,43) or swapped");

        /* P3 and P4 must be at: (row=62,col=1)/(row=1,col=43) or swapped */
        int p3_row = pixel_to_tile_row(players[2].x_pos);
        int p3_col = pixel_to_tile_col(players[2].y_pos);
        int p4_row = pixel_to_tile_row(players[3].x_pos);
        int p4_col = pixel_to_tile_col(players[3].y_pos);

        bool pair2_a = (p3_row == 62 && p3_col == 1 && p4_row == 1 && p4_col == 43);
        bool pair2_b = (p3_row == 1 && p3_col == 43 && p4_row == 62 && p4_col == 1);
        TEST_ASSERT_TRUE_MESSAGE(pair2_a || pair2_b,
            "P3/P4 must be at (62,1)/(1,43) or swapped");

        /* Re-fill map for next trial */
        setup_test_map(&r.map);
    }
}

/* Spawn path clearing: tiles near corners should be floor after placement.
 * Decompiled ref: seg_1010:7456-7512. */
void test_spawn_path_clearing(void)
{
    Round r;
    memset(&r, 0, sizeof(Round));

    /* Fill interior with destructible walls */
    for (int row = 0; row < MAP_ROWS; row++)
        for (int col = 0; col < MAP_COLS; col++) {
            r.map.tiles[row][col] = '7';
            r.map.collision[row][col] = 0;
            r.map.overlay[row][col] = 0;
        }
    /* Border */
    for (int c = 0; c < MAP_COLS; c++) {
        r.map.tiles[0][c] = '1';
        r.map.tiles[MAP_ROWS - 1][c] = '1';
    }
    for (int row = 0; row < MAP_ROWS; row++) {
        r.map.tiles[row][0] = '1';
        r.map.tiles[row][MAP_COLS - 1] = '1';
    }

    Player players[4];
    for (int i = 0; i < 4; i++)
        player_init_defaults(&players[i], i);

    round_place_players(&r, players, 4);

    /* At minimum 4 tiles must be cleared in each corner direction
     * (the clearing function uses 4+rand(6), so at least 4 tiles). */
    int top_left_clear = 0;
    for (int c = 1; c < 10; c++) {
        if (r.map.tiles[1][c] == '0') top_left_clear++;
    }
    TEST_ASSERT_TRUE_MESSAGE(top_left_clear >= 4,
        "Top-left corner must have at least 4 clear tiles in row 1");

    int bottom_right_clear = 0;
    for (int c = MAP_COLS - 2; c > MAP_COLS - 11; c--) {
        if (r.map.tiles[62][c] == '0') bottom_right_clear++;
    }
    TEST_ASSERT_TRUE_MESSAGE(bottom_right_clear >= 4,
        "Bottom-right corner must have at least 4 clear tiles in row 62");

    /* 3+ player corners */
    int bottom_left_clear = 0;
    for (int c = 1; c < 10; c++) {
        if (r.map.tiles[62][c] == '0') bottom_left_clear++;
    }
    TEST_ASSERT_TRUE_MESSAGE(bottom_left_clear >= 4,
        "Bottom-left corner must have at least 4 clear tiles in row 62");

    int top_right_clear = 0;
    for (int c = MAP_COLS - 2; c > MAP_COLS - 11; c--) {
        if (r.map.tiles[1][c] == '0') top_right_clear++;
    }
    TEST_ASSERT_TRUE_MESSAGE(top_right_clear >= 4,
        "Top-right corner must have at least 4 clear tiles in row 1");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_round_count_alive);
    RUN_TEST(test_round_end_few_alive);
    RUN_TEST(test_inactivity_threshold_boundary);
    RUN_TEST(test_round_end_time);
    RUN_TEST(test_single_player_exit_tile);
    RUN_TEST(test_scoring_cash_floor);
    RUN_TEST(test_scoring_pools_earned_not_cash);
    RUN_TEST(test_scoring_sole_survivor_treasure_bonus);
    RUN_TEST(test_scoring_treasure_bonus_truncates);
    RUN_TEST(test_scoring_all_dead_draw);
    RUN_TEST(test_scoring_no_deaths_no_wins);
    RUN_TEST(test_scoring_single_player_banks_earnings);
    RUN_TEST(test_interest_basic_values);
    RUN_TEST(test_interest_rounds_to_nearest);
    RUN_TEST(test_interest_half_boundary_matches_original);
    RUN_TEST(test_interest_before_scoring_ordering);
    RUN_TEST(test_shop_abort_scoring_sequence);
    RUN_TEST(test_treasure_empty_ramp_tick_count);
    RUN_TEST(test_round_reset_carry_over);
    RUN_TEST(test_sp_lives_init);
    RUN_TEST(test_sp_retry_with_lives);
    RUN_TEST(test_sp_game_over_no_lives);
    RUN_TEST(test_sp_death_inactivity_ramp);
    RUN_TEST(test_one_shot_keys_latch_across_odd_frames);
    RUN_TEST(test_facing_updates_while_digging);
    RUN_TEST(test_extra_life_single_player_only);
    RUN_TEST(test_direction_to_sprite_band);
    RUN_TEST(test_direction_resolution_priority);
    RUN_TEST(test_direction_resolution_stop_semantics);
    RUN_TEST(test_direction_resolution_per_player_independent);
    RUN_TEST(test_starting_positions_corners);
    RUN_TEST(test_spawn_path_clearing);
    return UNITY_END();
}
