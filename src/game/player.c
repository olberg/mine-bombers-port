#include "game/player.h"
#include "game/bombs.h"
#include "game/movement.h"
#include <string.h>
#include <stdio.h>

Player g_players[MAX_PLAYERS];
int    g_num_active_players = 0;

/*
 * Check if a tile ID is any player's bomb signature tile.
 * If so, returns 0 for sig[0] type or 1 for sig[1] type. Otherwise returns -1.
 */
static int classify_bomb_sig(uint8_t tile_id)
{
    /* All sig[0] tiles: 'c'(0x63), 'g'(0x67), 0x82, 'i'(0x69) */
    if (tile_id == 0x63 || tile_id == 0x67 || tile_id == 0x82 || tile_id == 0x69)
        return 0;
    /* All sig[1] tiles: 'd'(0x64), 'h'(0x68), 0x83, 'j'(0x6A) */
    if (tile_id == 0x64 || tile_id == 0x68 || tile_id == 0x83 || tile_id == 0x6A)
        return 1;
    return -1;
}

int player_weapon_index(uint8_t weapon_id)
{
    /* Check base cycle weapons */
    for (int i = 0; i < WEAPON_CYCLE_USED; i++) {
        if (WEAPON_CYCLE_ORDER[i] == weapon_id) return i;
    }
    /* Check bomb signature tiles → map to sig slots */
    int sig = classify_bomb_sig(weapon_id);
    if (sig == 0) return WEAPON_SIG0_SLOT;
    if (sig == 1) return WEAPON_SIG1_SLOT;
    return -1;
}

void player_init_defaults(Player *p, int player_num)
{
    memset(p, 0, sizeof(*p));

    p->player_num = (uint8_t)player_num;
    snprintf(p->name, sizeof(p->name), "Player %d", player_num + 1);

    /* Starting position varies per player — will be set by map spawn points */
    p->x_pos = 0;
    p->y_pos = 0;
    p->x_velocity = 0;
    p->y_velocity = 0;
    p->direction = 0;       /* stopped */
    p->last_direction = 1;  /* facing RIGHT — original inits +0xA6 = 1
                             * (seg_1010:7404-7407) */
    p->speed_divisor = 0;

    p->health = 100;     /* original: DAT_1038_1c07 = 100 (seg_1010:7189) */
    p->max_health = 100;
    p->cash = 0;
    p->earned = 0;
    p->digging_power = 1;   /* reset per round (seg_1010:7073: DAT_1038_1c92 = 1) */
    p->bonus_stat = 0;
    p->dead = 0;
    p->active = 1;
    p->kills = 0;
    p->round_wins = 0;

    /* No starting inventory: a fresh match begins with every weapon count
     * at 0 — the original's first shop shows ITEMS 0 with no quantity bar
     * on the small-bomb cell (DOSBox capture, 2026-06-11). The previous
     * "start with 5 small bombs" was a port invention. */
    p->selected_weapon = WEAPON_SMALL_BOMB;

    p->anim_frame = 0;
    p->cooldown = 0;

    /*
     * Per-player bomb signature tiles (decompiled seg_1010:5562-5567).
     * These are placed on the map instead of a generic tile when a player uses
     * weapons at inventory offsets 0xB8 and 0xBA (the two per-player remote
     * bomb variants). They allow remote detonation to identify bomb ownership.
     *
     * Player 1: 'c' (0x63), 'd' (0x64)
     * Player 2: 'g' (0x67), 'h' (0x68)
     * Player 3: 0x82, 0x83
     * Player 4: 'i' (0x69), 'j' (0x6A)
     */
    switch (player_num) {
    case 0: p->bomb_sig[0] = 0x63; p->bomb_sig[1] = 0x64; break;
    case 1: p->bomb_sig[0] = 0x67; p->bomb_sig[1] = 0x68; break;
    case 2: p->bomb_sig[0] = 0x82; p->bomb_sig[1] = 0x83; break;
    case 3: p->bomb_sig[0] = 0x69; p->bomb_sig[1] = 0x6A; break;
    default: p->bomb_sig[0] = 0x63; p->bomb_sig[1] = 0x64; break;
    }

    p->lives = 3;
    p->steel_plates = 0;
}

/*
 * Reset per-round player state. Recalculates health from persistent upgrades.
 *
 * Decompiled ref: game_state_update (seg_1010:7399-7425)
 *   - Zeros dead flags (line 7412-7415)
 *   - Calls FUN_1010_c15c (line 7425) which recalculates health:
 *       health = steel_plate_count * 100 + 100
 *       max_health = health
 *
 * In the original, the formula also adds DAT_1038_1c09 (offset +0x04),
 * which appears to be a secondary health bonus. For now we use just
 * steel_plates as the upgrade counter.
 */
void player_reset_for_round(Player *p)
{
    p->dead = 0;
    p->active = 1;
    p->direction = DIR_STOP;
    p->anim_frame = 0;
    p->digging = 0;
    p->cooldown = 0;
    p->money_bomb_counter = 0;
    p->x_velocity = 0;
    p->y_velocity = 0;

    /* Recalculate health from persistent upgrades (FUN_1010_c15c) */
    int16_t new_health = (int16_t)(p->steel_plates * 100 + 100);
    p->health = new_health;
    p->max_health = new_health;

    /* Zero this round's earnings (game_state_update, seg_1010:7428-7435).
     * The previous round's earnings were already banked or forfeited by
     * round_apply_scoring (FUN_1000_a17c). */
    p->earned = 0;

    /* Reset digging_power to 1 per round (game_state_update, seg_1010:7073).
     * In the original, DAT_1038_1c92 (digging_power) is set to 1 each round.
     * Stat gems picked up during gameplay accumulate into this value.
     * The total dig damage per frame is: digging_power + bonus_stat. */
    p->digging_power = 1;

    /* Recalculate dig upgrade bonus from upgrade counts (game_state_update seg_1010:7065).
     * Formula: bonus_stat = lg_rockpick * 3 + rockpick + powerdrill * 5
     * This is added to digging_power in the dig damage formula (seg_1000:3722-3724). */
    p->bonus_stat = player_dig_upgrade_bonus(p);

    /* Reset per-round harness observability counters (no original
     * equivalent — the original's persistent stats accumulate in
     * match_stats, which deliberately survives this reset). */
    p->kills = 0;
    p->deaths = 0;
    p->bombs_placed = 0;
}

/*
 * Post-shop recompute. The original runs the per-round resets and
 * placement BEFORE the shop (FUN_1010_c5f4, seg_1000:7096), then after the
 * shop re-runs only game_state_update (seg_1010:7061-7082: dig tool bonus,
 * digging_power := 1) and FUN_1010_c15c (seg_1010:7086-7099: health :=
 * steel_plates*100 + 100) at seg_1000:7128-7129 — so dig tools and steel
 * plates bought in the shop take effect for the round about to start.
 * (c15c's extra DAT_1038_1c09 term is vestigial: its only writer in v3.11
 * is the zeroing in process_menu_selection, seg_1010:7193-7196.)
 */
void player_apply_shop_purchases(Player *p)
{
    p->bonus_stat = player_dig_upgrade_bonus(p);
    p->digging_power = 1;
    int16_t new_health = (int16_t)(p->steel_plates * 100 + 100);
    p->health = new_health;
    p->max_health = new_health;
}

void player_money_bomb_tick(Player *p)
{
    if (!p || p->dead) return;
    if (p->money_bomb_counter < 2) {
        /* Counter is 0 (inactive) or 1 (deduction time) */
        if (p->money_bomb_counter == 1) {
            /* Deduct the loaned 300 DIGGING POWER (seg_1010:7664-7671
             * subtracts from +0xA8 and redraws the dig HUD line) */
            p->money_bomb_counter = 0;
            p->digging_power -= MONEY_BOMB_DIG_LOAN;
        }
        return;
    }
    /* Counter >= 2: decrement (seg_1010:7674-7678) */
    p->money_bomb_counter--;
}

/*
 * Build the full weapon cycle for a player, including their bomb_sig tiles.
 * Decompiled cycle (FUN_1000_3974): positions 0-6 match base, then sig[0],
 * sig[1], then positions 7-20 of the base cycle.
 *
 * Total cycle length = WEAPON_CYCLE_USED + 2 = 23.
 */
#define FULL_CYCLE_LEN (WEAPON_CYCLE_USED + 2)

static void build_full_cycle(const Player *p, uint8_t out_tiles[FULL_CYCLE_LEN],
                              int out_slots[FULL_CYCLE_LEN])
{
    int pos = 0;
    /* Base positions 0-6 (W through 0x81) */
    for (int i = 0; i <= 6; i++) {
        out_tiles[pos] = WEAPON_CYCLE_ORDER[i];
        out_slots[pos] = i;
        pos++;
    }
    /* Insert bomb_sig[0] and bomb_sig[1] */
    out_tiles[pos] = p->bomb_sig[0];
    out_slots[pos] = WEAPON_SIG0_SLOT;
    pos++;
    out_tiles[pos] = p->bomb_sig[1];
    out_slots[pos] = WEAPON_SIG1_SLOT;
    pos++;
    /* Base positions 7-20 (0x65 through 0xAB) */
    for (int i = 7; i < WEAPON_CYCLE_USED; i++) {
        out_tiles[pos] = WEAPON_CYCLE_ORDER[i];
        out_slots[pos] = i;
        pos++;
    }
}

void player_cycle_weapon(Player *p, int direction)
{
    uint8_t tiles[FULL_CYCLE_LEN];
    int     slots[FULL_CYCLE_LEN];
    build_full_cycle(p, tiles, slots);

    /* Find current position in the full cycle */
    int current = -1;
    for (int i = 0; i < FULL_CYCLE_LEN; i++) {
        if (tiles[i] == p->selected_weapon) {
            current = i;
            break;
        }
    }
    if (current < 0) current = 0;

    /* Search for next weapon with stock > 0 */
    for (int i = 1; i <= FULL_CYCLE_LEN; i++) {
        int ci;
        if (direction >= 0) {
            ci = (current + i) % FULL_CYCLE_LEN;
        } else {
            ci = (current - i + FULL_CYCLE_LEN) % FULL_CYCLE_LEN;
        }

        if (p->weapons[slots[ci]] > 0) {
            p->selected_weapon = tiles[ci];
            return;
        }
    }
    /* No weapons with stock — keep current selection */
}
