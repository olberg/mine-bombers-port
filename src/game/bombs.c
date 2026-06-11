#include "game/bombs.h"
#include "game/config.h"
#include "game/weapons.h"
#include "game/player_db.h"
#include "game/movement.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include "game/entity.h"
#include "audio/sfx.h"
#include "util/prng.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>

/* Module-level entity list pointer, set each frame via bombs_set_entity_list().
 * Used by bombs_check_player_damage() to iterate entities during explosions. */
static Entity *s_entity_head = NULL;

/* Forward declarations for functions referenced before definition */
static bool rocket_tile_passable(uint8_t tile);
static void apply_explosion_hit(TileMap *map, int col, int row, bool strong,
                                uint8_t player_damage);

void bombs_set_entity_list(Entity *head)
{
    s_entity_head = head;
}

/* Direction-to-arrow-tile mapping (process_weapons seg_1000:2744-2773:
 * the placed weapon char 0xA5 is rewritten per +0xA4/+0xA6 — dir 1→0xA5,
 * 2→0xA6, 3→0xA8, 4→0xA7). If neither direction matches (facing 0) the
 * original leaves the initial 0xA5 in place, so the fallback is RIGHT. */
static uint8_t direction_to_arrow(uint8_t direction, uint8_t last_direction)
{
    uint8_t dir = direction ? direction : last_direction;
    switch (dir) {
    case DIR_RIGHT: return ARROW_RIGHT;  /* 0xA5 */
    case DIR_LEFT:  return ARROW_LEFT;   /* 0xA6 */
    case DIR_UP:    return ARROW_UP;     /* 0xA8 */
    case DIR_DOWN:  return ARROW_DOWN;   /* 0xA7 */
    default:        return ARROW_RIGHT;
    }
}

/*
 * Get the initial overlay (fuse) value for a placed bomb tile.
 * Derived from decompiled process_weapons (seg_1000:2582), lines 2707-2777.
 */
static uint16_t bomb_init_overlay(uint8_t tile, TileMap *map, int col, int row)
{
    switch (tile) {
    /* Overlay=0: explosive (instant) and per-player bomb signatures (remote).
     * Bomb signatures sit with overlay=0 until remote detonation sets it to 1.
     * See decompiled process_weapons, seg_1000:2707-2710. */
    case 0x65: /* 'e' explosive */
    case BOMB_SIG_P3_A: case BOMB_SIG_P3_B: /* Player 3: 0x82, 0x83 */
    case BOMB_SIG_P1_A: case BOMB_SIG_P1_B: /* Player 1: 'c', 'd' */
    case BOMB_SIG_P2_A: case BOMB_SIG_P2_B: /* Player 2: 'g', 'h' */
    case BOMB_SIG_P4_A: case BOMB_SIG_P4_B: /* Player 4: 'i', 'j' */
        return 0;

    case 0x7F: /* Mega bomb */
        return 260;  /* 0x104 */

    case 0x9D: /* Mine */
        return 280;  /* 0x118 */

    case 0xA9: /* Teleporter bomb - immediate */
        return 1;

    case 0xA1: /* Timer bomb */
        return 90;   /* 0x5A */

    case 0xA4: /* Power bomb - immediate */
        return 0;

    case 0x9C: /* Warp bomb - immediate */
        return 0;

    case 0x59: /* 'Y' Large bomb */
        return 80;   /* 0x50 */

    case 0xAB: /* Random bomb: fuse = random(80) + 80 */
    {
        uint16_t fuse = (uint16_t)(mb_random(80)) + 80;
        uint16_t coll = (uint16_t)(mb_random(20)) + 7;
        map->collision[row][col] = coll;
        return fuse;
    }

    case 0x6F: /* 'o' Proximity mine: fuse = random(80) */
    {
        uint16_t fuse = (uint16_t)(mb_random(80));
        map->collision[row][col] = BOMB_COLLISION_PROX;
        return fuse;
    }

    /* Directional arrows — immediate. The original's -0x5b placement case
     * rewrites the tile to 0xA5-0xA8 by direction and sets overlay 1 for
     * ALL FOUR (seg_1000:2744-2773); only handling 0xA5 here left the
     * other three with the 100-tick default. */
    case ARROW_RIGHT: case ARROW_LEFT: case ARROW_DOWN: case ARROW_UP:
        return 1;

    default:
        return 100;  /* W, X, and all others default to 100 */
    }
}

bool bomb_place(Player *p, TileMap *map)
{
    if (!p || !map || p->dead) return false;

    uint8_t weapon = p->selected_weapon;
    int inv_idx = player_weapon_index(weapon);
    if (inv_idx < 0 || p->weapons[inv_idx] <= 0) return false;

    TraceLog(LOG_DEBUG, "BOMB_PLACE: P%d weapon=0x%02X inv_idx=%d qty=%d cooldown=%d",
             p->player_num, weapon, inv_idx, p->weapons[inv_idx], p->cooldown);

    /* Compute player's tile position (center of sprite).
     * VGA convention: row from screen X, col from screen Y. */
    int row = pixel_to_tile_row(p->x_pos + SPRITE_W / 2);
    int col = pixel_to_tile_col(p->y_pos + SPRITE_H / 2);
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return false;

    /* "Money bomb" 0xB0 = SUPERpora (super drill, HISTORIA v3.1): a +300
     * DIGGING POWER loan (seg_1000:2675-2686 adds 300 to +0xA8 and redraws
     * the dig HUD line), NOT cash.
     * Gated by money_bomb_counter == 0; counter = 10, ticking down via
     * player_money_bomb_tick (every 18 frames); at 1 the 300 is deducted
     * back. The original's branch never reaches the inventory decrement
     * (bVar4 stays false), so using it consumes NO ammo. */
    if (weapon == WEAPON_MONEY_BOMB) {
        if (p->money_bomb_counter != 0) return false;
        p->money_bomb_counter = MONEY_BOMB_COOLDOWN;
        p->digging_power += MONEY_BOMB_DIG_LOAN;
        TraceLog(LOG_DEBUG, "MONEY_BOMB: P%d counter set to %d, dig now %d",
                 p->player_num, p->money_bomb_counter, p->digging_power);
        return true;
    }

    /* Rocket launcher (0x72): fire a beam in facing direction.
     * Decompiled ref: seg_1000:2657-2664 dispatches to FUN_1010_4dff. */
    if (weapon == WEAPON_ROCKET_R) {
        uint8_t dir = p->direction ? p->direction : p->last_direction;
        rocket_launcher_fire(map, col, row, dir);
        p->weapons[inv_idx]--;
        p->bombs_placed++;
        return true;
    }

    /* Directional rocket (0x5A): expanding cross in facing direction.
     * Decompiled ref: seg_1000:2666-2673 dispatches to FUN_1010_45cd. */
    if (weapon == WEAPON_ROCKET_DIR) {
        uint8_t dir = p->direction ? p->direction : p->last_direction;
        directional_rocket_fire(map, col, row, dir);
        p->weapons[inv_idx]--;
        p->bombs_placed++;
        return true;
    }

    /* Creature spawner: handled in round.c (needs entity list access). */
    if (weapon == WEAPON_CREATURE_N) {
        /* Just consume and return true — caller handles spawning */
        p->weapons[inv_idx]--;
        p->bombs_placed++;
        return true;
    }

    /* Normal bomb placement: check tile is available (collision must be 0) */
    if (map->collision[row][col] != 0) return false;

    /* Place the bomb tile */
    uint8_t placed_tile = weapon;

    /* Directional arrows: encode direction into the tile value */
    if (weapon == WEAPON_DIR_ARROW) {
        placed_tile = direction_to_arrow(p->direction, p->last_direction);
    }

    map->tiles[row][col] = placed_tile;
    map->bomb_owner[row][col] = p->player_num;

    /* Set collision value: 0 for arrows (0xA5-0xA8), 20 for most bombs */
    if (placed_tile >= ARROW_RIGHT && placed_tile <= ARROW_UP) {
        map->collision[row][col] = 0;
    } else {
        map->collision[row][col] = BOMB_COLLISION_DEFAULT;
    }

    /* Set overlay (fuse timer) - may override collision for special types */
    map->overlay[row][col] = bomb_init_overlay(placed_tile, map, col, row);

    /* Consume weapon from inventory */
    p->weapons[inv_idx]--;

    /* Track stat for persistent record (FUN_1000_15c7 accumulation) */
    p->bombs_placed++;

    return true;
}

/*
 * Apply explosion effect at a single tile.
 * Matches FUN_1010_1326 (seg_1010:754-823) behavior.
 *
 * strong=false (param_2==0): "weak" mode — walls get fixed HP (500/1000),
 *   reinforced walls degrade one stage. Walls survive the explosion.
 * strong=true (param_2!=0): "strong" mode — everything becomes fire (0x84).
 *
 * player_damage: HP damage to apply to any player/entity on this tile
 *   (FUN_1010_98dd call inside FUN_1010_1326). 0 means presence check only.
 */
static void apply_explosion_hit(TileMap *map, int col, int row, bool strong,
                                uint8_t player_damage)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return;

    uint8_t tile = map->tiles[row][col];

    /* Indestructible ('1') or gate tiles: explosion blocked */
    if (tile == '1' || tile == 'l') return;
    /* Skip gate switches */
    if (tile == 0xB4 || tile == 0xB5) return;

    /* Wall classification (seg_1010:772): ONLY intact walls '7'-'9' and
     * 'A'-'F'. Already-degraded walls ('p','q','5','6') are NOT in the set —
     * they take the fire branch below, i.e. the second weak hit DESTROYS
     * them (the classic two-bomb wall kill). They used to be misclassified
     * as walls here, which reset them to fresh 500/1000 HP on every hit and
     * made walls unkillable by bombs alone. */
    bool is_wall = (tile >= '7' && tile <= '9') || (tile >= 'A' && tile <= 'F');
    bool is_reinforced = (tile == 0xAC || tile == 0xAD || tile == 0xAE);

    if (!is_wall) {
        /* Non-wall tile */
        if (!is_reinforced) {
            /* Place fire (seg_1010:775-778) */
            map->tiles[row][col] = TILE_EXPLOSION;
            /* Check player/entity damage BEFORE setting overlay
             * (FUN_1010_98dd called at seg_1010:776 before overlay set at 778) */
            bombs_check_player_damage(map, col, row, player_damage);
            map->overlay[row][col] = 3;
            map->collision[row][col] = 0;
        } else if (strong) {
            /* Strong mode: reinforced walls become fire (seg_1010:795-796) */
            map->tiles[row][col] = TILE_EXPLOSION;
            map->overlay[row][col] = 3;
            map->collision[row][col] = 0;
        } else {
            /* Weak mode: reinforced walls degrade one stage (seg_1010:780-792) */
            if (tile == 0xAC) {
                map->collision[row][col] = 4000;
                map->tiles[row][col] = 0xAD;
            } else if (tile == 0xAD) {
                map->collision[row][col] = 2000;
                map->tiles[row][col] = 0xAE;
            } else { /* 0xAE */
                map->tiles[row][col] = TILE_EXPLOSION;
                map->overlay[row][col] = 3;
                map->collision[row][col] = 0;
            }
        }
    } else {
        /* Intact wall tile ('7'-'9','A'-'F') */
        if (strong) {
            /* Strong mode: walls become fire (seg_1010:811-813) */
            map->tiles[row][col] = TILE_EXPLOSION;
            map->overlay[row][col] = 3;
            map->collision[row][col] = 0;
        } else {
            /* Weak mode: walls get fixed HP and degrade (seg_1010:799-809).
             * collision SET to 500, then random: 50% → 'q', 50% → 'p' (HP=1000). */
            map->collision[row][col] = 500;
            if (mb_random(2) == 0) {
                map->tiles[row][col] = 'q';
            } else {
                map->tiles[row][col] = 'p';
                map->collision[row][col] = 1000;
            }
        }
    }
}

void bombs_update(TileMap *map)
{
    if (!map) return;

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint16_t overlay = map->overlay[row][col];

            if (overlay < 2) {
                if (overlay == 1) {
                    uint8_t t = map->tiles[row][col];
                    map->overlay[row][col] = 0;
                    /* Shop gate tiles (0xB4/0xB5) use overlay as cooldown,
                     * not as bomb fuse — skip detonation for them. */
                    if (t != 0xB4 && t != 0xB5) {
                        if (!bomb_check_treasure_drop(map, col, row)) {
                            bomb_detonate(map, col, row);
                        }
                    }
                }
                continue;
            }

            /* Decrement fuse */
            map->overlay[row][col] = overlay - 1;
            uint16_t new_overlay = overlay - 1;

            /* Visual stage transitions */
            uint8_t tile = map->tiles[row][col];
            switch (tile) {
            case BOMB_SMALL_1:  /* 'W' → 'w' at <60 */
                if (new_overlay < 60)
                    map->tiles[row][col] = BOMB_SMALL_2;
                break;
            case BOMB_SMALL_2:  /* 'w' → 'x' at <30 */
                if (new_overlay < 30)
                    map->tiles[row][col] = BOMB_SMALL_3;
                break;
            case BOMB_MEDIUM_1: /* 'X' → 0x8B at <60 */
                if (new_overlay < 60)
                    map->tiles[row][col] = BOMB_MEDIUM_2;
                break;
            case BOMB_MEDIUM_2: /* 0x8B → 0x8C at <30 */
                if (new_overlay < 30)
                    map->tiles[row][col] = BOMB_MEDIUM_3;
                break;
            case BOMB_LARGE_1:  /* 'Y' → 0x8D at <40 */
                if (new_overlay < 40)
                    map->tiles[row][col] = BOMB_LARGE_2;
                break;
            case BOMB_LARGE_2:  /* 0x8D → 0x8E at <20 */
                if (new_overlay < 20)
                    map->tiles[row][col] = BOMB_LARGE_3;
                break;
            case BOMB_MEGA_1:   /* 0x7F → 0xA3 (immediate on tick) */
                map->tiles[row][col] = BOMB_MEGA_2;
                break;
            case BOMB_MEGA_2:   /* 0xA3 → 0x7F (flash cycle) */
                map->tiles[row][col] = BOMB_MEGA_1;
                break;
            case BOMB_MINE_1:   /* 0x9D → 0x9E */
                map->tiles[row][col] = BOMB_MINE_2;
                break;
            case BOMB_MINE_2:   /* 0x9E → 0x9F */
                map->tiles[row][col] = BOMB_MINE_3;
                break;
            case BOMB_MINE_3:   /* 0x9F → 0x9D (cycle) */
                map->tiles[row][col] = BOMB_MINE_1;
                break;
            default:
                break;
            }
        }
    }
}

bool bomb_check_treasure_drop(TileMap *map, int col, int row)
{
    if (!map) return false;
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return false;

    uint8_t tile = map->tiles[row][col];

    /* Only late-stage bomb tiles can drop treasure */
    bool can_drop = (tile == BOMB_SMALL_3  ||   /* 'x' (0x78) */
                     tile == BOMB_MEDIUM_2 ||   /* 0x8B */
                     tile == BOMB_MEDIUM_3 ||   /* 0x8C */
                     tile == BOMB_LARGE_2);     /* 0x8D */

    if (!can_drop) return false;

    /* 1.1% chance: random(1000) < 11 */
    if ((mb_random(TREASURE_DROP_RANGE)) >= TREASURE_DROP_CHANCE) return false;

    /* Convert bomb to treasure sprite (detonated form) */
    switch (tile) {
    case BOMB_SMALL_3:  map->tiles[row][col] = BOMB_SMALL_DET;  break;  /* x → 0x7D */
    case BOMB_MEDIUM_2: map->tiles[row][col] = BOMB_MEDIUM_DET; break;  /* 0x8B → 0x7E */
    case BOMB_MEDIUM_3: map->tiles[row][col] = BOMB_RANDOM_DET; break;  /* 0x8C → 0xAA */
    case BOMB_LARGE_2:  map->tiles[row][col] = BOMB_LARGE_DET;  break;  /* 0x8D → 0x7C */
    }

    map->collision[row][col] = 0;
    return true;
}

/*
 * Play the appropriate explosion sound effect for a detonating bomb tile.
 * Mapping derived from decompiled FUN_1010_165b trigger_sound_effect calls.
 * Trigger #2=explos1, #3=explos2, #4=explos3, #5=explos4, #6=explos5,
 * #8=picaxe, #9=pikkupom, #10=urethan.
 */
static void bomb_play_detonation_sfx(uint8_t tile)
{
    switch (tile) {
    /* Small bombs / explosive: trigger #9 = pikkupom (seg_1010:1282) */
    case BOMB_SMALL_1: case BOMB_SMALL_2: case BOMB_SMALL_3:
    case 0x65: /* 'e' explosive */
    case 0x7D: /* '}' small detonated sprite */
        sfx_play(SFX_PIKKUPOM);
        break;

    /* Medium bombs + player sig A tiles: trigger #2 = explos1 (seg_1010:1411) */
    case BOMB_MEDIUM_1: case BOMB_MEDIUM_2: case BOMB_MEDIUM_3:
    case BOMB_SIG_P3_A:  /* 0x82 */
    case BOMB_SIG_P1_A:  /* 'c' */
    case BOMB_SIG_P4_A:  /* 'i' */
    case BOMB_SIG_P2_A:  /* 'g' */
    case 0xA0:           /* type A0 */
    case 0x7E:           /* '~' medium detonated sprite */
    case 0x8A:           /* type 8A (seg_1010:1336) */
        sfx_play(SFX_EXPLOS1);
        break;

    /* Large bombs + player sig B tiles: trigger #3 = explos2 (seg_1010:1578) */
    case BOMB_LARGE_1: case BOMB_LARGE_2: case BOMB_LARGE_3:
    case BOMB_SIG_P3_B:  /* 0x83 */
    case BOMB_SIG_P1_B:  /* 'd' */
    case BOMB_SIG_P2_B:  /* 'h' */
    case BOMB_SIG_P4_B:  /* 'j' */
    case 0x9C:           /* warp bomb */
    case BOMB_RANDOM_DET: /* 0xAA */
        sfx_play(SFX_EXPLOS2);
        break;

    /* Mines: trigger #4 = explos3, played 3 times (seg_1010:1241-1243) */
    case BOMB_MINE_1: case BOMB_MINE_2: case BOMB_MINE_3:
        sfx_play(SFX_EXPLOS3);
        break;

    /* Type 0x80: trigger #4 = explos3 (seg_1010:1339) */
    case 0x80:
        sfx_play(SFX_EXPLOS3);
        break;

    /* Directional arrows: trigger #5 = explos4 (seg_1010:2265) */
    case ARROW_DOWN: case ARROW_UP: case ARROW_RIGHT: case ARROW_LEFT:
        sfx_play(SFX_EXPLOS4);
        break;

    /* Mega bomb + remote bomb: trigger #6 = explos5 (seg_1010:1582, 1705) */
    case BOMB_MEGA_1: case BOMB_MEGA_2:
    case 0xA2:           /* remote bomb (seg_1010:1582) */
    case 0x7C:           /* '|' large detonated / mega variant */
        sfx_play(SFX_EXPLOS5);
        break;

    /* Timer bomb: trigger #10 = urethan (seg_1010:1105) */
    case BOMB_TIMER_1:
        sfx_play(SFX_URETHAN);
        break;

    /* Super bomb / urethane (0x81): trigger #10 = urethan (seg_1010:1850) */
    case 0x81:
        sfx_play(SFX_URETHAN);
        break;

    /* Teleporter bomb (0xA9): trigger #8 = picaxe (seg_1010:908) */
    case 0xA9:
        sfx_play(SFX_PICAXE);
        break;

    /* Power bomb scatter (0xA4): trigger #2 = explos1 (seg_1010:959) */
    case 0xA4:
        sfx_play(SFX_EXPLOS1);
        break;

    /* Proximity mine: same as small bomb */
    case 0x6F:
        sfx_play(SFX_PIKKUPOM);
        break;

    default:
        /* Unknown tile type - play a generic small explosion */
        sfx_play(SFX_EXPLOS1);
        break;
    }
}

/*
 * Check if any player is currently on the given tile.
 * Mirrors FUN_1010_98dd(0, ...) which returns non-zero if a player is present.
 * Used by directional arrows to decide if they can move into a tile.
 */
static bool is_player_on_tile(int col, int row)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g_players[i].dead || !g_players[i].active) continue;
        int prow = pixel_to_tile_row(g_players[i].x_pos + SPRITE_W / 2);
        int pcol = pixel_to_tile_col(g_players[i].y_pos + SPRITE_H / 2);
        if (pcol == col && prow == row) return true;
    }
    return false;
}

/*
 * Check if any player or entity is on the given tile and apply explosion damage.
 * Faithfully implements FUN_1010_98dd (seg_1010:5337-5473).
 *
 * damage=0: presence check only (arrow blocking). Kill → corpse tile 'f'.
 * damage>0: explosion damage. Kill → explosion death tile 0x85 + death sound.
 *
 * Player damage scaling (seg_1010:5378-5386): single-player subtracts raw
 * damage; multiplayer routes it through the Pascal real RTL
 * (FUN_1030_1545/1537/1549 = load, multiply, Trunc) — that multiplier is
 * the BOMB DAMAGE % option: health -= Trunc(damage * pct/100). Entities
 * always take raw damage (seg_1010:5442).
 */
bool bombs_check_player_damage(TileMap *map, int col, int row, uint8_t damage)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return false;

    bool found = false;

    /* Part 1: Check all players (seg_1010:5359-5420) */
    for (int i = 0; i < g_num_active_players && i < MAX_PLAYERS; i++) {
        if (g_players[i].dead) continue;

        /* Player position check: row from screen X, col from screen Y.
         * VGA convention: pixel_to_tile_row(x) → row, pixel_to_tile_col(y) → col. */
        int prow = pixel_to_tile_row(g_players[i].x_pos + SPRITE_W / 2);
        int pcol = pixel_to_tile_col(g_players[i].y_pos + SPRITE_H / 2);
        if (prow != row || pcol != col) continue;

        /* Apply damage to player health (seg_1010:5378-5386): raw in SP,
         * scaled by the BOMB DAMAGE % option in MP (see header comment). */
        int16_t dmg = (int16_t)damage;
        if (g_num_active_players >= 2) {
            dmg = (int16_t)(((int)damage * g_config.bomb_damage_pct) / 100);
        }
        g_players[i].health -= dmg;

        /* Check if player died from this damage */
        if (g_players[i].health < 1 && !g_players[i].dead) {
            /* Credit kill to bomb owner (seg_1010:5401-5410).
             * Original checks offsets +0xFF/+0x101 (far pointer to killer),
             * increments killer's kill counter at +0x20/+0x22.
             * In the port, bomb_owner tracks who placed the bomb. */
            uint8_t owner = map->bomb_owner[row][col];
            if (owner < MAX_PLAYERS && (int)owner != i) {
                g_players[owner].kills++;
            }

            /* Place death tile at death position (seg_1010:5394-5400).
             * damage==0 → 'f' (corpse), damage!=0 → 0x85 (explosion death) */
            if (damage == 0) {
                map->tiles[row][col] = 'f';
            } else {
                map->tiles[row][col] = TILE_EXPLOSION2; /* 0x85 */
                map->overlay[row][col] = 3;
            }

            /* Play death sound if damage > 0 (seg_1010:5411-5413).
             * trigger #12 = aargh.voc at 11000 Hz */
            if (damage != 0) {
                sfx_play(SFX_AARGH);
            }

            /* Mark dead (seg_1010:5415); match-stats deaths dword +0x20
             * also incremented here in the original (seg_1010:5401-5409). */
            g_players[i].dead = 1;
            g_players[i].active = 0;
            g_players[i].deaths++;
            g_players[i].match_stats[STAT_DEATHS] += 1;
        }

        found = true;
    }

    /* Part 2: Check entities (seg_1010:5422-5472).
     * Iterates linked list, checks position, subtracts damage from entity HP. */
    Entity *e = s_entity_head;
    while (e) {
        if (!e->dead) {
            int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
            int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
            if (erow == row && ecol == col) {
                found = true;
                e->health -= (int16_t)damage;
                if (e->health < 1) {
                    /* Entity death (seg_1010:5444-5467).
                     * Play death sound for entities too. */
                    if (damage != 0) {
                        sfx_play(SFX_AARGH);
                    }
                    entity_kill(e);
                    /* Place death marker tile */
                    if (damage == 0) {
                        map->tiles[row][col] = 'f';
                    } else {
                        map->tiles[row][col] = TILE_EXPLOSION2;
                        map->overlay[row][col] = 3;
                    }
                }
            }
        }
        e = e->next;
    }

    return found;
}

/*
 * Check if a tile is passable for arrow bomb movement.
 * From decompiled seg_1010:1037-1039: arrows can move into '0', 'f', 0xAF,
 * or another arrow tile of the same type.
 */
static bool arrow_tile_passable(uint8_t tile, uint8_t arrow_tile)
{
    return tile == '0' || tile == 'f' || tile == 0xAF || tile == arrow_tile;
}

/*
 * Try to move a directional arrow bomb one tile in its direction.
 * Decompiled ref: seg_1010:1036-1102 (FUN_1010_165b arrow cases).
 *
 * Arrow movement per direction (seg_1010:1037-1102; row = screen X):
 *   0xA5 RIGHT: dest row+1, dest overlay = 2
 *   0xA6 LEFT:  dest row-1, dest overlay = 1
 *   0xA7 DOWN:  dest col+1, dest overlay = 2
 *   0xA8 UP:    dest col-1, dest overlay = 1
 * (+movers get overlay 2, -movers 1: the bomb pass scans in increasing
 * tile order, so a tile that moved forward must skip one tick.)
 *
 * Returns true if the arrow moved (sets overlay at destination for continued movement).
 * Returns false if blocked — caller should detonate as small bomb at current position.
 */
static bool arrow_try_move(TileMap *map, int col, int row, uint8_t arrow_tile)
{
    int dest_col = col, dest_row = row;
    uint16_t dest_overlay;

    switch (arrow_tile) {
    case ARROW_RIGHT: dest_row = row + 1; dest_overlay = 2; break;  /* 0xA5: screen X+ */
    case ARROW_LEFT:  dest_row = row - 1; dest_overlay = 1; break;  /* 0xA6: screen X- */
    case ARROW_DOWN:  dest_col = col + 1; dest_overlay = 2; break;  /* 0xA7: screen Y+ */
    case ARROW_UP:    dest_col = col - 1; dest_overlay = 1; break;  /* 0xA8: screen Y- */
    default: return false;
    }

    /* Bounds check */
    if (dest_row < 0 || dest_row >= MAP_ROWS || dest_col < 0 || dest_col >= MAP_COLS)
        return false;

    uint8_t dest_tile = map->tiles[dest_row][dest_col];

    /* Check if destination is passable for arrows AND no player is there */
    if (arrow_tile_passable(dest_tile, arrow_tile) && !is_player_on_tile(dest_col, dest_row)) {
        /* Move arrow to destination */
        map->tiles[dest_row][dest_col] = arrow_tile;
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        map->overlay[row][col] = 0;
        map->overlay[dest_row][dest_col] = dest_overlay;
        /* Preserve bomb owner at destination */
        map->bomb_owner[dest_row][dest_col] = map->bomb_owner[row][col];
        map->bomb_owner[row][col] = 0xFF;
        return true;
    }

    return false;
}

/*
 * Player pushing a bomb (FUN_1000_5073 non-wall branch, seg_1000:3656-3706).
 * Reached from player_dig once digging has drained the bomb's collision
 * below 2: the bomb slides one tile in the push direction, carrying its
 * fuse overlay, if the destination is open floor ('0'/'f'/0xAF) with no
 * live player (seg_1000:3681-3685) or entity (FUN_1000_4946, seg_1000:3686)
 * on it. Collision at the destination resets to 0x18 (seg_1000:3697).
 * The original also gates on a Pascal set-membership test of the tile
 * (seg_1000:3680-3681) whose set constant isn't recoverable from the
 * decompilation; the wall/non-wall split in player_dig stands in for it.
 */
bool bomb_try_push(TileMap *map, int col, int row, uint8_t dir)
{
    int dest_col = col, dest_row = row;

    /* seg_1000:3660-3670; row = screen X, col = screen Y */
    switch (dir) {
    case DIR_RIGHT: dest_row = row + 1; break;
    case DIR_LEFT:  dest_row = row - 1; break;
    case DIR_UP:    dest_col = col - 1; break;
    case DIR_DOWN:  dest_col = col + 1; break;
    default: return false;
    }

    if (dest_row < 0 || dest_row >= MAP_ROWS || dest_col < 0 || dest_col >= MAP_COLS)
        return false;

    uint8_t dest_tile = map->tiles[dest_row][dest_col];
    if (dest_tile != '0' && dest_tile != 'f' && dest_tile != 0xAF)
        return false;
    if (is_player_on_tile(dest_col, dest_row))
        return false;
    for (Entity *e = s_entity_head; e; e = e->next) {
        if (e->dead) continue;
        int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
        int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
        if (erow == dest_row && ecol == dest_col) return false;
    }

    uint8_t tile = map->tiles[row][col];
    map->tiles[row][col] = '0';
    map->overlay[dest_row][dest_col] = map->overlay[row][col];
    map->overlay[row][col] = 0;
    /* FUN_1010_98dd(0, ...) presence check at the vacated tile (seg_1000:3693) */
    bombs_check_player_damage(map, col, row, 0);
    map->tiles[dest_row][dest_col] = tile;
    map->collision[dest_row][dest_col] = BOMB_COLLISION_PUSHED;
    /* Owner travels with the bomb (port-side tracking, as in arrow_try_move).
     * The original leaves the drained (<2, possibly negative) collision at
     * the source; the port zeroes it because collision 0 is what marks the
     * vacated floor walkable/placeable again. */
    map->bomb_owner[dest_row][dest_col] = map->bomb_owner[row][col];
    map->bomb_owner[row][col] = 0xFF;
    map->collision[row][col] = 0;
    return true;
}

/*
 * Mine cross-pattern detonation (seg_1010:1212-1246).
 * Creates a large cross/diamond explosion with palette flash and screen shake.
 * For each row offset (-12 to +12), picks a random horizontal extent,
 * then applies damage=0xFF to all tiles in that horizontal range.
 */
static void mine_detonate_cross(TileMap *map, int col, int row)
{
    /* Palette flash (set_palette_to_white, seg_1010:1213) */
    map->palette_flash = 2;

    /* Player/entity damage at mine center (seg_1010:1216) */
    bombs_check_player_damage(map, col, row, 0xFF);

    /* Clear origin tile */
    map->tiles[row][col] = '0';
    map->collision[row][col] = 0;
    map->overlay[row][col] = 0;
    map->bomb_owner[row][col] = 0xFF;

    /* Cross pattern: iterate row offsets from -12 to +12 (seg_1010:1217-1240).
     * The original picks a random horizontal extent per row. The extent narrows
     * toward the top and bottom, creating a diamond/cross shape. */
    for (int dr = -12; dr <= 12; dr++) {
        int tr = row + dr;
        if (tr < 0 || tr >= MAP_ROWS) continue;

        /* Random horizontal extent: diamond shape with random variation.
         * Max extent = 12 - |dr|, then pick random 0..max_extent. */
        int max_extent = 12 - abs(dr);
        int extent = (max_extent > 0) ? (mb_random((max_extent + 1))) : 0;

        for (int dc = -extent; dc <= extent; dc++) {
            int tc = col + dc;
            if (tc < 0 || tc >= MAP_COLS) continue;
            if (tr == row && tc == col) continue;  /* already handled center */

            apply_explosion_hit(map, tc, tr, true, 0xFF);
        }
    }

    /* Screen shake (seg_1010:1245) */
    map->screen_shake += 10;
}

/*
 * Urethane flood-fill (decompiled seg_1010:1104-1211 for 0xA1,
 * seg_1010:1849-1950 for 0x81).
 *
 * "Urethane" (aka "slime") is an expanding blocking substance produced by
 * timer bomb (0xA1) and super bomb (0x81). Manual: "First trap your opponent
 * in urethane." Sound effect: urethan.voc.
 *
 * Algorithm:
 * 1. Mark detonation center as 0x88 (urethane origin).
 * 2. Iterate up to max_iter times:
 *    a. Scan entire map for 0x88 tiles.
 *    b. For each 0x88, try to place 0x89 on adjacent passable tiles (up/down/left/right).
 *    c. Convert all 0x89 → 0x88 for next iteration.
 *    d. Stop when no new tiles were added.
 * 3. Final pass: convert all 0x88 → 0xA0 (urethane) with collision=400, overlay=250.
 *    Exception: detonation center becomes '0' (floor).
 *
 * final_tile: the tile to place after flood-fill completes.
 *   0xA0 for timer bomb (temporary, overlay=0xFA).
 *   0x9B for super bomb (permanent, overlay=0).
 * final_overlay: overlay value for the final tiles.
 */
static void urethane_flood_fill(TileMap *map, int col, int row, int max_iter,
                                uint8_t final_tile, uint16_t final_overlay)
{
    /* Mark center as urethane origin */
    map->tiles[row][col] = TILE_URETHANE_ORIGIN;

    int count = 1;
    int prev_count = 0;

    while (count < max_iter && prev_count != count) {
        prev_count = count;

        /* Scan for 0x88 tiles and expand to adjacent passable tiles */
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] != TILE_URETHANE_ORIGIN) continue;

                /* Try all 4 adjacent tiles */
                /* Down (row+1) */
                if (r + 1 < MAP_ROWS) {
                    uint8_t t = map->tiles[r + 1][c];
                    if (t == '0' || t == 'f' || t == 0xAF) {
                        map->tiles[r + 1][c] = TILE_URETHANE_EXPAND;
                        count++;
                    }
                }
                /* Up (row-1) */
                if (r - 1 >= 0) {
                    uint8_t t = map->tiles[r - 1][c];
                    if (t == '0' || t == 'f' || t == 0xAF) {
                        map->tiles[r - 1][c] = TILE_URETHANE_EXPAND;
                        count++;
                    }
                }
                /* Left (col-1) */
                if (c - 1 >= 0) {
                    uint8_t t = map->tiles[r][c - 1];
                    if (t == '0' || t == 'f' || t == 0xAF) {
                        map->tiles[r][c - 1] = TILE_URETHANE_EXPAND;
                        count++;
                    }
                }
                /* Right (col+1) */
                if (c + 1 < MAP_COLS) {
                    uint8_t t = map->tiles[r][c + 1];
                    if (t == '0' || t == 'f' || t == 0xAF) {
                        map->tiles[r][c + 1] = TILE_URETHANE_EXPAND;
                        count++;
                    }
                }
            }
        }

        /* Convert all 0x89 → 0x88 for next iteration */
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] == TILE_URETHANE_EXPAND) {
                    map->tiles[r][c] = TILE_URETHANE_ORIGIN;
                }
            }
        }
    }

    /* Final pass: convert all 0x88 → final_tile.
     * Detonation center becomes floor ('0'). */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map->tiles[r][c] != TILE_URETHANE_ORIGIN) continue;

            if (r == row && c == col) {
                /* Detonation center: clear to floor */
                map->tiles[r][c] = '0';
                map->overlay[r][c] = 0;
                map->collision[r][c] = 0;
            } else {
                map->tiles[r][c] = final_tile;
                map->collision[r][c] = BOMB_COLLISION_PROX;
                map->overlay[r][c] = final_overlay;
            }
        }
    }
}

/*
 * Check if a tile is a wall ('7'-'9' or 'A'-'F').
 * Used by remote bomb flood-fill to determine expansion targets.
 */
static bool is_wall_tile(uint8_t tile)
{
    return (tile >= '7' && tile <= '9') || (tile >= 'A' && tile <= 'F');
}

/*
 * Remote bomb (0xA2) flood-fill through walls with chain-detonation.
 * Decompiled ref: seg_1010:1581-1701 (FUN_1010_165b, param_1 == -0x5e).
 *
 * Algorithm:
 * 1. Mark center as 0x88 (same origin marker as urethane).
 * 2. Iterate up to 75 times:
 *    a. Scan entire map for 0x88 tiles.
 *    b. For each 0x88, check 4 adjacent tiles:
 *       - If adjacent is a wall tile ('7'-'9','A'-'F'):
 *         Random gate (50%): expand (place 0x89) or chain-detonate.
 *       - If adjacent is NOT a wall tile:
 *         Random gate (50%): chain-detonate (apply_explosion_hit) or skip.
 *    c. Convert all 0x89 → 0x88 for next iteration.
 *    d. Stop when no new tiles were added.
 * 3. Final pass: apply strong damage (param_2=1, param_3=10) at all 0x88 positions,
 *    converting them to fire. Detonation center becomes floor.
 *
 * The DS[0x163B] random gate in the original controls expand-vs-detonate.
 * Since we don't know the exact value, we approximate as 50%.
 */
static void remote_bomb_flood_fill(TileMap *map, int col, int row)
{
    /* Mark center as urethane origin (seg_1010:1583) */
    map->tiles[row][col] = TILE_URETHANE_ORIGIN;

    int count = 1;
    int prev_count = 0;

    while (count < REMOTE_BOMB_MAX_ITER && prev_count != count) {
        prev_count = count;

        /* Scan for 0x88 tiles and try to expand through walls */
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] != TILE_URETHANE_ORIGIN) continue;

                /* Check 4 adjacent directions */
                static const int dirs[4][2] = {
                    { 1,  0 }, /* row+1 (seg_1010:1597-1616) */
                    {-1,  0 }, /* row-1 (seg_1010:1618-1637) */
                    { 0, -1 }, /* col-1 (seg_1010:1639-1658) */
                    { 0,  1 }, /* col+1 (seg_1010:1660-1679) */
                };

                for (int d = 0; d < 4; d++) {
                    int nr = r + dirs[d][0];
                    int nc = c + dirs[d][1];
                    if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;

                    uint8_t adj = map->tiles[nr][nc];

                    /* Skip tiles that are already part of our expansion (0x88/0x89) */
                    if (adj == TILE_URETHANE_ORIGIN || adj == TILE_URETHANE_EXPAND) continue;

                    if (is_wall_tile(adj)) {
                        /* Wall tile: random gate decides expand vs chain-detonate.
                         * Original: if (DS[0x163B] & Random()) != 0 → chain-detonate
                         * else → expand (place 0x89). Approximated as 50%. */
                        if (mb_random(2) != 0) {
                            /* Chain-detonate the wall (strong hit) */
                            apply_explosion_hit(map, nc, nr, true, 10);
                        } else {
                            /* Expand into this wall position */
                            map->tiles[nr][nc] = TILE_URETHANE_EXPAND;
                            count++;
                        }
                    } else {
                        /* Non-wall tile: random gate for chain-detonation.
                         * Original: if (DS[0x163B] & Random()) != 0 → chain-detonate */
                        if (mb_random(2) != 0) {
                            apply_explosion_hit(map, nc, nr, true, 10);
                        }
                    }
                }
            }
        }

        /* Convert all 0x89 → 0x88 for next iteration (seg_1010:1686 implied) */
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] == TILE_URETHANE_EXPAND) {
                    map->tiles[r][c] = TILE_URETHANE_ORIGIN;
                }
            }
        }
    }

    /* Final pass: apply strong damage at all 0x88 positions (seg_1010:1686-1701).
     * FUN_1010_1326(&stack, 1, 10, col, row) = strong mode, player_damage=10.
     * In the original, FUN_1010_1326 is called at EVERY 0x88 tile including center,
     * which checks player damage and converts to fire. Center becomes '0' afterward. */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map->tiles[r][c] != TILE_URETHANE_ORIGIN) continue;

            /* Apply strong damage (FUN_1010_1326 param_2=1, param_3=10) */
            apply_explosion_hit(map, c, r, true, 10);
            bombs_check_player_damage(map, c, r, 10);
        }
    }

    /* Clear detonation center to floor after damage pass */
    if (map->tiles[row][col] == TILE_EXPLOSION) {
        map->tiles[row][col] = '0';
        map->overlay[row][col] = 0;
        map->collision[row][col] = 0;
    }
}

/*
 * Mega bomb random omnidirectional expansion (seg_1010:1704-1847).
 * Uses 'z' (0x7A) marker tiles with random probability gates in all 4 directions.
 * Expands up to 75 tiles. When blocked (tile not passable), randomly
 * chain-detonates the blocking tile. After expansion, applies strong damage
 * (via FUN_1010_1326 with param_2=1, param_3=0xDC=220) at each expanded position.
 *
 * Same passability check as directional rocket (rocket_tile_passable).
 */
static void mega_bomb_expand(TileMap *map, int col, int row)
{
    /* Place initial 'z' at bomb center (seg_1010:1706) */
    map->tiles[row][col] = TILE_ROCKET_EXPAND;  /* 0x7A = 'z' */

    int count = 1;
    int prev_count = 2;  /* local_9 = '\x02' — ensures first iteration runs */

    while (count < MEGA_BOMB_MAX_ITER && prev_count != count) {
        prev_count = count;

        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] != TILE_ROCKET_EXPAND) continue;

                /* Try 4 directions with random probability gates.
                 * Decompiled uses `(DS[0x163b] & Random()) != 0` as gate.
                 * When gate fails: expand. When gate passes: apply explosion.
                 * Approximated as 50% chance for each.
                 *
                 * When blocked, the original calls FUN_1010_165b on the blocking
                 * tile — which for walls applies damage/degradation, for '1' does
                 * player damage only. We use apply_explosion_hit which handles
                 * wall degradation and skips indestructible tiles. */

                /* row+1 (seg_1010:1720-1734) */
                if (r + 1 < MAP_ROWS) {
                    uint8_t t = map->tiles[r + 1][c];
                    if (rocket_tile_passable(t)) {
                        if (mb_random(2) == 0) {
                            map->tiles[r + 1][c] = 0x5A;
                            count++;
                        }
                    } else if (mb_random(2) != 0) {
                        apply_explosion_hit(map, c, r + 1, true, 0xDC);
                    }
                }

                /* row-1 (seg_1010:1746-1770) */
                if (r - 1 >= 0) {
                    uint8_t t = map->tiles[r - 1][c];
                    if (rocket_tile_passable(t)) {
                        if (mb_random(2) == 0) {
                            map->tiles[r - 1][c] = 0x5A;
                            count++;
                        }
                    } else if (mb_random(2) != 0) {
                        apply_explosion_hit(map, c, r - 1, true, 0xDC);
                    }
                }

                /* col-1 (seg_1010:1772-1796) */
                if (c - 1 >= 0) {
                    uint8_t t = map->tiles[r][c - 1];
                    if (rocket_tile_passable(t)) {
                        if (mb_random(2) == 0) {
                            map->tiles[r][c - 1] = 0x5A;
                            count++;
                        }
                    } else if (mb_random(2) != 0) {
                        apply_explosion_hit(map, c - 1, r, true, 0xDC);
                    }
                }

                /* col+1 (seg_1010:1798-1822) */
                if (c + 1 < MAP_COLS) {
                    uint8_t t = map->tiles[r][c + 1];
                    if (rocket_tile_passable(t)) {
                        if (mb_random(2) == 0) {
                            map->tiles[r][c + 1] = 0x5A;
                            count++;
                        }
                    } else if (mb_random(2) != 0) {
                        apply_explosion_hit(map, c + 1, r, true, 0xDC);
                    }
                }
            }
        }

        /* Convert all 'Z' (0x5A) → 'z' (0x7A) for next iteration */
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] == 0x5A)
                    map->tiles[r][c] = TILE_ROCKET_EXPAND;
            }
        }
    }

    /* Final pass: apply strong damage at each 'z' position,
     * then clear tile to floor (seg_1010:1829-1847).
     * FUN_1010_1326 called with param_2=1 (strong), param_3=0xDC (220). */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map->tiles[r][c] == TILE_ROCKET_EXPAND) {
                map->tiles[r][c] = '0';
                map->collision[r][c] = 0;
                map->overlay[r][c] = 0;
                apply_explosion_hit(map, c, r, true, 0xDC);
                bombs_check_player_damage(map, c, r, 0xDC);
            }
        }
    }
}

/*
 * Apply a power bomb scatter hit at a single tile.
 * Replicates FUN_1010_1326 behavior when called with param_2=0, param_3=0x54.
 * Chain-detonates any bombs at the target before applying damage.
 * Decompiled ref: seg_1010:754-823 (FUN_1010_1326)
 */
static void power_bomb_scatter_hit(TileMap *map, int col, int row)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return;

    /* Apply weak explosion hit (param_2=0, damage=0x54 for power bomb scatter).
     * No chain-detonation: DS[0x1326]=0 means the gate in FUN_1010_1326 always
     * fails, so scatter hits apply normal damage only. */
    apply_explosion_hit(map, col, row, false, 0x54);
}

/*
 * Power bomb (0xA4) scatter detonation.
 * Decompiled ref: seg_1010:955-1034.
 *
 * Algorithm:
 * 1. Place fire (0x84, overlay=3) at center + play sound
 * 2. Loop random(5)..14: for each iteration:
 *    a. Pick random position within ±10 tiles of center (bounds-checked)
 *    b. Apply scatter hit at that position
 *    c. Expand from that position in 12 directions (diamond pattern, radius 1-2)
 *    d. Apply scatter hit at each valid expansion position
 *    e. Play sound
 *
 * The 12 expansion offsets form a diamond at radius 1-2:
 *   Dir  1: (col-1, row)     Dir  7: (col, row+2)
 *   Dir  2: (col+1, row)     Dir  8: (col+1, row+1)
 *   Dir  3: (col, row-1)     Dir  9: (col+2, row)
 *   Dir  4: (col, row+1)     Dir 10: (col+1, row-1)
 *   Dir  5: (col-2, row)     Dir 11: (col, row-2)
 *   Dir  6: (col-1, row+1)   Dir 12: (col-1, row-1)
 */
static void power_bomb_scatter(TileMap *map, int col, int row)
{
    /* 12 expansion offsets (dc, dr) from decompiled local_14 cases 1-12 */
    static const int scatter_offsets[12][2] = {
        { -1,  0 },  /* dir 1:  col-1 */
        {  1,  0 },  /* dir 2:  col+1 */
        {  0, -1 },  /* dir 3:  row-1 */
        {  0,  1 },  /* dir 4:  row+1 */
        { -2,  0 },  /* dir 5:  col-2 */
        { -1,  1 },  /* dir 6:  col-1, row+1 */
        {  0,  2 },  /* dir 7:  row+2 */
        {  1,  1 },  /* dir 8:  col+1, row+1 */
        {  2,  0 },  /* dir 9:  col+2 */
        {  1, -1 },  /* dir 10: col+1, row-1 */
        {  0, -2 },  /* dir 11: row-2 */
        { -1, -1 },  /* dir 12: col-1, row-1 */
    };

    /* 1. Place fire at detonation center */
    map->tiles[row][col] = TILE_EXPLOSION;
    map->overlay[row][col] = 3;
    map->collision[row][col] = 0;
    map->bomb_owner[row][col] = 0xFF;

    /* Play sound (trigger #2 = explos1, seg_1010:959) */
    sfx_play(SFX_EXPLOS1);

    /* 2. Scatter loop: random(5) to 14 groups (seg_1010:960) */
    int start = mb_random(5);
    for (int g = start; g < 15; g++) {
        /* 2a. Pick random target within ±10 tiles, bounds-checked */
        int trow, tcol;
        int attempts = 0;
        do {
            trow = row + (mb_random(20)) - 10;
            tcol = col + (mb_random(20)) - 10;
            attempts++;
            if (attempts > 100) goto next_group;  /* safety: avoid infinite loop */
        } while (tcol < 1 || tcol > 43 || trow < 1 || trow > 62);

        /* 2b. Apply scatter hit at random target */
        power_bomb_scatter_hit(map, tcol, trow);

        /* 2c. Expand in 12 directions from the random target */
        for (int d = 0; d < 12; d++) {
            int ec = tcol + scatter_offsets[d][0];
            int er = trow + scatter_offsets[d][1];

            /* Bounds check (decompiled: col in [0,44], row in [0,63]) */
            if (ec >= 0 && ec < MAP_COLS && er >= 0 && er < MAP_ROWS) {
                power_bomb_scatter_hit(map, ec, er);
            }
        }

        next_group:
        /* 2d. Play sound after each group (seg_1010:1033) */
        sfx_play(SFX_EXPLOS1);
    }
}

void bomb_detonate(TileMap *map, int col, int row)
{
    if (!map) return;
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return;

    uint8_t tile = map->tiles[row][col];

    TraceLog(LOG_DEBUG, "BOMB_DETONATE: tile=0x%02X at row=%d col=%d overlay=%d collision=%d",
             tile, row, col, map->overlay[row][col], map->collision[row][col]);

    /* Explosion/fire tile visual decay chain (seg_1010:1958-1986).
     * These tiles are NOT bombs — they're the visual aftermath of explosions.
     * When their overlay reaches 1, they transition to the next decay stage,
     * NOT trigger a new explosion.
     *
     * Decay sequences:
     *   0x84 → 0x61 (overlay=3) → 0x62 (overlay=3) → '0' (floor, overlay=0)
     *   0x85 → 0x86 (overlay=3) → 0x87 (overlay=3) → 'f' (corpse, overlay=0)
     */
    if (tile == TILE_EXPLOSION) {  /* 0x84 */
        map->tiles[row][col] = TILE_ROCKET_FIRE;  /* 0x61 */
        map->overlay[row][col] = 3;
        return;
    }
    if (tile == TILE_ROCKET_FIRE) {  /* 0x61 'a' */
        map->tiles[row][col] = 0x62;  /* 'b' */
        map->overlay[row][col] = 3;
        return;
    }
    if (tile == 0x62) {  /* 'b' — final decay → floor */
        map->tiles[row][col] = '0';
        map->overlay[row][col] = 0;
        map->collision[row][col] = 0;
        return;
    }
    if (tile == TILE_EXPLOSION2) {  /* 0x85 */
        map->tiles[row][col] = 0x86;
        map->overlay[row][col] = 3;
        return;
    }
    if (tile == 0x86) {
        map->tiles[row][col] = 0x87;
        map->overlay[row][col] = 3;
        return;
    }
    if (tile == 0x87) {  /* final decay → corpse */
        map->tiles[row][col] = 'f';
        map->overlay[row][col] = 0;
        map->collision[row][col] = 0;
        return;
    }

    /* Directional arrows: try to MOVE instead of exploding (seg_1010:1036-1102).
     * If blocked, detonate as small bomb (0x57) at current position. */
    if (tile == ARROW_DOWN || tile == ARROW_UP ||
        tile == ARROW_RIGHT || tile == ARROW_LEFT) {
        /* Sound plays each tick the arrow moves */
        bomb_play_detonation_sfx(tile);
        if (!arrow_try_move(map, col, row, tile)) {
            /* Blocked: clear arrow, then detonate as small bomb (seg_1010:1050) */
            map->tiles[row][col] = BOMB_SMALL_1;
            bomb_detonate(map, col, row);
        }
        return;
    }

    /* Power bomb (0xA4): scatter projectiles in 12-direction pattern (seg_1010:955-1034) */
    if (tile == 0xA4) {
        power_bomb_scatter(map, col, row);
        return;
    }

    /* Teleporter bomb (0xA9): creates indestructible wall (seg_1010:905-909).
     * Despite the name, this bomb does NOT teleport — it places tile '1'
     * (indestructible wall) with collision=30000 at the bomb position. */
    if (tile == 0xA9) {
        bomb_play_detonation_sfx(tile);
        map->tiles[row][col] = '1';
        map->collision[row][col] = 30000;
        map->overlay[row][col] = 0;
        return;
    }

    /* Random bomb (0xAB): explode as random type, then bounce (seg_1010:911-953).
     * 1. Clear tile to '0'
     * 2. Detonate a random small/medium/large bomb (0x57+random(3)) at same position
     * 3. If collision > 1: find a nearby passable tile (±4, up to 7 attempts),
     *    place a new 0xAB there with collision-1 and random overlay (1..0xB4) */
    if (tile == 0xAB) {
        uint16_t coll = map->collision[row][col];

        /* Step 1: clear current tile */
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        map->overlay[row][col] = 0;
        map->bomb_owner[row][col] = 0xFF;

        /* Step 2: detonate random bomb type at this position (seg_1010:916-917) */
        uint8_t random_type = (uint8_t)(mb_random(3)) + BOMB_SMALL_1;
        map->tiles[row][col] = random_type;
        map->overlay[row][col] = 1;  /* immediate detonation */
        bomb_detonate(map, col, row);

        /* Step 3: bounce to nearby tile if collision > 1 (seg_1010:918-953) */
        if (coll > 1) {
            int dr = 0, dc = 0;
            int attempts;
            for (attempts = 0; attempts < 7; attempts++) {
                dr = (mb_random(8)) - 4;  /* random(-4..3) */
                dc = (mb_random(8)) - 4;
                int nr = row + dr, nc = col + dc;
                if (nr > 1 && nr < 0x3F && nc < 0x2C && nc > 1) {
                    uint8_t dest = map->tiles[nr][nc];
                    /* Passable tiles (seg_1010:930-935):
                     * '0', '2'-'4', '7'-'9', 'A'-'F', 0x84 */
                    if (dest == '0' ||
                        (dest > '1' && (dest < '5' ||
                         (dest > '6' && (dest < '9' + 1 ||
                          (dest > 'A' - 1 && (dest < 'G' || dest == 0x84))))))) {
                        break;
                    }
                }
            }
            if (attempts >= 7) {
                dr = 0;
                dc = 0;
            }
            int nr = row + dr, nc = col + dc;
            map->tiles[nr][nc] = 0xAB;
            map->collision[nr][nc] = coll - 1;
            if (dr != 0 || dc != 0) {
                map->collision[row][col] = 0;
            }
            map->overlay[nr][nc] = (uint16_t)(mb_random(0xB4)) + 1;
        }
        return;
    }

    /* Urethane final tiles: when overlay expires or detonated, convert to floor.
     * 0xA0 (timer bomb urethane): temporary, decays via overlay countdown.
     * 0x9B (super bomb urethane): permanent (overlay=0), only destroyed by explosions. */
    if (tile == TILE_URETHANE_FINAL || tile == TILE_URETHANE_PERM) {
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        map->overlay[row][col] = 0;
        return;
    }

    /* Remote bomb (0xA2): wall-penetrating flood-fill (seg_1010:1581-1701).
     * Expands through wall tiles, chain-detonates non-walls. Final pass
     * applies strong damage at all expanded positions. */
    if (tile == 0xA2) {
        bomb_play_detonation_sfx(tile);
        remote_bomb_flood_fill(map, col, row);
        return;
    }

    /* Urethane flood-fill (seg_1010:1104-1211 for 0xA1, 1849-1957 for 0x81).
     * Timer bomb creates temporary 0xA0 tiles (overlay=250, decays to floor).
     * Super bomb creates permanent 0x9B tiles (overlay=0, stays forever). */
    if (tile == BOMB_TIMER_1) {
        bomb_play_detonation_sfx(tile);
        urethane_flood_fill(map, col, row, URETHANE_MAX_ITER_A1,
                            TILE_URETHANE_FINAL, URETHANE_FINAL_OVERLAY);
        return;
    }
    if (tile == 0x81) {
        bomb_play_detonation_sfx(tile);
        urethane_flood_fill(map, col, row, URETHANE_MAX_ITER_81,
                            TILE_URETHANE_PERM, 0);
        return;
    }

    /* Proximity mine movement (seg_1010:2131-2166).
     * Instead of exploding, mines MOVE to an adjacent passable tile.
     * Direction is determined by (random(140)+1) % 4. */
    if (tile == 0x6F) {
        int fuse = (mb_random(140)) + 1;
        int dir = fuse % 4;
        int dr = 0, dc = 0;
        switch (dir) {
        case 0: dr =  1; break; /* row+1 */
        case 1: dr = -1; break; /* row-1 */
        case 2: dc =  1; break; /* col+1 */
        case 3: dc = -1; break; /* col-1 */
        }
        int nr = row + dr, nc = col + dc;
        if (nr >= 0 && nr < MAP_ROWS && nc >= 0 && nc < MAP_COLS) {
            uint8_t dest = map->tiles[nr][nc];
            if (dest == '0' || dest == 'f' || dest == 0xAF) {
                /* Spawn copy at destination */
                map->tiles[nr][nc] = 0x6F;
                map->overlay[nr][nc] = (uint16_t)fuse;
                map->collision[nr][nc] = BOMB_COLLISION_PROX;
            }
        }
        /* Original mine stays with new fuse (decompiled sets overlay at current pos too) */
        map->overlay[row][col] = (uint16_t)fuse;
        return;
    }

    /* Mine detonation: cross-pattern with palette flash (seg_1010:1212-1246) */
    if (tile == BOMB_MINE_1 || tile == BOMB_MINE_2 || tile == BOMB_MINE_3) {
        bomb_play_detonation_sfx(tile);
        mine_detonate_cross(map, col, row);
        return;
    }

    /* Mega bomb (0x7F/0xA3/0x7C): random omnidirectional expansion (seg_1010:1704-1847).
     * Uses 'z' marker tiles expanding in all directions with random probability
     * gates, then applies strong damage at every expanded position. */
    if (tile == BOMB_MEGA_1 || tile == BOMB_MEGA_2 || tile == BOMB_LARGE_DET) {
        bomb_play_detonation_sfx(tile);
        mega_bomb_expand(map, col, row);
        return;
    }

    /* Play explosion sound effect based on bomb type */
    bomb_play_detonation_sfx(tile);

    /*
     * Explosion patterns matching FUN_1010_165b (seg_1010:1247-1578).
     *
     * The original uses EXPLICIT cross/diamond coordinate lists, NOT square
     * radii. Each bomb type has a specific set of offsets extracted from
     * the decompiled direction cases (local_14 loop).
     *
     * Chain-detonation: FUN_1010_1326 (seg_1010:768) checks
     * (DS[0x1326] & Random()) != 0 before chain-detonating. Since DS[0x1326]
     * is never initialized (always 0), the AND result is always 0, meaning
     * chain-detonation through FUN_1010_1326 NEVER happens. Only explicit
     * code paths (mine cross, rocket beam, power bomb scatter) can trigger it.
     *
     * Player stats (digging_power + bonus_stat) are NOT used for explosion
     * damage — they only affect manual digging.
     */

    /* Small bomb cross pattern: 4 cardinal tiles (seg_1010:1253-1281).
     * Offsets are (row_delta, col_delta) in map array convention. */
    static const int small_offsets[][2] = {
        { 0, -1 }, /* col-1 */
        { 0,  1 }, /* col+1 */
        {-1,  0 }, /* row-1 */
        { 1,  0 }, /* row+1 */
    };

    /* Medium bomb diamond: 12 tiles (seg_1010:1349-1410).
     * Ring 1 (4 cardinal) + Ring 2 (8 at distance 2).
     * Verified 1:1 against decompiled CONCAT11 patterns. */
    static const int medium_offsets[][2] = {
        { 0, -1 }, { 0,  1 }, {-1,  0 }, { 1,  0 },   /* ring 1: cardinal */
        { 0, -2 }, { 1, -1 }, { 2,  0 }, { 1,  1 },   /* ring 2: clockwise */
        { 0,  2 }, {-1,  1 }, {-2,  0 }, {-1, -1 },    /* from top */
    };

    /* Large bomb diamond: 36 tiles (seg_1010:1420-1577).
     * Ring 1 (4) + Ring 2 (8) + Ring 3 (24).
     * Verified 1:1 against decompiled CONCAT11 patterns. */
    static const int large_offsets[][2] = {
        /* Ring 1: cardinal */
        { 0, -1 }, { 0,  1 }, {-1,  0 }, { 1,  0 },
        /* Ring 2: same as medium */
        { 0, -2 }, { 1, -1 }, { 2,  0 }, { 1,  1 },
        { 0,  2 }, {-1,  1 }, {-2,  0 }, {-1, -1 },
        /* Ring 3 (seg_1010:1470-1567): clockwise from top */
        { 0, -3 }, { 1, -3 }, { 1, -2 }, { 2, -2 },
        { 2, -1 }, { 3, -1 }, { 3,  0 }, { 3,  1 },
        { 2,  1 }, { 2,  2 }, { 1,  2 }, { 1,  3 },
        { 0,  3 }, {-1,  3 }, {-1,  2 }, {-2,  2 },
        {-2,  1 }, {-3,  1 }, {-3,  0 }, {-3, -1 },
        {-2, -1 }, {-2, -2 }, {-1, -2 }, {-1, -3 },
    };

    /* Determine pattern, damage mode, and player HP damage per bomb type */
    const int (*offsets)[2] = NULL;
    int num_offsets = 0;
    bool strong = false;
    uint8_t player_damage = 60;

    switch (tile) {
    case BOMB_SMALL_1: case BOMB_SMALL_2: case BOMB_SMALL_3:
    case 0x65: /* 'e' explosive */
    case 0x7D: /* small detonated */
        /* Small: cross, weak (seg_1010:1251,1277 → param_2=0, param_3=0x3C) */
        offsets = small_offsets;
        num_offsets = 4;
        strong = false;
        player_damage = 0x3C;  /* 60 */
        break;

    case BOMB_MEDIUM_1: case BOMB_MEDIUM_2: case BOMB_MEDIUM_3:
    case BOMB_SIG_P3_A: case BOMB_SIG_P1_A: case BOMB_SIG_P4_A: case BOMB_SIG_P2_A:
    case 0xA0: case 0x7E:
        /* Medium + sig-A: diamond 12, weak (seg_1010:1348,1406 → param_2=0, param_3=0x54) */
        offsets = medium_offsets;
        num_offsets = 12;
        strong = false;
        player_damage = 0x54;  /* 84 */
        break;

    case BOMB_LARGE_1: case BOMB_LARGE_2: case BOMB_LARGE_3:
    case BOMB_SIG_P3_B: case BOMB_SIG_P1_B: case BOMB_SIG_P2_B: case BOMB_SIG_P4_B:
    case 0x9C: case BOMB_RANDOM_DET:
        /* Large + sig-B: diamond 36, weak (seg_1010:1419,1573 → param_2=0, param_3=100) */
        offsets = large_offsets;
        num_offsets = 36;
        strong = false;
        player_damage = 100;
        break;

    /* BOMB_MEGA_1/BOMB_MEGA_2/BOMB_LARGE_DET handled above by mega_bomb_expand() */

    case 0x8A:
    case 0x80:
        /* Napalm (0x8A) and Cluster (0x80): directional beam pattern.
         * Extends in 4 cardinal directions until hitting walls.
         * 0x8A has max range 16; 0x80 has no max range.
         * Handled by beam_detonate() below. */
        break;

    default:
        offsets = small_offsets;
        num_offsets = 4;
        strong = false;
        player_damage = 0x3C;
        break;
    }

    /* Napalm (0x8A) and Cluster bomb (0x80): directional beam pattern
     * (seg_1010:1284-1340). Extends in 4 cardinal directions, stops at
     * walls ('1', 'k'-'l', 0xB4-0xB5). 0x8A limited to 16 tiles per arm.
     * 0x80 limited only by walls/bounds. */
    if (tile == 0x8A || tile == 0x80) {
        uint8_t beam_damage = (tile == 0x8A) ? 100 : 200;
        int max_dist = (tile == 0x8A) ? 16 : MAP_ROWS; /* 0x80 has no limit */

        bombs_check_player_damage(map, col, row, beam_damage);
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        map->overlay[row][col] = 0;
        map->bomb_owner[row][col] = 0xFF;

        /* 4 cardinal directions (seg_1010:1293-1333) */
        static const int beam_dirs[4][2] = {
            { 0,  1 }, /* col+ (DOWN on screen) */
            { 0, -1 }, /* col- (UP on screen) */
            { 1,  0 }, /* row+ (RIGHT on screen) */
            {-1,  0 }, /* row- (LEFT on screen) */
        };
        for (int d = 0; d < 4; d++) {
            for (int dist = 1; dist < max_dist; dist++) {
                int tr = row + beam_dirs[d][0] * dist;
                int tc = col + beam_dirs[d][1] * dist;
                if (tr < 0 || tr >= MAP_ROWS || tc < 0 || tc >= MAP_COLS) break;

                apply_explosion_hit(map, tc, tr, false, beam_damage);

                /* Stop at blocking tiles (seg_1010:1329-1331):
                 * '1' (indestructible), 'k'-'l' (exit/gate), 0xB4-0xB5 (switches) */
                uint8_t t = map->tiles[tr][tc];
                if (t == '1' || (t >= 'k' && t <= 'l') ||
                    (t >= 0xB4 && t <= 0xB5)) break;
            }
        }
        return;
    }

    /* Check player/entity damage at the bomb center before clearing it.
     * The original's FUN_1010_165b calls FUN_1010_98dd at the bomb position
     * for several bomb types (e.g. seg_1010:1251, 1348, 1419). */
    bombs_check_player_damage(map, col, row, player_damage);

    /* Clear the bomb tile itself */
    map->tiles[row][col] = '0';
    map->collision[row][col] = 0;
    map->overlay[row][col] = 0;
    map->bomb_owner[row][col] = 0xFF;

    /* Apply explosion at each offset position using the correct pattern.
     * No chain-detonation: DS[0x1326] is 0, so the original's random
     * chain-detonation gate in FUN_1010_1326 always fails. */
    for (int i = 0; i < num_offsets; i++) {
        int tr = row + offsets[i][0];
        int tc = col + offsets[i][1];
        if (tr < 0 || tr >= MAP_ROWS || tc < 0 || tc >= MAP_COLS) continue;

        apply_explosion_hit(map, tc, tr, strong, player_damage);
    }
}

/*
 * Check if a tile is passable for rocket weapons.
 * From decompiled FUN_1010_4c31 (seg_1010:2499-2501) and FUN_1010_45cd (2282-2285):
 *   '0' OR (0x60 < tile AND ((tile < 99 OR tile == 0x66 OR tile == 0x6f)
 *           OR (0x83 < tile AND ((tile < 0x88 OR tile == 0x9b OR tile == 0xaf)))))
 *
 * This accepts: '0', 'a'-'b' (0x61-0x62), 'f' (0x66), 'o' (0x6F),
 *               0x84-0x87, 0x9B, 0xAF.
 */
static bool rocket_tile_passable(uint8_t tile)
{
    if (tile == '0') return true;
    if (tile <= 0x60) return false;
    if (tile < 99) return true;      /* 0x61-0x62: 'a','b' */
    if (tile == 0x66) return true;   /* 'f' corpse */
    if (tile == 0x6F) return true;   /* 'o' proximity mine */
    if (tile <= 0x83) return false;
    if (tile < 0x88) return true;    /* 0x84-0x87: explosion tiles */
    if (tile == 0x9B) return true;
    if (tile == 0xAF) return true;
    return false;
}

/*
 * Process one tile in the rocket launcher beam path.
 * Faithfully ports FUN_1010_4c31 (seg_1010:2475-2538).
 *
 * Returns 1 if beam should continue, 0 if beam is blocked.
 *
 * If the tile is passable: places fire 'a' (0x61) with overlay=3.
 * If there's a bomb at the position: chain-detonate/neutralize it.
 * If blocked: beam stops.
 */
static int rocket_beam_tile(TileMap *map, int col, int row)
{
    /* Bounds check (seg_1010:2492) */
    if (row <= 0 || col <= 0 || row >= MAP_ROWS || col >= MAP_COLS) return 0;

    uint8_t tile = map->tiles[row][col];

    /* Check for arrow bombs (0xA5-0xA8) — neutralize them (seg_1010:2498) */
    if (tile >= ARROW_RIGHT && tile <= ARROW_UP) {
        /* Zero the overlay and set collision to 20 */
        map->overlay[row][col] = 0;
        map->collision[row][col] = BOMB_COLLISION_DEFAULT;
        return 1;
    }

    /* Passable tile → place fire (seg_1010:2501-2504) */
    if (rocket_tile_passable(tile)) {
        map->tiles[row][col] = TILE_ROCKET_FIRE;  /* 'a' (0x61) */
        map->overlay[row][col] = 3;
        /* Check player/entity damage (seg_1010:2302 equivalent — FUN_1010_98dd) */
        bombs_check_player_damage(map, col, row, 0x22);
        return 1;
    }

    /* Non-passable tile: check if it's a bomb we can chain-detonate (seg_1010:2511-2533) */
    /* Convert late-stage bombs to detonated sprites */
    if (tile == BOMB_LARGE_1 || tile == 0x8D || tile == 0x8E) {
        map->tiles[row][col] = 0xAA;
    } else if (tile == BOMB_MEDIUM_1 || tile == 0x8B || tile == 0x8C) {
        map->tiles[row][col] = 0x7E;
    } else if (tile == BOMB_SMALL_1 || tile == 'w' || tile == 'x') {
        map->tiles[row][col] = 0x7D;
    } else if (tile == BOMB_MEGA_1 || tile == 0xA3) {
        map->tiles[row][col] = 0x7C;
    }

    /* Beam stops at non-passable tiles */
    return 0;
}

void rocket_launcher_fire(TileMap *map, int col, int row, uint8_t direction)
{
    if (!map) return;

    for (int i = 1; i < 7; i++) {
        int tc = col, tr = row;
        switch (direction) {
        case DIR_DOWN:  tc = col + i; break;  /* screen Y+ → col+ */
        case DIR_UP:    tc = col - i; break;  /* screen Y- → col- */
        case DIR_LEFT:  tr = row - i; break;  /* screen X- → row- */
        case DIR_RIGHT: tr = row + i; break;  /* screen X+ → row+ */
        default: return;
        }
        if (!rocket_beam_tile(map, tc, tr)) break;
    }
}

void directional_rocket_fire(TileMap *map, int col, int row, uint8_t direction)
{
    if (!map) return;

    /* Play sound: trigger #5 = explos4 (seg_1010:2265) */
    sfx_play(SFX_EXPLOS4);

    /* 1. Offset start one tile in facing direction if passable (seg_1010:2268-2288).
     * VGA convention: DOWN/UP → col±, LEFT/RIGHT → row±. */
    int start_col = col, start_row = row;
    switch (direction) {
    case DIR_DOWN:
        if (start_col < MAP_COLS - 1 && rocket_tile_passable(map->tiles[start_row][start_col + 1]))
            start_col++;
        break;
    case DIR_UP:
        if (start_col > 0 && rocket_tile_passable(map->tiles[start_row][start_col - 1]))
            start_col--;
        break;
    case DIR_LEFT:
        if (start_row > 0 && rocket_tile_passable(map->tiles[start_row - 1][start_col]))
            start_row--;
        break;
    case DIR_RIGHT:
        if (start_row < MAP_ROWS - 1 && rocket_tile_passable(map->tiles[start_row + 1][start_col]))
            start_row++;
        break;
    }

    /* 2. Place initial 'z' tile (seg_1010:2289) */
    map->tiles[start_row][start_col] = TILE_ROCKET_EXPAND;

    /* 3. Iterative expansion (seg_1010:2290-2469).
     * Each iteration scans for 'z' tiles and places 'Z' (0x5A) at adjacent
     * passable tiles, with direction-biased probability.
     *
     * The bias works as follows (seg_1010:2327-2338):
     *   - Facing direction: always allowed (50% random gate)
     *   - Behind direction: never allowed
     *   - Perpendicular: allowed when dist_along * 2 < dist_across (50% random)
     *
     * FUN_1010_455f = abs(center_row - tile_row)
     * FUN_1010_4586 = abs(center_col - tile_col) */
    int count = 1;
    int prev_count;

    do {
        prev_count = count;
        if (count > 29) break;

        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] != TILE_ROCKET_EXPAND) continue;

                /* Try row+1 (screen X+ = RIGHT) */
                if (r + 1 < MAP_ROWS && rocket_tile_passable(map->tiles[r + 1][c])) {
                    bool allow = false;
                    if (direction == DIR_RIGHT)
                        allow = (mb_random(2) == 0);
                    else if (direction != DIR_LEFT) {
                        int d_along = abs((r + 1) - start_row);
                        int d_across = abs(c - start_col);
                        allow = (d_along * 2 < d_across) && (mb_random(2) == 0);
                    }
                    if (allow) { map->tiles[r + 1][c] = 0x5A; count++; }
                }

                /* Try row-1 (screen X- = LEFT) */
                if (r - 1 >= 0 && rocket_tile_passable(map->tiles[r - 1][c])) {
                    bool allow = false;
                    if (direction == DIR_LEFT)
                        allow = (mb_random(2) == 0);
                    else if (direction != DIR_RIGHT) {
                        int d_along = abs((r - 1) - start_row);
                        int d_across = abs(c - start_col);
                        allow = (d_along * 2 < d_across) && (mb_random(2) == 0);
                    }
                    if (allow) { map->tiles[r - 1][c] = 0x5A; count++; }
                }

                /* Try col-1 (screen Y- = UP) */
                if (c - 1 >= 0 && rocket_tile_passable(map->tiles[r][c - 1])) {
                    bool allow = false;
                    if (direction == DIR_UP)
                        allow = (mb_random(2) == 0);
                    else if (direction != DIR_DOWN) {
                        int d_across = abs(r - start_row);
                        int d_along = abs((c - 1) - start_col);
                        allow = (d_along * 2 < d_across) && (mb_random(2) == 0);
                    }
                    if (allow) { map->tiles[r][c - 1] = 0x5A; count++; }
                }

                /* Try col+1 (screen Y+ = DOWN) */
                if (c + 1 < MAP_COLS && rocket_tile_passable(map->tiles[r][c + 1])) {
                    bool allow = false;
                    if (direction == DIR_DOWN)
                        allow = (mb_random(2) == 0);
                    else if (direction != DIR_UP) {
                        int d_across = abs(r - start_row);
                        int d_along = abs((c + 1) - start_col);
                        allow = (d_along * 2 < d_across) && (mb_random(2) == 0);
                    }
                    if (allow) { map->tiles[r][c + 1] = 0x5A; count++; }
                }
            }
        }

        /* Convert all 'Z' (0x5A) → 'z' (0x7A) for next iteration (seg_1010:2455-2469) */
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                if (map->tiles[r][c] == 0x5A)
                    map->tiles[r][c] = TILE_ROCKET_EXPAND;
            }
        }
    } while (prev_count != count);

    /* 5. Final pass: 'z' → fire (0x84) with overlay=3, check player damage (seg_1010:2296-2313) */
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (map->tiles[r][c] == TILE_ROCKET_EXPAND) {
                map->tiles[r][c] = TILE_EXPLOSION;
                bombs_check_player_damage(map, c, r, 0x22);
                map->overlay[r][c] = 3;
            }
        }
    }
}

void bombs_remote_detonate(TileMap *map, const Player *p)
{
    if (!map || !p) return;

    uint8_t sig0 = p->bomb_sig[0];
    uint8_t sig1 = p->bomb_sig[1];

    /* No signature set = no remote bombs to detonate */
    if (sig0 == 0 && sig1 == 0) return;

    /* Scan entire map for tiles matching player's bomb signature */
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = map->tiles[row][col];
            if ((sig0 != 0 && tile == sig0) || (sig1 != 0 && tile == sig1)) {
                map->overlay[row][col] = 1;  /* detonate next frame */
            }
        }
    }
}
