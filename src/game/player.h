#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PLAYERS    4
#define WEAPON_SLOTS   27

/* Weapon tile IDs in cycling order (W -> X -> Y -> ... -> 0xAB -> W) */
#define WEAPON_SMALL_BOMB   0x57  /* 'W' */
#define WEAPON_MEDIUM_BOMB  0x58  /* 'X' */
#define WEAPON_LARGE_BOMB   0x59  /* 'Y' */
#define WEAPON_ROCKET_DIR   0x5A  /* 'Z' directional rocket */
#define WEAPON_MEGA_BOMB    0x7F
#define WEAPON_TYPE_80      0x80
#define WEAPON_TYPE_81      0x81
#define WEAPON_EXPLOSIVE_E  0x65  /* 'e' */
#define WEAPON_TYPE_8A      0x8A
#define WEAPON_MINE         0x9D
#define WEAPON_ROCKET_R     0x72  /* 'r' rocket launcher */
#define WEAPON_TIMER_BOMB   0xA1
#define WEAPON_MONEY_BOMB   0xB0
#define WEAPON_TYPE_A2      0xA2
#define WEAPON_TYPE_A4      0xA4
#define WEAPON_DIR_ARROW    0xA5
#define WEAPON_TELE_BOMB    0xA9
#define WEAPON_TYPE_9C      0x9C
#define WEAPON_CREATURE_N   0x6E  /* 'n' creature spawner */
#define WEAPON_PROX_MINE    0x6F  /* 'o' proximity mine */
#define WEAPON_RANDOM_BOMB  0xAB

/*
 * Weapon cycling order (matching decompiled FUN_1000_3974, seg_1000:2398-2488).
 *
 * Slots 21 and 22 hold per-player bomb signature weapons at inventory
 * offsets 0xB8 and 0xBA. In the original, the cycle goes:
 *   ... → 0x81 → bomb_sig[0] → bomb_sig[1] → 0x65 → ...
 * The actual tile IDs vary per player (see player_init_defaults).
 * player_cycle_weapon() handles the dynamic insertion.
 */
static const uint8_t WEAPON_CYCLE_ORDER[WEAPON_SLOTS] = {
    0x57, 0x58, 0x59, 0x5A, 0x7F, 0x80, 0x81, 0x65,
    0x8A, 0x9D, 0x72, 0xA1, 0xB0, 0xA2, 0xA4, 0xA5,
    0xA9, 0x9C, 0x6E, 0x6F, 0xAB,
    0x00, 0x00,  /* 21, 22: bomb_sig[0], bomb_sig[1] (per-player) */
    0x00, 0x00, 0x00, 0x00
};

#define WEAPON_CYCLE_USED  21  /* base weapons in cycle (excl. sig slots) */
#define WEAPON_SIG0_SLOT   21  /* weapons[] index for bomb_sig[0] (orig 0xB8) */
#define WEAPON_SIG1_SLOT   22  /* weapons[] index for bomb_sig[1] (orig 0xBA) */

typedef struct {
    /* Identity */
    char     name[27];          /* runtime name: "N " player-number prefix +
                                   up to 24 record chars + NUL. The original
                                   appends the prefix at player-select exit
                                   (FUN_1000_3276 tail, rtl_strcopy limit
                                   0x1a = 26); the shop masks the digit. */
    uint8_t  player_num;        /* 0-3 (index) */
    uint8_t  color;             /* color index from config */

    /* Position & movement */
    int16_t  x_pos;             /* pixel X */
    int16_t  y_pos;             /* pixel Y */
    int16_t  x_velocity;        /* speed bonus X (>0 = double move) */
    int16_t  y_velocity;        /* speed bonus Y (>0 = double move) */
    uint8_t  direction;         /* 0=stop, 1=right, 2=left, 3=up, 4=down */
    uint8_t  last_direction;    /* facing; persists when stopped (+0xA6) */
    int16_t  speed_divisor;     /* speed boost factor (>1 = double move) */

    /* Stats */
    int16_t  health;
    int16_t  max_health;
    /* Two money fields, as in the original:
     *   cash   = wallet (original +0xEA/0xEC, DAT_1038_1cd4): spent in the
     *            shop, receives round-end payouts, ranked on results screen.
     *   earned = cash collected this round (original +0xE6/0xE8,
     *            DAT_1038_1cd0): credited by treasure pickups, zeroed at
     *            round start (seg_1010:7428), banked into cash (or
     *            forfeited to the survivors' pool if dead) by
     *            round_apply_scoring (FUN_1000_a17c).
     * The in-game HUD money line shows cash + earned (seg_1010:3515). */
    int32_t  cash;
    int32_t  earned;
    int16_t  digging_power;     /* base damage stat (offset 0xA8 in original) */
    int16_t  bonus_stat;        /* bonus accumulator (offset 0xAC in original) */
    uint8_t  dead;              /* 0=alive, 1=dead */
    uint8_t  active;            /* collision/active flag */
    int16_t  kills;
    int16_t  round_wins;

    /* Match-stats accumulator — port of the original's heap block pointed
     * to by player +0xFF (40 bytes = 10 dwords, allocated in setup, zeroed
     * on menu→match transition, merged into the PLAYERS.DAT record ONCE
     * per match by FUN_1000_15c7: record.stats[i] += block[i]). Known
     * dwords: [0]=matches played (results screen, all players),
     * [1]=matches won (results screen, rank-0 only), [2]=rounds played
     * (FUN_1000_a17c), [4]=treasures collected (pickup handler,
     * seg_1000:3513-3517), [5]=money earned (FUN_1000_a17c). [3] and
     * [6..9] have no identified writers yet. */
    uint32_t match_stats[10];

    /* Weapon inventory: quantity per weapon type (indexed by WEAPON_CYCLE_ORDER) */
    int16_t  weapons[WEAPON_SLOTS];
    uint8_t  selected_weapon;   /* current weapon tile ID */

    /* Animation */
    int16_t  anim_frame;
    int16_t  cooldown;

    /* Key bindings (Raylib key codes at runtime) */
    int      key_up, key_down, key_left, key_right;
    int      key_stop, key_bomb, key_remote, key_cycle;

    /* Bomb ownership */
    uint8_t  bomb_sig[2];       /* signature tiles for bomb ownership */

    /* Dig upgrade counts (purchased in shop, persist across rounds).
     * Original offsets: 0xD2 (rockpick), 0xD4 (lg_rockpick), 0xD6 (powerdrill).
     * game_state_update (seg_1010:7065) computes:
     *   dig_upgrade_stat = lg_rockpick * 3 + rockpick + powerdrill * 5
     * This value is stored at original offset +0x91 and used alongside
     * digging_power (+0xA8) for total dig damage. */
    int16_t  rockpick;          /* qty of Rockpick upgrades (shop item 18, offset 0xD2) */
    int16_t  lg_rockpick;       /* qty of Lg rockpick upgrades (shop item 19, offset 0xD4) */
    int16_t  powerdrill;        /* qty of Powerdrill upgrades (shop item 20, offset 0xD6) */

    /* Health upgrades (steel plates bought in shop).
     * Original: inventory offset 0xE0, player struct offset +0xC5.
     * Each steel plate adds 100 to max health at round start.
     * Decompiled ref: FUN_1010_c15c (seg_1010:7086-7098) recalculates:
     *   health = steel_plates * 100 + 100 + health_bonus
     */
    int16_t  steel_plates;

    /* Cheat visual effect (set by player_apply_cheat, used by renderer).
     * CHEAT_INVIS: player sprite not drawn (invisible).
     * CHEAT_MUTATION: player uses monster sprite set 10 (Mon 3). */
    uint8_t  cheat_visual;      /* 0=none, CHEAT_INVIS or CHEAT_MUTATION */

    /* Money bomb loan counter (original offset +0x104/+0x106, 32-bit).
     * When money bomb is used: set to 10, +300 cash immediately.
     * Decremented every 18 frames by per_player_update.
     * When reaches 1: deducts 300 cash, zeroes out.
     * This is a "loan" system — if the player spends the cash before
     * the timer expires, they profit; otherwise it's taken back.
     * Decompiled ref: process_weapons (seg_1000:2675-2686),
     *                 per_player_update (seg_1010:7650-7681). */
    int32_t  money_bomb_counter;

    /* Single-player */
    int16_t  lives;

    /* Runtime stat accumulators — reset at game start, accumulated into
     * PlayerRecord.stats[] after each multiplayer round via player_db_update_record.
     * Decompiled ref: FUN_1000_15c7 (seg_1000:867-955) reads these from
     * the player struct and adds them to the persistent record. */
    int16_t  bombs_placed;       /* incremented in bomb_place() */
    int16_t  deaths;             /* incremented in check_player_death() */
    int16_t  record_slot;        /* PLAYERS.DAT slot index (0-31), set by player_init_from_record */
} Player;

extern Player g_players[MAX_PLAYERS];
extern int    g_num_active_players;

/* Initialize player to starting defaults for the given slot index (0-3). */
void player_init_defaults(Player *p, int player_num);

/* Reset per-round player state (health, dead flag, position/anim).
 * Called at the start of each round. Recalculates health from steel_plates.
 * Decompiled ref: game_state_update (seg_1010:7399-7454) + FUN_1010_c15c. */
void player_reset_for_round(Player *p);

/* Post-shop recompute: re-apply what the original's second
 * game_state_update + FUN_1010_c15c do after the shop (seg_1000:7128-7129)
 * — dig tool bonus, digging_power := 1, health/max := plates*100 + 100 —
 * so shop purchases take effect for the upcoming round. All other
 * per-round resets run BEFORE the shop (FUN_1010_c5f4). */
void player_apply_shop_purchases(Player *p);

/* Cycle to next/prev weapon with stock > 0. direction: +1 forward, -1 backward. */
void player_cycle_weapon(Player *p, int direction);

/* Map weapon tile ID to inventory index (0..WEAPON_CYCLE_USED-1). Returns -1 if invalid. */
int player_weapon_index(uint8_t weapon_id);

/* Money bomb per-player tick: called every 18 frames (from game_tick_update).
 * Decrements money_bomb_counter and deducts 300 cash when it reaches 1.
 * Decompiled ref: per_player_update (seg_1010:7650-7681). */
void player_money_bomb_tick(Player *p);

/* Recompute dig upgrade bonus from upgrade counts.
 * Formula: lg_rockpick * 3 + rockpick + powerdrill * 5
 * Decompiled ref: game_state_update (seg_1010:7065) */
static inline int16_t player_dig_upgrade_bonus(const Player *p)
{
    return (int16_t)(p->lg_rockpick * 3 + p->rockpick + p->powerdrill * 5);
}

#endif
