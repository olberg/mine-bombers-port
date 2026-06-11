#include "unity.h"
#include "game/movement.h"
#include "game/player.h"
#include "game/map.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include <string.h>

static TileMap map;
static Player  p;

void setUp(void)
{
    memset(&map, 0, sizeof(map));
    /* Fill map with floor */
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map.tiles[r][c] = '0';

    /* Add border walls */
    for (int r = 0; r < MAP_ROWS; r++) {
        map.tiles[r][0] = '1';
        map.tiles[r][MAP_COLS - 1] = '1';
    }
    for (int c = 0; c < MAP_COLS; c++) {
        map.tiles[0][c] = '1';
        map.tiles[MAP_ROWS - 1][c] = '1';
    }

    /* Initialize collision HP for wall tiles (indestructible = 0xFFFF) */
    map_init_collision(&map);

    player_init_defaults(&p, 0);
    /* Place player at tile (5, 5) → pixel (80, 50) */
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
}

void tearDown(void) {}

void test_passable_tiles(void)
{
    TEST_ASSERT_TRUE(tile_is_passable('0'));
    TEST_ASSERT_TRUE(tile_is_passable('f'));
    TEST_ASSERT_TRUE(tile_is_passable(0xAF));
    TEST_ASSERT_FALSE(tile_is_passable('1'));
    TEST_ASSERT_FALSE(tile_is_passable('W'));
    TEST_ASSERT_FALSE(tile_is_passable('7'));
}

void test_wall_blocks(void)
{
    TEST_ASSERT_FALSE(tile_is_passable('1'));
    TEST_ASSERT_FALSE(tile_is_passable('2'));
    TEST_ASSERT_FALSE(tile_is_passable('3'));
    TEST_ASSERT_FALSE(tile_is_passable('4'));
    TEST_ASSERT_FALSE(tile_is_passable('5'));
}

void test_move_on_open_floor(void)
{
    int16_t old_y = p.y_pos;
    p.direction = DIR_DOWN;
    bool moved = player_move(&p, &map);
    TEST_ASSERT_TRUE(moved);
    TEST_ASSERT_EQUAL(old_y + 1, p.y_pos);
}

void test_move_blocked_by_wall(void)
{
    /* Place player next to top wall (row 1, col 5) */
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(1);

    int16_t old_y = p.y_pos;
    p.direction = DIR_UP;
    bool moved = player_move(&p, &map);
    TEST_ASSERT_FALSE(moved);
    TEST_ASSERT_EQUAL(old_y, p.y_pos);
}

void test_treasure_pickup(void)
{
    /* Place treasure at player's center tile */
    int col = 5, row = 5;
    map.tiles[row][col] = 0x92;  /* 15 cash */

    p.x_pos = tile_to_pixel_x(col);
    p.y_pos = tile_to_pixel_y(row);
    int32_t old_cash = p.cash;
    int32_t old_earned = p.earned;

    bool picked = player_check_pickup(&p, &map, row, col);
    TEST_ASSERT_TRUE(picked);
    /* Treasure credits this round's EARNINGS (original +0xE6), not the
     * wallet — the wallet only changes at round end / in the shop. */
    TEST_ASSERT_EQUAL(old_earned + 15, p.earned);
    TEST_ASSERT_EQUAL(old_cash, p.cash);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[row][col]);
}

void test_health_pickup(void)
{
    int col = 5, row = 5;
    map.tiles[row][col] = 'm';  /* health restore */

    p.x_pos = tile_to_pixel_x(col);
    p.y_pos = tile_to_pixel_y(row);
    p.health = 500;

    bool picked = player_check_pickup(&p, &map, row, col);
    TEST_ASSERT_TRUE(picked);
    TEST_ASSERT_EQUAL(p.max_health, p.health);
}

/* --- Mystery box (0x79): random weapon drop --- */
void test_mystery_box_pickup(void)
{
    int col = 5, row = 5;
    map.tiles[row][col] = 0x79;
    map.collision[row][col] = 0;

    p.x_pos = tile_to_pixel_x(col);
    p.y_pos = tile_to_pixel_y(row);

    /* Count total weapons before */
    int total_before = 0;
    for (int i = 0; i < WEAPON_SLOTS; i++)
        total_before += p.weapons[i];

    bool picked = player_check_pickup(&p, &map, row, col);
    TEST_ASSERT_TRUE(picked);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[row][col]);
    TEST_ASSERT_EQUAL(0, map.collision[row][col]);

    /* Player should have received at least 1 weapon (unless RNG hit an unmapped slot) */
    int total_after = 0;
    for (int i = 0; i < WEAPON_SLOTS; i++)
        total_after += p.weapons[i];
    /* total_after >= total_before (may be equal if RNG hit unmapped slot -1) */
    TEST_ASSERT_TRUE(total_after >= total_before);
}

void test_mystery_box_repeated_gives_weapons(void)
{
    /* Pick up many mystery boxes to statistically ensure weapons are awarded.
     * With 3 unmapped slots out of 26 total entries, and multiple pickups,
     * the chance of getting zero weapons across 20 pickups is vanishingly small. */
    int col = 5, row = 5;
    p.x_pos = tile_to_pixel_x(col);
    p.y_pos = tile_to_pixel_y(row);

    int total_weapons = 0;
    for (int i = 0; i < 20; i++) {
        map.tiles[row][col] = 0x79;
        map.collision[row][col] = 0;
        player_check_pickup(&p, &map, row, col);
    }

    for (int i = 0; i < WEAPON_SLOTS; i++)
        total_weapons += p.weapons[i];

    /* Started with 5 small bombs + at least some mystery box drops */
    TEST_ASSERT_TRUE(total_weapons > 5);
}

/* --- Teleporter tile (0x9C): random warp --- */
void test_teleporter_warps_player(void)
{
    /* Place 3 teleporter tiles on the map */
    map.tiles[5][5] = 0x9C;   /* player is here */
    map.tiles[10][10] = 0x9C;
    map.tiles[20][20] = 0x9C;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    int16_t old_x = p.x_pos;
    int16_t old_y = p.y_pos;

    bool picked = player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_TRUE(picked);

    /* Player should have moved to a different position */
    bool moved = (p.x_pos != old_x || p.y_pos != old_y);
    TEST_ASSERT_TRUE(moved);

    /* Player should be at one of the other teleporter tile positions */
    bool at_valid = (p.x_pos == tile_to_pixel_x(10) && p.y_pos == tile_to_pixel_y(10)) ||
                    (p.x_pos == tile_to_pixel_x(20) && p.y_pos == tile_to_pixel_y(20));
    TEST_ASSERT_TRUE(at_valid);

    /* Teleporter tiles should NOT be cleared */
    TEST_ASSERT_EQUAL_HEX8(0x9C, map.tiles[5][5]);
    TEST_ASSERT_EQUAL_HEX8(0x9C, map.tiles[10][10]);
    TEST_ASSERT_EQUAL_HEX8(0x9C, map.tiles[20][20]);
}

void test_teleporter_single_does_nothing(void)
{
    /* Only one teleporter — no valid target, should return false */
    map.tiles[5][5] = 0x9C;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    int16_t old_x = p.x_pos;
    int16_t old_y = p.y_pos;

    bool picked = player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_FALSE(picked);
    TEST_ASSERT_EQUAL(old_x, p.x_pos);
    TEST_ASSERT_EQUAL(old_y, p.y_pos);
}

/* --- Wall sliding: perpendicular snap auto-aligns player to tile center --- */
void test_wall_slide_perpendicular_snap(void)
{
    /* Place player at tile (5, 5), offset X by 3 pixels right of center.
     * tile_to_pixel_x(5)=80, center_x would be 85 (intra_x=5).
     * Shift right by 3 so center_x=88 (intra_x=8). */
    p.x_pos = tile_to_pixel_x(5) + 3;
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_DOWN;

    int16_t old_x = p.x_pos;
    int16_t old_y = p.y_pos;
    bool moved = player_move(&p, &map);

    /* Primary movement blocked (not X-aligned: intra_x=8, need 4-5).
     * But perpendicular snap DOES occur: X snaps toward center. */
    TEST_ASSERT_FALSE(moved);
    TEST_ASSERT_EQUAL(old_x - 3, p.x_pos);  /* X snapped to center */
    TEST_ASSERT_EQUAL(old_y, p.y_pos);       /* Y unchanged (blocked) */

    /* After snap, player is now X-aligned. Second frame should succeed. */
    moved = player_move(&p, &map);
    TEST_ASSERT_TRUE(moved);
    TEST_ASSERT_EQUAL(old_y + 1, p.y_pos);
}

/* --- Wall sliding: approach corner and auto-align --- */
void test_wall_slide_around_corner(void)
{
    /* Wall at tiles[5][3] blocks the path above (col-1) from player at row=5, col=4.
     * DIR_UP checks tiles[row][col-1] = tiles[5][3]. */
    map.tiles[5][3] = '1';
    map_init_collision(&map);

    /* Place player at tile row=5, col=4 with centered position */
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(4);
    p.direction = DIR_UP;

    /* Should be blocked (wall above at tiles[5][3]) */
    bool moved = player_move(&p, &map);
    TEST_ASSERT_FALSE(moved);
}

/* --- Wall sliding: player can move to tile center even when next tile blocked --- */
void test_wall_slide_to_center(void)
{
    /* Wall at tiles[5][3] (col-1 from player col=4). DIR_UP checks tiles[row][col-1]. */
    map.tiles[5][3] = '1';
    map_init_collision(&map);

    /* Place player at the bottom of tile (5, 4): past center (intra_y > 5).
     * tile_to_pixel_y(4) = 40, center_y = 45 (intra_y = 5).
     * Add 2 pixels so center_y = 47 (intra_y = 7 > 5). */
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(4) + 2;
    p.direction = DIR_UP;

    /* Can still move up (intra_y > 5, moving toward center within tile) */
    bool moved = player_move(&p, &map);
    TEST_ASSERT_TRUE(moved);

    /* Keep moving until reaching center (intra_y = 5) */
    moved = player_move(&p, &map);
    TEST_ASSERT_TRUE(moved);

    /* At center, should be blocked by wall above */
    int cy = p.y_pos + SPRITE_H / 2;
    int intra_y = cy - (cy / TILE_SIZE) * TILE_SIZE;
    TEST_ASSERT_EQUAL(5, intra_y);

    moved = player_move(&p, &map);
    TEST_ASSERT_FALSE(moved);
}

/* --- Digging: player pushing against a wall subtracts HP --- */
void test_dig_reduces_wall_hp(void)
{
    /* Place a standard wall ('7') at tiles[5][4] (col-1 from player).
     * DIR_UP digs at tiles[row][col-1]. */
    map.tiles[5][4] = '7';
    map.collision[5][4] = 0x4CB;  /* 1227 */

    /* Player at row=5, col=5, centered (intra_y == 5) */
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 10;
    p.bonus_stat = 5;

    uint16_t old_hp = map.collision[5][4];
    player_dig(&p, &map);

    /* Wall HP should decrease by digging_power + bonus_stat = 15 */
    TEST_ASSERT_EQUAL_UINT16(old_hp - 15, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('7', map.tiles[5][4]);  /* tile not yet degraded */
}

void test_dig_destroys_wall(void)
{
    /* Place a weak wall with HP = 10 at tiles[5][4] (col-1 from player) */
    map.tiles[5][4] = '7';
    map.collision[5][4] = 10;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 20;
    p.bonus_stat = 0;

    player_dig(&p, &map);

    /* Wall should be destroyed (HP >= damage) */
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[5][4]);
}

void test_dig_wall_degradation_stages(void)
{
    /* Place a wall with HP that will cross the 1000 and 500 thresholds.
     * Wall at tiles[5][4] (col-1 from player). */
    map.tiles[5][4] = '7';
    map.collision[5][4] = 1005;  /* Just above 1000 threshold */

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 10;
    p.bonus_stat = 0;

    player_dig(&p, &map);

    /* HP = 995 (< 1000) → group1 wall '7' degrades to '6' */
    TEST_ASSERT_EQUAL_UINT16(995, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('6', map.tiles[5][4]);

    /* Now reduce to below 500 */
    map.collision[5][4] = 505;
    map.tiles[5][4] = '7';  /* reset tile for group1 check */

    player_dig(&p, &map);

    /* HP = 495 (< 500) → group1 wall degrades to '5' */
    TEST_ASSERT_EQUAL_UINT16(495, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('5', map.tiles[5][4]);
}

void test_dig_hp1_wall_destroys(void)
{
    /* Wall with HP=1 must be destroyed on any dig hit.
     * Original (seg_1000:3712-3719) treats hp < 2 as DESTROY.
     * Port guard: hp == 0 → skip (already floor); hp == 1 → falls through
     * to damage >= hp check, which destroys since damage >= 1 always. */
    map.tiles[5][4] = 'B';
    map.collision[5][4] = 1;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 1;
    p.bonus_stat = 0;

    player_dig(&p, &map);

    TEST_ASSERT_EQUAL_UINT16(0, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[5][4]);
}

void test_dig_hp0_tile_skipped(void)
{
    /* Tile with collision=0 is passable — dig should do nothing.
     * Original: hp < 2 → destroys (sets tile='0'), which is harmless
     * but redundant for already-passable tiles. Port: hp == 0 → return. */
    map.tiles[5][4] = 'f';   /* corpse tile, passable (collision=0) */
    map.collision[5][4] = 0;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 10;
    p.bonus_stat = 0;

    player_dig(&p, &map);

    /* Tile should be unchanged — nothing to dig */
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('f', map.tiles[5][4]);
}

void test_dig_indestructible_wall(void)
{
    /* Indestructible wall ('1', HP=30000) should not be damaged */
    map.tiles[4][5] = '1';
    map.collision[4][5] = 30000;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 100;
    p.bonus_stat = 100;

    player_dig(&p, &map);

    TEST_ASSERT_EQUAL_UINT16(30000, map.collision[4][5]);
    TEST_ASSERT_EQUAL_HEX8('1', map.tiles[4][5]);
}

void test_dig_requires_alignment(void)
{
    /* Wall at row 5, col 4 (left of player) */
    map.tiles[5][4] = '7';
    map.collision[5][4] = 0x4CB;

    /* Player NOT aligned on X axis (offset Y by 2 pixels).
     * For LEFT direction, digging checks intra_x == 5.
     * Offset Y so intra_y != 5, but that doesn't matter for LEFT.
     * Instead, offset X so intra_x != 5. */
    p.x_pos = tile_to_pixel_x(5) + 2;  /* intra_x = (x+5-30)/10 - col*10 = 7, not 5 */
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_LEFT;
    p.digging_power = 10;
    p.bonus_stat = 5;

    uint16_t old_hp = map.collision[5][4];
    player_dig(&p, &map);

    /* Wall should NOT be damaged (player not X-aligned for LEFT direction) */
    TEST_ASSERT_EQUAL_UINT16(old_hp, map.collision[5][4]);
}

/* --- Dig animation state --- */

/* Pushing against a diggable wall: no movement, but the dig-anim state is
 * set and the animation counter still advances. The original's
 * animate_player_sprite (seg_1000:3770) runs on EVERY move_player call
 * with direction != 0 — mode 1 (dig frames) when the tile ahead on the
 * movement axis is in the dig-anim wall set and the player is centered on
 * that axis (move_player tails, e.g. seg_1000:3927-3936). */
void test_dig_anim_state_while_blocked(void)
{
    map.tiles[5][4] = '7';        /* wall above (DIR_UP digs [row][col-1]) */
    map.collision[5][4] = 3000;

    p.direction = DIR_UP;         /* centered at (5,5) from setUp */
    p.anim_frame = 0;

    bool moved = player_move(&p, &map);
    TEST_ASSERT_FALSE(moved);                  /* blocked by the wall */
    TEST_ASSERT_EQUAL_UINT8(1, p.digging);     /* dig animation active */
    TEST_ASSERT_EQUAL_INT16(1, p.anim_frame);  /* counter advanced anyway */

    /* Counter keeps running while the key is held against the wall */
    player_move(&p, &map);
    player_move(&p, &map);
    TEST_ASSERT_EQUAL_INT16(3, p.anim_frame);
    TEST_ASSERT_EQUAL_UINT8(1, p.digging);

    /* Wall gone: same push becomes a normal walk (dig state clears) */
    map.tiles[5][4] = '0';
    map.collision[5][4] = 0;
    moved = player_move(&p, &map);
    TEST_ASSERT_TRUE(moved);
    TEST_ASSERT_EQUAL_UINT8(0, p.digging);
    TEST_ASSERT_EQUAL_INT16(4, p.anim_frame);
}

/* Dig-anim wall set (move_player tail): includes 'q' (0x71), excludes the
 * pushable boulder 'B' (0x42) — unlike tile_is_diggable_wall. Off-center
 * on the movement axis: walk animation even with a wall ahead. */
void test_dig_anim_wall_set_and_alignment(void)
{
    /* 'q' degraded wall IS in the dig-anim set */
    map.tiles[5][4] = 'q';
    map.collision[5][4] = 500;
    p.direction = DIR_UP;
    player_move(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(1, p.digging);

    /* 'B' boulder is NOT (original omits 0x42 from the anim check) */
    map.tiles[5][4] = 'B';
    player_move(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(0, p.digging);

    /* Off-center on the movement axis: no dig anim */
    map.tiles[5][4] = '7';
    map.collision[5][4] = 3000;
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5) + 2;   /* intra_y != 5 */
    player_move(&p, &map);
    TEST_ASSERT_EQUAL_UINT8(0, p.digging);
}

/* --- Shop gate toggle tiles (0xB4/0xB5): open/close 'l' gate tiles --- */
void test_shop_gate_toggle_open(void)
{
    /* Place a 0xB4 switch and several 'l' gate tiles */
    map.tiles[5][5] = 0xB4;
    map.tiles[10][10] = 'l';
    map.tiles[20][20] = 'l';
    map.tiles[30][30] = 0xB4;  /* another switch */

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    bool picked = player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_FALSE(picked);  /* tile not consumed */

    /* Switch tiles should become 0xB5 with cooldown overlay */
    TEST_ASSERT_EQUAL_HEX8(0xB5, map.tiles[5][5]);
    TEST_ASSERT_EQUAL_HEX8(0xB5, map.tiles[30][30]);
    TEST_ASSERT_EQUAL_UINT16(0x28, map.overlay[5][5]);
    TEST_ASSERT_EQUAL_UINT16(0x28, map.overlay[30][30]);

    /* Gate tiles should become floor '0' with layer4 bit 2 set */
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[20][20]);
    TEST_ASSERT_TRUE(map.layer4[10][10] & 2);
    TEST_ASSERT_TRUE(map.layer4[20][20] & 2);
}

void test_shop_gate_toggle_close(void)
{
    /* Set up state as if gates were previously opened */
    map.tiles[5][5] = 0xB5;
    map.tiles[10][10] = '0';
    map.layer4[10][10] = 2;
    map.tiles[20][20] = '0';
    map.layer4[20][20] = 2;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    bool picked = player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_FALSE(picked);

    /* Switch should revert to 0xB4 with cooldown */
    TEST_ASSERT_EQUAL_HEX8(0xB4, map.tiles[5][5]);
    TEST_ASSERT_EQUAL_UINT16(0x28, map.overlay[5][5]);

    /* Gates should revert to 'l' */
    TEST_ASSERT_EQUAL_HEX8('l', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_HEX8('l', map.tiles[20][20]);
}

void test_shop_gate_cooldown_prevents_retrigger(void)
{
    /* Place a 0xB4 switch with active cooldown (overlay > 1) */
    map.tiles[5][5] = 0xB4;
    map.overlay[5][5] = 10;  /* still on cooldown */
    map.tiles[10][10] = 'l';

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    player_check_pickup(&p, &map, 5, 5);

    /* Nothing should change because overlay >= 2 */
    TEST_ASSERT_EQUAL_HEX8(0xB4, map.tiles[5][5]);
    TEST_ASSERT_EQUAL_HEX8('l', map.tiles[10][10]);
    TEST_ASSERT_EQUAL_UINT16(10, map.overlay[5][5]);
}

void test_shop_gate_round_trip(void)
{
    /* Full cycle: open then close */
    map.tiles[5][5] = 0xB4;
    map.tiles[10][10] = 'l';

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);

    /* Step 1: Open (0xB4 → 0xB5, 'l' → '0') */
    player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_EQUAL_HEX8(0xB5, map.tiles[5][5]);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[10][10]);

    /* Clear cooldown to allow re-trigger */
    map.overlay[5][5] = 0;

    /* Step 2: Close (0xB5 → 0xB4, layer4-marked → 'l') */
    player_check_pickup(&p, &map, 5, 5);
    TEST_ASSERT_EQUAL_HEX8(0xB4, map.tiles[5][5]);
    TEST_ASSERT_EQUAL_HEX8('l', map.tiles[10][10]);
}

/* Pickup fires during movement only when crossing tile center, checking tile ahead.
 * This prevents teleporters (0x9C) from re-triggering every frame.
 * (seg_1000:3920-4043: FUN_1000_5073 called only when intra == 5) */
void test_pickup_fires_on_tile_center_crossing(void)
{
    /* Place treasure at tiles[5][4] — one col above player at row=5, col=5.
     * DIR_UP checks tiles[row][col-1]. */
    map.tiles[5][4] = 0x92;  /* 15 cash treasure */
    map.collision[5][4] = 0;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);  /* centered: intra_y = 5 */
    p.direction = DIR_UP;
    int32_t old_earned = p.earned;

    /* First move: intra_y == 5 at start → pickup fires on tile ahead */
    player_move(&p, &map);
    TEST_ASSERT_EQUAL_INT32(old_earned + 15, p.earned);
    TEST_ASSERT_EQUAL_HEX8('0', map.tiles[5][4]);  /* consumed */
}

void test_teleporter_no_retrigger_on_move(void)
{
    /* Teleporter at tiles[5][4] — player at row=5, col=5 moves UP.
     * DIR_UP pickup checks tiles[row][col-1] = tiles[5][4]. */
    map.tiles[5][4] = 0x9C;
    map.tiles[20][20] = 0x9C;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);  /* centered: intra_y = 5 */
    p.direction = DIR_UP;

    /* Move: at intra_y == 5, pickup checks tile (4, 5) = teleporter → warps */
    player_move(&p, &map);

    /* Player should have been warped to the other teleporter */
    int16_t warp_x = p.x_pos;
    int16_t warp_y = p.y_pos;
    bool at_dest = (warp_x == tile_to_pixel_x(20) && warp_y == tile_to_pixel_y(20));
    TEST_ASSERT_TRUE(at_dest);

    /* Now player is at destination tile center (20, 20) moving UP.
     * The pickup check will look at tile (19, 20) — which is floor '0'.
     * So the destination teleporter should NOT retrigger. */
    p.direction = DIR_UP;
    int16_t pre_x = p.x_pos;
    int16_t pre_y = p.y_pos;

    player_move(&p, &map);

    /* Player should have moved normally (1 pixel up), NOT teleported again.
     * The y_pos should be pre_y - 1, not some random teleport location. */
    TEST_ASSERT_EQUAL_INT16(pre_y - 1, p.y_pos);
    TEST_ASSERT_EQUAL_INT16(pre_x, p.x_pos);
}

/* --- Digging works correctly across round resets --- */
void test_dig_works_after_round_reset(void)
{
    /* Simulate round 1: dig wall at tiles[5][4] (col-1 from player, DIR_UP) */
    map.tiles[5][4] = '7';
    map.collision[5][4] = 100;

    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;
    p.digging_power = 10;
    p.bonus_stat = 5;

    player_dig(&p, &map);
    TEST_ASSERT_EQUAL_UINT16(85, map.collision[5][4]);  /* 100 - 15 = 85 */

    /* Simulate round reset: player_reset_for_round restores digging_power to 1 */
    p.digging_power = 50;  /* accumulated from gems during round 1 */
    player_reset_for_round(&p);

    TEST_ASSERT_EQUAL(1, p.digging_power);
    TEST_ASSERT_EQUAL(0, p.bonus_stat);

    /* Reinitialize map (simulating round_init for round 2) */
    memset(&map, 0, sizeof(map));
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map.tiles[r][c] = '0';
    map.tiles[5][4] = '8';  /* fresh wall in round 2 */
    map_init_collision(&map);
    TEST_ASSERT_EQUAL_UINT16(0x4CB, map.collision[5][4]);  /* 1227 */

    /* Position player and dig again */
    p.x_pos = tile_to_pixel_x(5);
    p.y_pos = tile_to_pixel_y(5);
    p.direction = DIR_UP;

    player_dig(&p, &map);

    /* digging_power=1, bonus_stat=0 → damage=1 per frame */
    TEST_ASSERT_EQUAL_UINT16(0x4CB - 1, map.collision[5][4]);
    TEST_ASSERT_EQUAL_HEX8('8', map.tiles[5][4]);  /* not yet degraded */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_passable_tiles);
    RUN_TEST(test_wall_blocks);
    RUN_TEST(test_move_on_open_floor);
    RUN_TEST(test_move_blocked_by_wall);
    RUN_TEST(test_treasure_pickup);
    RUN_TEST(test_health_pickup);
    RUN_TEST(test_mystery_box_pickup);
    RUN_TEST(test_mystery_box_repeated_gives_weapons);
    RUN_TEST(test_teleporter_warps_player);
    RUN_TEST(test_teleporter_single_does_nothing);
    RUN_TEST(test_wall_slide_perpendicular_snap);
    RUN_TEST(test_wall_slide_around_corner);
    RUN_TEST(test_wall_slide_to_center);
    RUN_TEST(test_dig_reduces_wall_hp);
    RUN_TEST(test_dig_destroys_wall);
    RUN_TEST(test_dig_hp1_wall_destroys);
    RUN_TEST(test_dig_hp0_tile_skipped);
    RUN_TEST(test_dig_wall_degradation_stages);
    RUN_TEST(test_dig_indestructible_wall);
    RUN_TEST(test_dig_requires_alignment);
    RUN_TEST(test_dig_anim_state_while_blocked);
    RUN_TEST(test_dig_anim_wall_set_and_alignment);
    RUN_TEST(test_shop_gate_toggle_open);
    RUN_TEST(test_shop_gate_toggle_close);
    RUN_TEST(test_shop_gate_cooldown_prevents_retrigger);
    RUN_TEST(test_shop_gate_round_trip);
    RUN_TEST(test_pickup_fires_on_tile_center_crossing);
    RUN_TEST(test_teleporter_no_retrigger_on_move);
    RUN_TEST(test_dig_works_after_round_reset);
    return UNITY_END();
}
