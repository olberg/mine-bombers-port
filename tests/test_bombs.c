#include "unity.h"
#include "util/prng.h"
#include "game/bombs.h"
#include "game/weapons.h"
#include "game/map.h"
#include "game/map_renderer.h"
#include "game/movement.h"
#include "game/player.h"
#include "game/sprites.h"
#include <string.h>
#include <stdlib.h>

/* Helper: set up a minimal test map with floor tiles */
static void setup_test_map(TileMap *map)
{
    memset(map, 0, sizeof(*map));
    memset(map->bomb_owner, 0xFF, sizeof(map->bomb_owner));
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map->tiles[r][c] = '0';
}

/* Helper: set up a player at a given tile position with a weapon.
 * VGA convention: x_pos = row * TILE_SIZE (screen X), y_pos = col * TILE_SIZE + MAP_Y_OFFSET (screen Y). */
static void setup_test_player(Player *p, int col, int row, uint8_t weapon, int16_t qty)
{
    player_init_defaults(p, 0);
    p->x_pos = (int16_t)(row * TILE_SIZE);
    p->y_pos = (int16_t)(col * TILE_SIZE + MAP_Y_OFFSET);
    p->selected_weapon = weapon;
    int idx = player_weapon_index(weapon);
    if (idx >= 0) p->weapons[idx] = qty;
    p->direction = DIR_DOWN;
    p->last_direction = DIR_DOWN;
}

void setUp(void) { mb_prng_set_seed(42u); }
void tearDown(void) {}

/* --- Test: bomb placement sets tile, collision, overlay, decrements inventory --- */
void test_place_bomb(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_SMALL_BOMB, 3);

    bool placed = bomb_place(&p, &map);

    TEST_ASSERT_TRUE(placed);
    TEST_ASSERT_EQUAL_UINT8(WEAPON_SMALL_BOMB, map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT16(BOMB_COLLISION_DEFAULT, map.collision[10][5]);
    TEST_ASSERT_EQUAL_UINT16(100, map.overlay[10][5]);

    int idx = player_weapon_index(WEAPON_SMALL_BOMB);
    TEST_ASSERT_EQUAL_INT16(2, p.weapons[idx]);
}

/* --- Test: placement fails with 0 inventory --- */
void test_place_bomb_no_ammo(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_SMALL_BOMB, 0);

    bool placed = bomb_place(&p, &map);

    TEST_ASSERT_FALSE(placed);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);
}

/* --- Test: placement fails on occupied tile --- */
void test_place_bomb_occupied_tile(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_SMALL_BOMB, 3);
    map.collision[10][5] = 20;  /* already a bomb here */

    bool placed = bomb_place(&p, &map);

    TEST_ASSERT_FALSE(placed);
}

/* --- Test: fuse countdown from 5 → 0 triggers detonation --- */
void test_fuse_countdown(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a "bomb" tile with overlay=5 */
    map.tiles[10][5] = WEAPON_SMALL_BOMB;
    map.overlay[10][5] = 5;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;

    /* Update 4 times: overlay goes 5→4→3→2→... but small bomb also transitions */
    for (int i = 0; i < 4; i++)
        bombs_update(&map);

    /* Overlay should be 1 (5 - 4 = 1), about to detonate */
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[10][5]);

    /* One more update: detonation triggers (overlay was 1) */
    bombs_update(&map);

    /* After detonation, tile should be cleared to floor */
    TEST_ASSERT_EQUAL_UINT16(0, map.overlay[10][5]);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);
}

/* --- Test: small bomb visual stages W → w → x --- */
void test_visual_stage_small(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = BOMB_SMALL_1;  /* 'W' */
    map.overlay[10][5] = 100;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;

    /* Count down from 100 to 60: should still be 'W' */
    for (int i = 0; i < 40; i++)
        bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_SMALL_1, map.tiles[10][5]);  /* overlay=60, not yet <60 */

    /* One more tick: overlay becomes 59 (<60), transitions to 'w' */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_SMALL_2, map.tiles[10][5]);  /* 'w' */

    /* Count down to overlay 30: should still be 'w' */
    for (int i = 0; i < 29; i++)
        bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_SMALL_2, map.tiles[10][5]);  /* overlay=30, not yet <30 */

    /* One more tick: overlay becomes 29 (<30), transitions to 'x' */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_SMALL_3, map.tiles[10][5]);  /* 'x' */
}

/* --- Test: medium bomb visual stages X → 0x8B → 0x8C --- */
void test_visual_stage_medium(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = BOMB_MEDIUM_1;  /* 'X' */
    map.overlay[10][5] = 100;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;

    /* Count down from 100 to 60 */
    for (int i = 0; i < 40; i++)
        bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MEDIUM_1, map.tiles[10][5]);

    /* One more: overlay=59, transition to 0x8B */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MEDIUM_2, map.tiles[10][5]);

    /* Down to overlay=30 */
    for (int i = 0; i < 29; i++)
        bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MEDIUM_2, map.tiles[10][5]);

    /* One more: overlay=29, transition to 0x8C */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MEDIUM_3, map.tiles[10][5]);
}

/* --- Test: large bomb visual stages Y → 0x8D → 0x8E --- */
void test_visual_stage_large(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = BOMB_LARGE_1;  /* 'Y' */
    map.overlay[10][5] = 80;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;

    /* Count down from 80 to 40 */
    for (int i = 0; i < 40; i++)
        bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_LARGE_1, map.tiles[10][5]);

    /* One more: overlay=39, transition to 0x8D */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_LARGE_2, map.tiles[10][5]);

    /* Down to overlay=20 */
    for (int i = 0; i < 19; i++)
        bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_LARGE_2, map.tiles[10][5]);

    /* One more: overlay=19, transition to 0x8E */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_LARGE_3, map.tiles[10][5]);
}

/* --- Test: detonation degrades adjacent destructible wall --- */
void test_detonation_destroys_wall(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a destructible wall next to the bomb */
    map.tiles[10][6] = '7';
    map.collision[10][6] = 1227;  /* standard wall HP */

    /* Detonate at (5, 10) with radius reaching (6, 10) */
    map.tiles[10][5] = BOMB_SMALL_1;
    bomb_detonate(&map, 5, 10);

    /* Small bomb uses weak mode: wall should degrade to 'p'/'q' with HP 500/1000,
     * NOT be destroyed (matching original FUN_1010_1326 param_2=0 behavior). */
    TEST_ASSERT_TRUE(map.tiles[10][6] == 'p' || map.tiles[10][6] == 'q');
    TEST_ASSERT_TRUE(map.collision[10][6] == 500 || map.collision[10][6] == 1000);
}

/* --- Test: detonation leaves indestructible walls intact --- */
void test_detonation_indestructible(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][6] = '1';
    map.collision[10][6] = 30000;  /* indestructible sentinel (original value) */

    map.tiles[10][5] = BOMB_SMALL_1;
    bomb_detonate(&map, 5, 10);

    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[10][6]);
    TEST_ASSERT_EQUAL_UINT16(30000, map.collision[10][6]);
}

/* --- Test: treasure drop rate is approximately 1.1% --- */
void test_treasure_drop_rate(void)
{
    TileMap map;
    int drops = 0;
    int trials = 10000;

    mb_prng_set_seed(12345u);  /* fixed seed for reproducibility */

    for (int i = 0; i < trials; i++) {
        setup_test_map(&map);
        map.tiles[10][5] = BOMB_SMALL_3;  /* 'x' - eligible for drop */
        map.collision[10][5] = 20;

        if (bomb_check_treasure_drop(&map, 5, 10)) {
            drops++;
        }
    }

    /* Expected: ~110 drops (1.1%). Allow wide margin: 30-250. */
    TEST_ASSERT_GREATER_THAN(30, drops);
    TEST_ASSERT_LESS_THAN(250, drops);
}

/* --- Test: money bomb (SUPERpora) loans +300 DIGGING POWER, no tile,
 * no cash change, no ammo consumed (seg_1000:2675-2686) --- */
void test_money_bomb(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_MONEY_BOMB, 2);
    int32_t starting_cash = p.cash;
    int16_t starting_dig = p.digging_power;

    bool used = bomb_place(&p, &map);

    TEST_ASSERT_TRUE(used);
    TEST_ASSERT_EQUAL_INT16(starting_dig + MONEY_BOMB_DIG_LOAN,
                            p.digging_power);
    TEST_ASSERT_EQUAL_INT32(starting_cash, p.cash);  /* cash untouched */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);  /* no tile placed */
    /* Money bomb uses loan counter, NOT general cooldown */
    TEST_ASSERT_EQUAL_INT32(MONEY_BOMB_COOLDOWN, p.money_bomb_counter);
    TEST_ASSERT_EQUAL_INT16(0, p.cooldown);  /* general cooldown unaffected */

    /* The original's money-bomb branch never reaches the inventory
     * decrement — using it consumes NO ammo. */
    int idx = player_weapon_index(WEAPON_MONEY_BOMB);
    TEST_ASSERT_EQUAL_INT16(2, p.weapons[idx]);
}

/* --- Test: money bomb blocked during loan counter active --- */
void test_money_bomb_cooldown(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_MONEY_BOMB, 2);
    p.money_bomb_counter = 5;  /* loan still active */

    bool used = bomb_place(&p, &map);
    TEST_ASSERT_FALSE(used);  /* blocked: counter != 0 */
}

/* --- Test: dig-power loan deduction after countdown (seg_1010:7662-7678) --- */
void test_money_bomb_loan_deduction(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.digging_power = 301;     /* 1 + active +300 loan */
    p.cash = 1000;
    p.money_bomb_counter = 2;

    /* Tick 1: 2 → 1 (still active) */
    player_money_bomb_tick(&p);
    TEST_ASSERT_EQUAL_INT32(1, p.money_bomb_counter);
    TEST_ASSERT_EQUAL_INT16(301, p.digging_power);  /* no deduction yet */

    /* Tick 2: 1 → 0 (repay 300 dig power) */
    player_money_bomb_tick(&p);
    TEST_ASSERT_EQUAL_INT32(0, p.money_bomb_counter);
    TEST_ASSERT_EQUAL_INT16(1, p.digging_power);
    TEST_ASSERT_EQUAL_INT32(1000, p.cash);          /* cash never touched */

    /* Tick 3: 0 → stays 0 (no-op) */
    player_money_bomb_tick(&p);
    TEST_ASSERT_EQUAL_INT32(0, p.money_bomb_counter);
    TEST_ASSERT_EQUAL_INT16(1, p.digging_power);
}

/* --- Test: remote detonation uses per-player bomb_sig tiles --- */
void test_remote_detonate(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    player_init_defaults(&p, 0);
    /* Player 1 bomb_sig: 'c' (0x63), 'd' (0x64) */

    /* Place matching sig[0] and sig[1] bombs and one non-matching */
    map.tiles[5][3] = BOMB_SIG_P1_A;  /* 'c' */
    map.overlay[5][3] = 0;
    map.tiles[20][10] = BOMB_SIG_P1_B;  /* 'd' */
    map.overlay[20][10] = 0;
    map.tiles[30][15] = BOMB_SMALL_1;  /* not remote */
    map.overlay[30][15] = 80;

    bombs_remote_detonate(&map, &p);

    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[5][3]);
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[20][10]);
    TEST_ASSERT_EQUAL_UINT16(80, map.overlay[30][15]);  /* unchanged */
}

/* --- Test: remote detonation only triggers own player's bombs --- */
void test_remote_detonate_per_player(void)
{
    TileMap map;
    Player p1, p3;
    setup_test_map(&map);
    player_init_defaults(&p1, 0);  /* sig: 'c', 'd' */
    player_init_defaults(&p3, 2);  /* sig: 0x82, 0x83 */

    /* Place P1 and P3 bomb sig tiles on map */
    map.tiles[5][3] = BOMB_SIG_P1_A;   /* Player 1 bomb */
    map.overlay[5][3] = 0;
    map.tiles[10][7] = BOMB_SIG_P3_A;  /* Player 3 bomb (0x82) */
    map.overlay[10][7] = 0;

    /* Player 1 remote detonates: only their bombs trigger */
    bombs_remote_detonate(&map, &p1);
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[5][3]);    /* P1 bomb triggered */
    TEST_ASSERT_EQUAL_UINT16(0, map.overlay[10][7]);   /* P3 bomb untouched */
}

/* --- Test: placing a bomb_sig weapon sets the player's signature tile --- */
void test_place_bomb_sig_tile(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    player_init_defaults(&p, 2);  /* Player 3: sig = 0x82, 0x83 */

    /* Give player stock of sig[0] weapon */
    p.weapons[WEAPON_SIG0_SLOT] = 3;
    p.selected_weapon = p.bomb_sig[0];  /* 0x82 */
    p.x_pos = (int16_t)(10 * TILE_SIZE);
    p.y_pos = (int16_t)(5 * TILE_SIZE + MAP_Y_OFFSET);
    p.direction = DIR_DOWN;

    bool placed = bomb_place(&p, &map);

    TEST_ASSERT_TRUE(placed);
    TEST_ASSERT_EQUAL_UINT8(0x82, map.tiles[10][5]);   /* Player 3's sig tile */
    TEST_ASSERT_EQUAL_UINT16(BOMB_COLLISION_DEFAULT, map.collision[10][5]);
    TEST_ASSERT_EQUAL_UINT16(0, map.overlay[10][5]);    /* overlay=0 (remote) */
    TEST_ASSERT_EQUAL_INT16(2, p.weapons[WEAPON_SIG0_SLOT]);  /* decremented */
}

/* --- Test: mega bomb fuse value --- */
void test_mega_bomb_overlay(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_MEGA_BOMB, 1);

    bomb_place(&p, &map);

    TEST_ASSERT_EQUAL_UINT8(WEAPON_MEGA_BOMB, map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT16(260, map.overlay[10][5]);
}

/* --- Test: mine fuse value --- */
void test_mine_overlay(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_MINE, 1);

    bomb_place(&p, &map);

    TEST_ASSERT_EQUAL_UINT8(WEAPON_MINE, map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT16(280, map.overlay[10][5]);
}

/* --- Test: directional arrow encoding --- */
void test_directional_arrow(void)
{
    TileMap map;
    Player p;

    /* Down direction */
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_DIR_ARROW, 4);
    p.direction = DIR_DOWN;
    bomb_place(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(ARROW_DOWN, map.tiles[10][5]);

    /* Up direction */
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_DIR_ARROW, 3);
    p.direction = DIR_UP;
    bomb_place(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(ARROW_UP, map.tiles[10][5]);

    /* Left direction */
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_DIR_ARROW, 2);
    p.direction = DIR_LEFT;
    bomb_place(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(ARROW_LEFT, map.tiles[10][5]);

    /* Right direction */
    setup_test_map(&map);
    setup_test_player(&p, 5, 10, WEAPON_DIR_ARROW, 1);
    p.direction = DIR_RIGHT;
    bomb_place(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(ARROW_RIGHT, map.tiles[10][5]);
}

/* --- Test: mine cycling stages 0x9D → 0x9E → 0x9F → 0x9D --- */
void test_mine_stage_cycling(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = BOMB_MINE_1;  /* 0x9D */
    map.overlay[10][5] = 10;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;

    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MINE_2, map.tiles[10][5]);  /* 0x9E */

    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MINE_3, map.tiles[10][5]);  /* 0x9F */

    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(BOMB_MINE_1, map.tiles[10][5]);  /* 0x9D (back) */
}

/* --- Test: mine detonation triggers screen shake (seg_1010:1245) --- */
void test_mine_detonation_screen_shake(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a mine (stage 1) */
    map.tiles[10][5] = BOMB_MINE_1;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;
    map.overlay[10][5] = 0;

    TEST_ASSERT_EQUAL_INT(0, map.screen_shake);

    /* Detonate mine — should add 10 to screen_shake */
    bomb_detonate(&map, 5, 10);
    TEST_ASSERT_EQUAL_INT(10, map.screen_shake);

    /* Detonate another mine — shake should accumulate */
    map.tiles[20][15] = BOMB_MINE_2;
    map.collision[20][15] = BOMB_COLLISION_DEFAULT;
    bomb_detonate(&map, 15, 20);
    TEST_ASSERT_EQUAL_INT(20, map.screen_shake);

    /* Non-mine bomb should NOT trigger shake */
    map.tiles[30][10] = BOMB_SMALL_1;
    map.collision[30][10] = BOMB_COLLISION_DEFAULT;
    bomb_detonate(&map, 10, 30);
    TEST_ASSERT_EQUAL_INT(20, map.screen_shake);  /* unchanged */
}

/* --- Test: adjacent bombs do NOT chain-detonate --- */
void test_chain_detonation(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place two bombs next to each other */
    map.tiles[10][5] = BOMB_SMALL_1;
    map.overlay[10][5] = 0;  /* about to detonate */
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;

    map.tiles[10][6] = BOMB_SMALL_1;
    map.overlay[10][6] = 50;  /* not yet ready */
    map.collision[10][6] = BOMB_COLLISION_DEFAULT;

    /* Detonate first bomb — adjacent bomb is destroyed by explosion fire,
     * NOT chain-detonated. FUN_1010_1326 (seg_1010:768) checks
     * (DS[0x1326] & Random()) != 0 but DS[0x1326] is never initialized
     * (always 0), so chain-detonation never triggers. */
    bomb_detonate(&map, 5, 10);

    /* Adjacent bomb should be converted to explosion fire (overlay=3) */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[10][6]);
    TEST_ASSERT_EQUAL_UINT16(3, map.overlay[10][6]);
}

/* --- Test: arrow bomb moves one tile DOWN per detonation tick --- */
void test_arrow_moves_down(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place arrow DOWN at (5, 10) with overlay=1 (about to detonate/move) */
    map.tiles[10][5] = ARROW_DOWN;
    map.overlay[10][5] = 1;
    map.bomb_owner[10][5] = 0;

    /* Detonation should move arrow DOWN (col+1) instead of exploding */
    bomb_detonate(&map, 5, 10);

    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);        /* origin cleared */
    TEST_ASSERT_EQUAL_UINT8(ARROW_DOWN, map.tiles[10][6]); /* moved down (col+1) */
    TEST_ASSERT_EQUAL_UINT16(2, map.overlay[10][6]);        /* overlay=2 for continued movement */
    TEST_ASSERT_EQUAL_UINT8(0, map.bomb_owner[10][6]);      /* owner preserved */
}

/* --- Test: arrow bomb moves UP --- */
void test_arrow_moves_up(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = ARROW_UP;
    map.overlay[10][5] = 1;

    bomb_detonate(&map, 5, 10);

    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT8(ARROW_UP, map.tiles[10][4]);  /* moved up (col-1) */
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[10][4]);  /* UP overlay = 1 */
}

/* --- Test: arrow bomb moves RIGHT --- */
void test_arrow_moves_right(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = ARROW_RIGHT;
    map.overlay[10][5] = 1;

    bomb_detonate(&map, 5, 10);

    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT8(ARROW_RIGHT, map.tiles[11][5]);  /* moved right (row+1) */
    TEST_ASSERT_EQUAL_UINT16(2, map.overlay[11][5]);
}

/* --- Test: arrow bomb moves LEFT --- */
void test_arrow_moves_left(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][5] = ARROW_LEFT;
    map.overlay[10][5] = 1;

    bomb_detonate(&map, 5, 10);

    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT8(ARROW_LEFT, map.tiles[9][5]);  /* moved left (row-1) */
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[9][5]);
}

/* --- Test: arrow bomb detonates as small bomb when blocked by wall --- */
void test_arrow_blocked_by_wall(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place arrow DOWN with wall at col+1 (DOWN direction in VGA convention) */
    map.tiles[10][5] = ARROW_DOWN;
    map.overlay[10][5] = 1;
    map.tiles[10][6] = '7';
    map.collision[10][6] = 1227;  /* standard wall HP */

    bomb_detonate(&map, 5, 10);

    /* Arrow should have detonated as small bomb at origin */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);  /* origin cleared */
    /* Wall should be degraded (weak mode: 'p'/'q' with HP 500/1000) */
    TEST_ASSERT_TRUE(map.tiles[10][6] == 'p' || map.tiles[10][6] == 'q');
}

/* --- Test: arrow bomb detonates at map edge --- */
void test_arrow_blocked_at_edge(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place arrow DOWN at the last col (DOWN = col+1, will be out of bounds) */
    map.tiles[5][MAP_COLS - 1] = ARROW_DOWN;
    map.overlay[5][MAP_COLS - 1] = 1;

    bomb_detonate(&map, MAP_COLS - 1, 5);

    /* Should detonate as small bomb (can't move off map) */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[5][MAP_COLS - 1]);
}

/* --- Test: arrow bomb traverses multiple tiles via bombs_update --- */
void test_arrow_multi_tick_movement(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place arrow RIGHT at (5, 10) */
    map.tiles[10][5] = ARROW_RIGHT;
    map.overlay[10][5] = 1;

    /* Tick 1: arrow at [10][5] overlay=1 → moves to [11][5] (row+1) overlay=2.
     * Then iteration reaches [11][5] and decrements overlay to 1. */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][5]);
    TEST_ASSERT_EQUAL_UINT8(ARROW_RIGHT, map.tiles[11][5]);

    /* Tick 2: [11][5] overlay=1 → moves to [12][5] (row+1) overlay=2.
     * Iteration reaches [12][5], decrements to 1. */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[11][5]);
    TEST_ASSERT_EQUAL_UINT8(ARROW_RIGHT, map.tiles[12][5]);

    /* Arrow advances one tile per bombs_update tick when moving RIGHT,
     * because the iteration (row 0→63) processes each new position same frame. */
}

/* --- Test: mine detonation creates cross pattern with palette flash --- */
void test_mine_cross_pattern(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Surround center with destructible walls */
    for (int dr = -3; dr <= 3; dr++) {
        for (int dc = -3; dc <= 3; dc++) {
            int r = 20 + dr, c = 20 + dc;
            if (r >= 0 && r < MAP_ROWS && c >= 0 && c < MAP_COLS) {
                map.tiles[r][c] = '7';
                map.collision[r][c] = 50;
            }
        }
    }

    /* Place mine at center */
    map.tiles[20][20] = BOMB_MINE_1;
    map.collision[20][20] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 20, 20);

    /* Mine should clear the center tile */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[20][20]);

    /* Palette flash should be triggered */
    TEST_ASSERT_TRUE(map.palette_flash > 0);

    /* Screen shake should be triggered */
    TEST_ASSERT_EQUAL_INT(10, map.screen_shake);

    /* Adjacent tiles should have been damaged (cross extends ±12 rows) */
    /* Check that at least some nearby tiles were affected */
    int damaged_count = 0;
    for (int dr = -3; dr <= 3; dr++) {
        for (int dc = -3; dc <= 3; dc++) {
            int r = 20 + dr, c = 20 + dc;
            if (r == 20 && c == 20) continue;
            if (map.collision[r][c] < 50 || map.tiles[r][c] == '0') {
                damaged_count++;
            }
        }
    }
    TEST_ASSERT_GREATER_THAN(0, damaged_count);
}

/* --- Test: mine cross does NOT chain-detonate adjacent bombs --- */
void test_mine_cross_chain_detonation(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place mine at (20, 20) */
    map.tiles[20][20] = BOMB_MINE_1;
    map.collision[20][20] = BOMB_COLLISION_DEFAULT;

    /* Place a bomb nearby with active fuse */
    map.tiles[20][22] = BOMB_SMALL_1;
    map.overlay[20][22] = 50;
    map.collision[20][22] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 20, 20);

    /* Nearby bomb destroyed by mine blast (converted to fire), NOT chain-detonated.
     * Mine cross has diamond radius 12 — tile at (20,22) is within blast. */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[20][22]);
    TEST_ASSERT_EQUAL_UINT16(3, map.overlay[20][22]);
}

/* --- Test: proximity mine moves to adjacent passable tile on detonation --- */
void test_proximity_mine_movement(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(100u);  /* fixed seed */

    /* Place proximity mine at (20, 20) */
    map.tiles[20][20] = 0x6F;
    map.collision[20][20] = BOMB_COLLISION_PROX;
    map.overlay[20][20] = 1;  /* about to "detonate" (move) */

    /* Surround with floor (passable) */
    bomb_detonate(&map, 20, 20);

    /* Original mine should still be there with a new fuse */
    TEST_ASSERT_EQUAL_UINT8(0x6F, map.tiles[20][20]);
    TEST_ASSERT_GREATER_THAN(0, map.overlay[20][20]);

    /* At least one adjacent tile should now also be a mine */
    int adjacent_mines = 0;
    if (map.tiles[19][20] == 0x6F) adjacent_mines++;
    if (map.tiles[21][20] == 0x6F) adjacent_mines++;
    if (map.tiles[20][19] == 0x6F) adjacent_mines++;
    if (map.tiles[20][21] == 0x6F) adjacent_mines++;
    TEST_ASSERT_GREATER_THAN(0, adjacent_mines);
}

/* --- Test: proximity mine does not spread to wall tiles --- */
void test_proximity_mine_blocked(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place mine surrounded by indestructible walls */
    map.tiles[20][20] = 0x6F;
    map.collision[20][20] = BOMB_COLLISION_PROX;
    map.overlay[20][20] = 1;

    map.tiles[19][20] = '1'; map.collision[19][20] = 30000;
    map.tiles[21][20] = '1'; map.collision[21][20] = 30000;
    map.tiles[20][19] = '1'; map.collision[20][19] = 30000;
    map.tiles[20][21] = '1'; map.collision[20][21] = 30000;

    bomb_detonate(&map, 20, 20);

    /* Mine should stay put (can't spread) but gets a new fuse */
    TEST_ASSERT_EQUAL_UINT8(0x6F, map.tiles[20][20]);
    TEST_ASSERT_GREATER_THAN(0, map.overlay[20][20]);

    /* No adjacent mines should appear */
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[19][20]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[21][20]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[20][19]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[20][21]);
}

/* --- Test: urethane (0xA1) flood-fill creates 0xA0 tiles --- */
void test_urethane_flood_fill(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place timer bomb at (10, 10) surrounded by open floor */
    map.tiles[10][10] = BOMB_TIMER_1;
    map.collision[10][10] = BOMB_COLLISION_DEFAULT;
    map.overlay[10][10] = 1;

    bomb_detonate(&map, 10, 10);

    /* Detonation center should be cleared to floor */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[10][10]);

    /* Adjacent tiles should be urethane (0xA0) */
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_FINAL, map.tiles[11][10]);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_FINAL, map.tiles[9][10]);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_FINAL, map.tiles[10][11]);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_FINAL, map.tiles[10][9]);

    /* Urethane tiles should have correct collision and overlay */
    TEST_ASSERT_EQUAL_UINT16(BOMB_COLLISION_PROX, map.collision[11][10]);
    TEST_ASSERT_EQUAL_UINT16(URETHANE_FINAL_OVERLAY, map.overlay[11][10]);
}

/* --- Test: urethane flood-fill is bounded by walls --- */
void test_urethane_bounded(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Create a small enclosed room: 3x3 open floor surrounded by walls */
    for (int r = 8; r <= 12; r++) {
        for (int c = 8; c <= 12; c++) {
            map.tiles[r][c] = '1';
            map.collision[r][c] = 30000;
        }
    }
    /* Open the interior */
    for (int r = 9; r <= 11; r++) {
        for (int c = 9; c <= 11; c++) {
            map.tiles[r][c] = '0';
            map.collision[r][c] = 0;
        }
    }

    /* Place timer bomb at center of room */
    map.tiles[10][10] = BOMB_TIMER_1;
    map.collision[10][10] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 10, 10);

    /* Center = floor, 8 surrounding floor tiles = urethane */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][10]);

    int urethane_count = 0;
    for (int r = 9; r <= 11; r++) {
        for (int c = 9; c <= 11; c++) {
            if (r == 10 && c == 10) continue;
            if (map.tiles[r][c] == TILE_URETHANE_FINAL) urethane_count++;
        }
    }
    TEST_ASSERT_EQUAL_INT(8, urethane_count);

    /* Walls should be untouched */
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[8][10]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[12][10]);
}

/* --- Test: urethane (0xA0) tiles expire to floor when overlay reaches 0 --- */
void test_urethane_tile_expiry(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a urethane tile with low overlay */
    map.tiles[10][10] = TILE_URETHANE_FINAL;
    map.collision[10][10] = BOMB_COLLISION_PROX;
    map.overlay[10][10] = 2;

    /* Two ticks: overlay 2→1, then 1→detonate (which expires to floor) */
    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_FINAL, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[10][10]);

    bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[10][10]);
}

/* --- Test: power bomb scatter creates explosion pattern --- */
void test_power_bomb_scatter(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a power bomb at center of map */
    int center_col = 22, center_row = 30;
    map.tiles[center_row][center_col] = 0xA4;
    map.overlay[center_row][center_col] = 0;
    map.collision[center_row][center_col] = BOMB_COLLISION_DEFAULT;
    map.bomb_owner[center_row][center_col] = 0;

    /* Seed for reproducible scatter pattern */
    mb_prng_set_seed(123u);

    /* Detonate the power bomb */
    bomb_detonate(&map, center_col, center_row);

    /* Center should be explosion fire */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[center_row][center_col]);
    TEST_ASSERT_EQUAL_UINT16(3, map.overlay[center_row][center_col]);

    /* Count how many tiles were affected (changed from floor '0') */
    int affected = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map.tiles[r][c] != '0') affected++;
        }
    }
    /* Should have many explosion tiles spread around (at least center + some scatter) */
    TEST_ASSERT_GREATER_THAN(10, affected);
    /* But not the entire map */
    TEST_ASSERT_LESS_THAN(MAP_ROWS * MAP_COLS / 2, affected);
}

/* --- Test: power bomb scatter degrades walls --- */
void test_power_bomb_scatter_walls(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place walls near center */
    int center_col = 22, center_row = 30;
    map.tiles[center_row][center_col] = 0xA4;
    map.overlay[center_row][center_col] = 0;
    map.collision[center_row][center_col] = BOMB_COLLISION_DEFAULT;
    map.bomb_owner[center_row][center_col] = 0;

    /* Place indestructible wall adjacent — should NOT be affected */
    map.tiles[center_row][center_col + 1] = '1';
    map.collision[center_row][center_col + 1] = 30000;

    /* Place some destructible walls nearby */
    map.tiles[center_row + 1][center_col] = '7';
    map.collision[center_row + 1][center_col] = 1227;

    mb_prng_set_seed(1u);  /* deterministic */

    bomb_detonate(&map, center_col, center_row);

    /* Indestructible wall should still be '1' */
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[center_row][center_col + 1]);
}

/* --- Test: power bomb NOT chain-detonated by adjacent small bomb --- */
void test_power_bomb_chain_detonate(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a small bomb next to a power bomb.
     * In the original, chain-detonation through FUN_1010_1326 never happens
     * because DS[0x1326] is always 0. The small bomb explosion converts the
     * adjacent power bomb to explosion fire without triggering its scatter. */
    map.tiles[10][10] = BOMB_SMALL_1;
    map.overlay[10][10] = 1;  /* about to detonate */
    map.collision[10][10] = BOMB_COLLISION_DEFAULT;
    map.bomb_owner[10][10] = 0;

    /* Power bomb at adjacent tile */
    map.tiles[10][11] = 0xA4;
    map.overlay[10][11] = 0;  /* inert */
    map.collision[10][11] = BOMB_COLLISION_DEFAULT;
    map.bomb_owner[10][11] = 0;

    mb_prng_set_seed(42u);

    /* Single update: small bomb detonates, power bomb destroyed by fire */
    bombs_update(&map);

    /* Power bomb should be replaced by explosion fire, NOT scatter-detonated.
     * Overlay starts at 3 from apply_explosion_hit, then decremented to 2
     * in the same bombs_update pass (col 11 processed after col 10). */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[10][11]);
    TEST_ASSERT_EQUAL_UINT16(2, map.overlay[10][11]);

    /* Only the small bomb's cross pattern should have created fire tiles.
     * Small cross = 4 tiles + center cleared. No power bomb scatter. */
    int affected = 0;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            if (map.tiles[r][c] == TILE_EXPLOSION) affected++;
    /* 4 cross tiles from small bomb (center becomes '0') */
    TEST_ASSERT_LESS_OR_EQUAL(4, affected);
}

/* --- Test: explosion damage uses fixed values, not player stats --- */
void test_explosion_fixed_damage(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a destructible wall adjacent to bomb */
    map.tiles[10][6] = 'B';
    map.collision[10][6] = 24;

    /* Place small bomb owned by player with high digging stats */
    map.tiles[10][5] = BOMB_SMALL_1;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;
    map.bomb_owner[10][5] = 0;
    g_players[0].digging_power = 100;
    g_players[0].bonus_stat = 50;

    bomb_detonate(&map, 5, 10);

    /* Wall should be degraded to 'p'/'q' with FIXED HP (500/1000),
     * NOT destroyed despite player having high stats.
     * Original uses fixed damage model (FUN_1010_1326 param_2=0). */
    TEST_ASSERT_TRUE(map.tiles[10][6] == 'p' || map.tiles[10][6] == 'q');
    TEST_ASSERT_TRUE(map.collision[10][6] == 500 || map.collision[10][6] == 1000);

    /* Reset player stats */
    g_players[0].digging_power = 1;
    g_players[0].bonus_stat = 0;
}

/* --- Test: strong explosion (mine) destroys walls completely --- */
void test_strong_explosion_destroys_walls(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place high-HP wall and mine next to it */
    map.tiles[20][21] = '7';
    map.collision[20][21] = 1227;

    map.tiles[20][20] = BOMB_MINE_1;
    map.collision[20][20] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 20, 20);

    /* Mine uses strong mode: wall should become fire tile (0x84)
     * regardless of HP (matching original FUN_1010_1326 param_2=1). */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[20][21]);
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[20][21]);
}

/* --- Test: gate closure random detonation --- */
void test_gate_closure_detonation(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Set up a gate position with layer4 bit 2 (gate marker) and a bomb tile */
    map.layer4[10][5] = 2;  /* gate marker from previous open */
    map.tiles[10][5] = BOMB_SMALL_1;
    map.collision[10][5] = BOMB_COLLISION_DEFAULT;
    map.overlay[10][5] = 50;

    /* Close gates many times to test random detonation fires sometimes */
    int detonated = 0;
    for (int i = 0; i < 100; i++) {
        setup_test_map(&map);
        map.layer4[10][5] = 2;
        map.tiles[10][5] = BOMB_SMALL_1;
        map.collision[10][5] = BOMB_COLLISION_DEFAULT;
        map.overlay[10][5] = 50;

        map_shop_toggle_close(&map);

        /* After close: tile should always be 'l' (gate) */
        TEST_ASSERT_EQUAL_UINT8('l', map.tiles[10][5]);

        /* If detonation happened, collision/overlay may have been changed */
        if (map.collision[10][5] == 0) detonated++;
    }

    /* With ~50% probability, detonation should have happened some of the time */
    TEST_ASSERT_GREATER_THAN(10, detonated);
    TEST_ASSERT_LESS_THAN(90, detonated);
}

/* --- Test: Rocket launcher fires a beam of fire tiles in facing direction --- */
void test_rocket_launcher_beam(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Fire rocket launcher DOWN from row=5, col=10.
     * DOWN = col+ in VGA convention. */
    rocket_launcher_fire(&map, 10, 5, DIR_DOWN);

    /* Should place fire 'a' (0x61) tiles at cols 11-16 (6 tiles) */
    for (int c = 11; c <= 16; c++) {
        TEST_ASSERT_EQUAL_UINT8(TILE_ROCKET_FIRE, map.tiles[5][c]);
        TEST_ASSERT_EQUAL_UINT16(3, map.overlay[5][c]);
    }
    /* Col 17 should be untouched floor */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[5][17]);
}

/* --- Test: Rocket launcher beam stops at walls --- */
void test_rocket_launcher_blocked_by_wall(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a wall at col=8 */
    map.tiles[10][8] = '1';

    /* Fire RIGHT from row=10, col=5 — RIGHT = row+ → should go 6 tiles to row=16 */
    rocket_launcher_fire(&map, 5, 10, DIR_RIGHT);
    for (int r = 11; r <= 16; r++) {
        TEST_ASSERT_EQUAL_UINT8(TILE_ROCKET_FIRE, map.tiles[r][5]);
    }

    /* Fire DOWN from row=5, col=10 — DOWN = col+ → should stop before wall at col=8.
     * But col=8 is not in the path (starts at col=11). Use a different scenario. */
    setup_test_map(&map);
    map.tiles[5][14] = '1';  /* wall at col=14 */
    rocket_launcher_fire(&map, 10, 5, DIR_DOWN);

    /* Tiles col 11,12,13 should be fire; col 14 should still be wall */
    TEST_ASSERT_EQUAL_UINT8(TILE_ROCKET_FIRE, map.tiles[5][11]);
    TEST_ASSERT_EQUAL_UINT8(TILE_ROCKET_FIRE, map.tiles[5][12]);
    TEST_ASSERT_EQUAL_UINT8(TILE_ROCKET_FIRE, map.tiles[5][13]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[5][14]);
}

/* --- Test: Directional rocket expands and creates fire tiles --- */
void test_directional_rocket_expanding_cross(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(123u);

    /* Fire directional rocket DOWN from center of map */
    directional_rocket_fire(&map, 22, 32, DIR_DOWN);

    /* After expansion, there should be fire (0x84) tiles on the map.
     * The exact pattern depends on random, but there should be at least
     * some fire tiles created. */
    int fire_count = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map.tiles[r][c] == TILE_EXPLOSION)
                fire_count++;
        }
    }

    /* Should have created fire tiles (the expansion places up to 30) */
    TEST_ASSERT_TRUE(fire_count >= 1);

    /* No 'z' (0x7A) tiles should remain — all converted to fire */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            TEST_ASSERT_NOT_EQUAL(TILE_ROCKET_EXPAND, map.tiles[r][c]);
        }
    }
}

/* --- Test: Directional rocket bias toward facing direction --- */
void test_directional_rocket_direction_bias(void)
{
    /* Fire two rockets in different directions with the same seed and verify
     * that fire tiles are biased toward the facing direction. */
    TileMap map_down, map_right;
    setup_test_map(&map_down);
    setup_test_map(&map_right);

    mb_prng_set_seed(42u);
    directional_rocket_fire(&map_down, 22, 32, DIR_DOWN);

    mb_prng_set_seed(42u);
    directional_rocket_fire(&map_right, 22, 32, DIR_RIGHT);

    /* Count fire tiles below vs above center for DOWN direction.
     * DOWN = col+ in VGA convention. Center is at col=22. */
    int down_below = 0, down_above = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map_down.tiles[r][c] == TILE_EXPLOSION) {
                if (c > 22) down_below++;
                else if (c < 22) down_above++;
            }
        }
    }

    /* When firing DOWN, more tiles should be at col>22 than col<22 */
    TEST_ASSERT_TRUE_MESSAGE(down_below >= down_above,
        "DOWN rocket should have more fire tiles below than above");
}

/* --- Test: Creature spawner consumes inventory via bomb_place --- */
void test_creature_spawner_consumes_inventory(void)
{
    TileMap map;
    Player p;
    setup_test_map(&map);
    setup_test_player(&p, 10, 20, WEAPON_CREATURE_N, 3);

    bool result = bomb_place(&p, &map);

    TEST_ASSERT_TRUE(result);
    /* Inventory should be decremented */
    int idx = player_weapon_index(WEAPON_CREATURE_N);
    TEST_ASSERT_EQUAL_INT16(2, p.weapons[idx]);
    /* bombs_placed should be incremented */
    TEST_ASSERT_EQUAL_INT16(1, p.bombs_placed);
    /* No tile should be placed on the map (entity spawning handled separately) */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[20][10]);
}

/* --- Test: Teleporter bomb (0xA9) creates indestructible wall --- */
void test_teleporter_bomb_creates_wall(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a teleporter bomb at (row=10, col=5) */
    map.tiles[10][5] = 0xA9;
    map.overlay[10][5] = 1;
    map.collision[10][5] = 20;

    bomb_detonate(&map, 5, 10);

    /* Should become indestructible wall '1' with collision=30000 */
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[10][5]);
    TEST_ASSERT_EQUAL_INT16(30000, map.collision[10][5]);
    TEST_ASSERT_EQUAL_INT16(0, map.overlay[10][5]);
}

/* --- Test: Random bomb (0xAB) detonates as random small/medium/large --- */
void test_random_bomb_explodes_and_bounces(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Place a random bomb at (row=20, col=20) with collision=5 (bounces left) */
    map.tiles[20][20] = 0xAB;
    map.collision[20][20] = 5;
    map.overlay[20][20] = 1;

    /* Surround with floor so bounce can find a target */
    bomb_detonate(&map, 20, 20);

    /* The original position should have been cleared (or have explosion remnant).
     * There should be a new 0xAB tile somewhere nearby with collision=4. */
    bool found_bounce = false;
    for (int r = 16; r < 25; r++) {
        for (int c = 16; c < 25; c++) {
            if (map.tiles[r][c] == 0xAB) {
                /* Bounce target found */
                TEST_ASSERT_EQUAL_INT16(4, map.collision[r][c]);
                TEST_ASSERT_TRUE(map.overlay[r][c] > 0);
                found_bounce = true;
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found_bounce, "Random bomb should bounce to nearby tile");
}

/* --- Test: Random bomb (0xAB) does not bounce when collision <= 1 --- */
void test_random_bomb_no_bounce_low_collision(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Place a random bomb with collision=1 (last bounce) */
    map.tiles[20][20] = 0xAB;
    map.collision[20][20] = 1;
    map.overlay[20][20] = 1;

    bomb_detonate(&map, 20, 20);

    /* Should NOT have any 0xAB tile anywhere (no bounce) */
    bool found_bounce = false;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            if (map.tiles[r][c] == 0xAB) found_bounce = true;
    TEST_ASSERT_FALSE_MESSAGE(found_bounce, "Random bomb with collision<=1 should not bounce");
}

/* --- Test: Mega bomb (0x7F) uses random expanding explosion, not mine cross --- */
void test_mega_bomb_expanding_explosion(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Place mega bomb at center of open area */
    map.tiles[30][20] = BOMB_MEGA_1;
    map.collision[30][20] = BOMB_COLLISION_DEFAULT;
    map.overlay[30][20] = 1;

    bomb_detonate(&map, 20, 30);

    /* Center and expanded tiles should become fire (0x84) with overlay=3.
     * FUN_1010_1326 with strong=1 places fire at every expanded position,
     * including center (after clearing 'z' to '0', apply_explosion_hit
     * converts '0' to 0x84). */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[30][20]);

    /* There should be NO remaining 'z' (0x7A) or 'Z' (0x5A) marker tiles */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            TEST_ASSERT_NOT_EQUAL(TILE_ROCKET_EXPAND, map.tiles[r][c]);
            TEST_ASSERT_NOT_EQUAL(0x5A, map.tiles[r][c]);
        }
    }

    /* Count fire tiles — should be multiple (expansion creates several) */
    int fire_count = 0;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            if (map.tiles[r][c] == TILE_EXPLOSION) fire_count++;
    TEST_ASSERT_GREATER_THAN(1, fire_count);

    /* Should NOT have palette flash (that's mine only) */
    TEST_ASSERT_EQUAL_INT(0, map.palette_flash);

    /* Should NOT have screen shake (that's mine only) */
    TEST_ASSERT_EQUAL_INT(0, map.screen_shake);
}

/* --- Test: Mega bomb expansion is bounded by walls --- */
void test_mega_bomb_bounded_by_walls(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(100u);

    /* Create a 5x5 enclosed room */
    for (int r = 28; r <= 32; r++) {
        for (int c = 18; c <= 22; c++) {
            map.tiles[r][c] = '1';
            map.collision[r][c] = 30000;
        }
    }
    for (int r = 29; r <= 31; r++) {
        for (int c = 19; c <= 21; c++) {
            map.tiles[r][c] = '0';
            map.collision[r][c] = 0;
        }
    }

    /* Place mega bomb at center */
    map.tiles[30][20] = BOMB_MEGA_1;
    map.collision[30][20] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 20, 30);

    /* Walls should still be intact (mega bomb expansion doesn't eat through walls
     * via rocket_tile_passable which excludes '1') */
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[28][20]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[32][20]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[30][18]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[30][22]);
}

/* --- Test: Super bomb (0x81) creates permanent 0x9B tiles, not 0xA0 --- */
void test_super_bomb_permanent_urethane(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place super bomb at (10, 10) */
    map.tiles[10][10] = 0x81;
    map.collision[10][10] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 10, 10);

    /* Center should be floor */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][10]);

    /* Adjacent tiles should be 0x9B (permanent urethane), NOT 0xA0 */
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_PERM, map.tiles[11][10]);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_PERM, map.tiles[9][10]);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_PERM, map.tiles[10][11]);
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_PERM, map.tiles[10][9]);

    /* 0x9B tiles should have collision=400 but overlay=0 (permanent) */
    TEST_ASSERT_EQUAL_UINT16(BOMB_COLLISION_PROX, map.collision[11][10]);
    TEST_ASSERT_EQUAL_UINT16(0, map.overlay[11][10]);
}

/* --- Test: Timer bomb still creates temporary 0xA0 tiles with overlay=250 --- */
void test_timer_bomb_temporary_urethane(void)
{
    TileMap map;
    setup_test_map(&map);

    map.tiles[10][10] = BOMB_TIMER_1;
    map.collision[10][10] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 10, 10);

    /* Adjacent should be 0xA0 (temporary) with overlay=250 */
    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_FINAL, map.tiles[11][10]);
    TEST_ASSERT_EQUAL_UINT16(URETHANE_FINAL_OVERLAY, map.overlay[11][10]);
}

/* --- Test: 0x9B (super bomb urethane) does not decay via overlay --- */
void test_super_bomb_urethane_no_decay(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a 0x9B tile with overlay=0 */
    map.tiles[10][10] = TILE_URETHANE_PERM;
    map.collision[10][10] = BOMB_COLLISION_PROX;
    map.overlay[10][10] = 0;

    /* Tick 100 times — should remain unchanged (overlay=0 means no countdown) */
    for (int i = 0; i < 100; i++) {
        bombs_update(&map);
    }

    TEST_ASSERT_EQUAL_UINT8(TILE_URETHANE_PERM, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(BOMB_COLLISION_PROX, map.collision[10][10]);
}

/* --- Test: Remote bomb (0xA2) flood-fill expands through walls --- */
void test_remote_bomb_flood_fill(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Create a column of walls adjacent to the bomb position */
    for (int r = 18; r <= 22; r++) {
        map.tiles[r][20] = '7';
        map.collision[r][20] = 1227;
    }

    /* Place remote bomb at (row=20, col=19) — adjacent to wall column */
    map.tiles[20][19] = 0xA2;
    map.collision[20][19] = BOMB_COLLISION_DEFAULT;
    map.overlay[20][19] = 1;

    bomb_detonate(&map, 19, 20);

    /* Center should be cleared to floor */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[20][19]);

    /* Some wall tiles should have been affected:
     * - Some converted to fire (strong damage)
     * - Some expanded through (also become fire in final pass) */
    int walls_affected = 0;
    for (int r = 18; r <= 22; r++) {
        if (map.tiles[r][20] != '7') walls_affected++;
    }
    TEST_ASSERT_GREATER_THAN(0, walls_affected);
}

/* --- Test: Remote bomb does NOT expand through indestructible walls --- */
void test_remote_bomb_blocked_by_indestructible(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Create a small room: indestructible walls around a 3x3 area */
    for (int r = 18; r <= 22; r++) {
        for (int c = 18; c <= 22; c++) {
            map.tiles[r][c] = '1';
            map.collision[r][c] = 30000;
        }
    }
    /* Open the interior 3x3 */
    for (int r = 19; r <= 21; r++) {
        for (int c = 19; c <= 21; c++) {
            map.tiles[r][c] = '0';
            map.collision[r][c] = 0;
        }
    }

    /* Place remote bomb at center of room */
    map.tiles[20][20] = 0xA2;
    map.collision[20][20] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 20, 20);

    /* Indestructible walls should still be '1' (is_wall_tile returns false for '1') */
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[18][20]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[22][20]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[20][18]);
    TEST_ASSERT_EQUAL_UINT8('1', map.tiles[20][22]);
}

/* --- Test: Remote bomb final pass uses low player damage (10) --- */
void test_remote_bomb_player_damage(void)
{
    TileMap map;
    setup_test_map(&map);
    mb_prng_set_seed(42u);

    /* Set up a player at the center tile with some health */
    g_num_active_players = 1;
    player_init_defaults(&g_players[0], 0);
    g_players[0].health = 100;
    g_players[0].active = 1;
    /* Place player at tile (row=20, col=20) — same tile as bomb center */
    g_players[0].x_pos = (int16_t)(20 * TILE_SIZE);
    g_players[0].y_pos = (int16_t)(20 * TILE_SIZE + MAP_Y_OFFSET);

    /* Place remote bomb at (row=20, col=20) */
    map.tiles[20][20] = 0xA2;
    map.collision[20][20] = BOMB_COLLISION_DEFAULT;

    bomb_detonate(&map, 20, 20);

    /* Player should have taken damage (10 from the non-wall chain-detonation
     * apply_explosion_hit at adjacent tiles, which calls bombs_check_player_damage).
     * The final pass bombs_check_player_damage(10) at expanded positions may
     * also hit. Either way, player at center gets damaged during expansion. */
    TEST_ASSERT_TRUE(g_players[0].health < 100);

    /* Reset player for other tests */
    player_init_defaults(&g_players[0], 0);
}

/* --- Test: fire tile visual decay chain (NOT explosion) --- */
void test_fire_tile_decay_chain(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place fire tile (0x84) at (10, 10) with overlay=3 */
    map.tiles[10][10] = TILE_EXPLOSION;
    map.overlay[10][10] = 3;

    /* Tick down: 3→2→1→detonation handler */
    bombs_update(&map);  /* 3→2 */
    TEST_ASSERT_EQUAL_UINT8(TILE_EXPLOSION, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(2, map.overlay[10][10]);

    bombs_update(&map);  /* 2→1 */
    bombs_update(&map);  /* 1→0, triggers bomb_detonate(0x84) → decay to 0x61 */

    /* Should decay to 0x61 ('a') with overlay=3, NOT explode */
    TEST_ASSERT_EQUAL_UINT8(0x61, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(3, map.overlay[10][10]);
    /* Verify no fire was placed on adjacent tiles (would indicate unwanted explosion) */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][9]);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][11]);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[9][10]);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[11][10]);

    /* Continue decay: 0x61 → 0x62 → '0' */
    bombs_update(&map); bombs_update(&map); bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(0x62, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(3, map.overlay[10][10]);

    bombs_update(&map); bombs_update(&map); bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(0, map.overlay[10][10]);
}

/* --- Test: explosion2 tile (0x85) decay chain --- */
void test_explosion2_tile_decay_chain(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place explosion2 tile (0x85) with overlay=3 */
    map.tiles[10][10] = TILE_EXPLOSION2;
    map.overlay[10][10] = 3;

    /* Tick down to 1 → decay to 0x86 */
    bombs_update(&map); bombs_update(&map); bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(0x86, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(3, map.overlay[10][10]);

    /* 0x86 → 0x87 */
    bombs_update(&map); bombs_update(&map); bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8(0x87, map.tiles[10][10]);

    /* 0x87 → 'f' (corpse) */
    bombs_update(&map); bombs_update(&map); bombs_update(&map);
    TEST_ASSERT_EQUAL_UINT8('f', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(0, map.overlay[10][10]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_place_bomb);
    RUN_TEST(test_place_bomb_no_ammo);
    RUN_TEST(test_place_bomb_occupied_tile);
    RUN_TEST(test_fuse_countdown);
    RUN_TEST(test_visual_stage_small);
    RUN_TEST(test_visual_stage_medium);
    RUN_TEST(test_visual_stage_large);
    RUN_TEST(test_detonation_destroys_wall);
    RUN_TEST(test_detonation_indestructible);
    RUN_TEST(test_treasure_drop_rate);
    RUN_TEST(test_money_bomb);
    RUN_TEST(test_money_bomb_cooldown);
    RUN_TEST(test_money_bomb_loan_deduction);
    RUN_TEST(test_remote_detonate);
    RUN_TEST(test_remote_detonate_per_player);
    RUN_TEST(test_place_bomb_sig_tile);
    RUN_TEST(test_mega_bomb_overlay);
    RUN_TEST(test_mine_overlay);
    RUN_TEST(test_directional_arrow);
    RUN_TEST(test_mine_stage_cycling);
    RUN_TEST(test_mine_detonation_screen_shake);
    RUN_TEST(test_chain_detonation);
    RUN_TEST(test_arrow_moves_down);
    RUN_TEST(test_arrow_moves_up);
    RUN_TEST(test_arrow_moves_right);
    RUN_TEST(test_arrow_moves_left);
    RUN_TEST(test_arrow_blocked_by_wall);
    RUN_TEST(test_arrow_blocked_at_edge);
    RUN_TEST(test_arrow_multi_tick_movement);
    RUN_TEST(test_mine_cross_pattern);
    RUN_TEST(test_mine_cross_chain_detonation);
    RUN_TEST(test_proximity_mine_movement);
    RUN_TEST(test_proximity_mine_blocked);
    RUN_TEST(test_urethane_flood_fill);
    RUN_TEST(test_urethane_bounded);
    RUN_TEST(test_urethane_tile_expiry);
    RUN_TEST(test_power_bomb_scatter);
    RUN_TEST(test_power_bomb_scatter_walls);
    RUN_TEST(test_power_bomb_chain_detonate);
    RUN_TEST(test_explosion_fixed_damage);
    RUN_TEST(test_strong_explosion_destroys_walls);
    RUN_TEST(test_gate_closure_detonation);
    RUN_TEST(test_rocket_launcher_beam);
    RUN_TEST(test_rocket_launcher_blocked_by_wall);
    RUN_TEST(test_directional_rocket_expanding_cross);
    RUN_TEST(test_directional_rocket_direction_bias);
    RUN_TEST(test_creature_spawner_consumes_inventory);
    RUN_TEST(test_teleporter_bomb_creates_wall);
    RUN_TEST(test_random_bomb_explodes_and_bounces);
    RUN_TEST(test_random_bomb_no_bounce_low_collision);
    RUN_TEST(test_mega_bomb_expanding_explosion);
    RUN_TEST(test_mega_bomb_bounded_by_walls);
    RUN_TEST(test_super_bomb_permanent_urethane);
    RUN_TEST(test_timer_bomb_temporary_urethane);
    RUN_TEST(test_super_bomb_urethane_no_decay);
    RUN_TEST(test_remote_bomb_flood_fill);
    RUN_TEST(test_remote_bomb_blocked_by_indestructible);
    RUN_TEST(test_remote_bomb_player_damage);
    RUN_TEST(test_fire_tile_decay_chain);
    RUN_TEST(test_explosion2_tile_decay_chain);
    return UNITY_END();
}
