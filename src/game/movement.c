#include "game/movement.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include "game/player_db.h"
#include "game/visibility.h"
#include "game/bombs.h"
#include "audio/sfx.h"
#include <raylib.h>
#include "util/prng.h"

bool tile_is_passable(uint8_t tile)
{
    return tile == '0' || tile == 'f' || tile == 0xAF;
}

/*
 * Movement-specific passability check. In the original (seg_1000:3898-4059),
 * movement only allows '0', 'f', 0xAF. However, pickups with collision=0
 * (treasures, stat gems, health, arrows) must also be walkable since the port
 * doesn't yet implement the original's adjacent-tile pickup mechanism.
 */
static bool move_passable(const TileMap *map, int row, int col)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return false;
    uint8_t tile = map->tiles[row][col];
    if (tile == '0' || tile == 'f' || tile == 0xAF) return true;
    return map->collision[row][col] == 0;
}

/*
 * Wall-sliding movement matching the original (seg_1000:3883-4059).
 *
 * COORDINATE CONVENTION (original VGA):
 *   row (first array index, 0-63)  → screen X (horizontal)
 *   col (second array index, 0-44) → screen Y (vertical) + MAP_Y_OFFSET
 *
 *   LEFT/RIGHT change x_pos → row changes  → check tiles[row±1][col]
 *   UP/DOWN    change y_pos → col changes  → check tiles[row][col±1]
 *
 * The player center must be aligned on the perpendicular axis (intra 4-5)
 * to enter the next tile. The perpendicular axis is always snapped to tile
 * center (intra = 5).
 */
/*
 * Wall set for the DIG ANIMATION check at the tail of each move_player
 * direction case (e.g. seg_1000:3927-3936 for LEFT): '7'-'9', 'A',
 * 'C'-'F', 'p', 'q', 0xAC-0xAE. Note this differs from both
 * tile_is_diggable_wall (degradation visuals) and tile_in_dig_wall_set
 * (dig vs push dispatch): it includes 'q' (0x71) and excludes 'B' (0x42).
 */
static bool tile_is_dig_anim_wall(uint8_t t)
{
    return (t >= 0x37 && t <= 0x39) || t == 0x41 ||
           (t >= 0x43 && t <= 0x46) ||
           t == 0x70 || t == 0x71 ||
           (t >= 0xAC && t <= 0xAE);
}

bool player_move(Player *p, TileMap *map)
{
    if (p->dead || p->direction == DIR_STOP) return false;

    /* Compute center position and tile/intra-tile offsets. */
    int cx = p->x_pos + SPRITE_W / 2;
    int cy = p->y_pos + SPRITE_H / 2;
    int tile_row = pixel_to_tile_row(cx);       /* first index, from X */
    int tile_col = pixel_to_tile_col(cy);       /* second index, from Y */
    int intra_x = cx - tile_to_pixel_x(tile_row);
    int intra_y = cy - tile_to_pixel_y(tile_col);

    bool primary_moved = false;

    switch (p->direction) {
    case DIR_UP: {
        /* UP: decrease y_pos → decrease col (second index) */
        bool at_bound = (cy < MAP_Y_OFFSET + 6);
        bool x_aligned = (intra_x >= 4 && intra_x <= 5);
        bool next_ok = !at_bound && x_aligned &&
                       move_passable(map, tile_row, tile_col - 1);

        if (next_ok) {
            p->y_pos--;
            primary_moved = true;
        } else if (!at_bound && x_aligned && intra_y > 5) {
            p->y_pos--;
            primary_moved = true;
        }
        /* Perpendicular snap: align X to tile center */
        if (intra_x != 5) {
            p->x_pos += (int16_t)(5 - intra_x);
        }
        if (intra_y == 5) {
            player_check_pickup(p, map, tile_row, tile_col - 1);
        }
        break;
    }
    case DIR_DOWN: {
        /* DOWN: increase y_pos → increase col (second index) */
        bool at_bound = (cy >= MAP_COLS * TILE_SIZE + MAP_Y_OFFSET - 5);
        bool x_aligned = (intra_x >= 4 && intra_x <= 5);
        bool next_ok = !at_bound && x_aligned &&
                       move_passable(map, tile_row, tile_col + 1);

        if (next_ok) {
            p->y_pos++;
            primary_moved = true;
        } else if (!at_bound && x_aligned && intra_y < 5) {
            p->y_pos++;
            primary_moved = true;
        }
        if (intra_x != 5) {
            p->x_pos += (int16_t)(5 - intra_x);
        }
        if (intra_y == 5) {
            player_check_pickup(p, map, tile_row, tile_col + 1);
        }
        break;
    }
    case DIR_LEFT: {
        /* LEFT: decrease x_pos → decrease row (first index) */
        bool at_bound = (cx < 6);
        bool y_aligned = (intra_y >= 4 && intra_y <= 5);
        bool next_ok = !at_bound && y_aligned &&
                       move_passable(map, tile_row - 1, tile_col);

        if (next_ok) {
            p->x_pos--;
            primary_moved = true;
        } else if (!at_bound && y_aligned && intra_x > 5) {
            p->x_pos--;
            primary_moved = true;
        }
        if (intra_y != 5) {
            p->y_pos += (int16_t)(5 - intra_y);
        }
        if (intra_x == 5) {
            player_check_pickup(p, map, tile_row - 1, tile_col);
        }
        break;
    }
    case DIR_RIGHT: {
        /* RIGHT: increase x_pos → increase row (first index) */
        bool at_bound = (cx >= MAP_ROWS * TILE_SIZE - 5);
        bool y_aligned = (intra_y >= 4 && intra_y <= 5);
        bool next_ok = !at_bound && y_aligned &&
                       move_passable(map, tile_row + 1, tile_col);

        if (next_ok) {
            p->x_pos++;
            primary_moved = true;
        } else if (!at_bound && y_aligned && intra_x < 5) {
            p->x_pos++;
            primary_moved = true;
        }
        if (intra_y != 5) {
            p->y_pos += (int16_t)(5 - intra_y);
        }
        if (intra_x == 5) {
            player_check_pickup(p, map, tile_row + 1, tile_col);
        }
        break;
    }
    }

    /* Dig-animation state — the tail of each original direction case checks
     * the tile AHEAD on the movement axis against the dig-anim wall set,
     * gated on being centered on that axis (intra == 5), using the
     * intra/tile values captured BEFORE this frame's movement. NOTE: facing
     * (+0xA6 / last_direction) is NOT updated here — the original does that
     * at the head of the weapon tick (process_weapons seg_1000:2602-2604),
     * which is what turns the sprite toward a wall it never moves into. */
    bool digging = false;
    if (p->direction == DIR_UP || p->direction == DIR_DOWN) {
        if (intra_y == 5) {
            int ahead = tile_col + (p->direction == DIR_DOWN ? 1 : -1);
            if (ahead >= 0 && ahead < MAP_COLS)
                digging = tile_is_dig_anim_wall(map->tiles[tile_row][ahead]);
        }
    } else {
        if (intra_x == 5) {
            int ahead = tile_row + (p->direction == DIR_RIGHT ? 1 : -1);
            if (ahead >= 0 && ahead < MAP_ROWS)
                digging = tile_is_dig_anim_wall(map->tiles[ahead][tile_col]);
        }
    }
    p->digging = digging ? 1 : 0;

    /* The animation counter advances EVERY frame a direction is held —
     * also while blocked/digging: animate_player_sprite (seg_1000:3770)
     * runs on every move_player call with direction != 0 and increments
     * +0xA2 at the end. While digging, the pickaxe clink plays once per
     * cycle at counter 16 (seg_1000:3855-3858: trigger #8 at
     * 11000+random(100) Hz). */
    if (p->digging && p->anim_frame == 16) {
        sfx_play(SFX_PICAXE);
    }
    p->anim_frame++;
    if (p->anim_frame >= 30)
        p->anim_frame = 0;

    return primary_moved;
}

/*
 * Check if a tile is a wall type eligible for degradation visuals.
 * Matches decompiled condition at seg_1000:3725-3726.
 */
static bool tile_is_diggable_wall(uint8_t t)
{
    /* '7'-'9' (0x37-0x39) or 'A'-'F' (0x41-0x46) or 'p' (0x70) or 0xAC-0xAE */
    if (t >= 0x37 && t <= 0x39) return true;
    if (t >= 0x41 && t <= 0x46) return true;
    if (t == 0x70) return true;
    if (t >= 0xAC && t <= 0xAE) return true;
    return false;
}

/*
 * Check if a tile is in "group 1": standard destructible walls '7'-'9' or 'A'.
 * These degrade to '5'/'6'. Others degrade to 'q'/'p'.
 * Matches decompiled check at seg_1000:3733, 3744.
 */
static bool tile_is_wall_group1(uint8_t t)
{
    return (t >= 0x37 && t <= 0x39) || t == 0x41;
}

/*
 * Wall-set classification from FUN_1000_5073 (seg_1000:3398-3404, inverted).
 * Wall-set tiles take the dig-damage path (degrade, then clear); anything
 * else that blocks — placed bombs — takes the push path instead. Set:
 * '1'-'9', 'A', 'C'-'F', 'o'-'q', 0x9B (permanent urethane), 0xA0
 * (urethane), 0xAC-0xAE (reinforced). Note the original deliberately omits
 * 'B' (0x42) — the pushable boulder tile, absent from v3.11 maps but still
 * supported by the engine.
 */
static bool tile_in_dig_wall_set(uint8_t t)
{
    return (t >= 0x31 && t <= 0x39) || t == 0x41 ||
           (t >= 0x43 && t <= 0x46) ||
           (t >= 0x6F && t <= 0x71) ||
           t == 0x9B || t == 0xA0 ||
           (t >= 0xAC && t <= 0xAE);
}

void player_dig(Player *p, TileMap *map)
{
    if (!p || !map || p->dead || p->direction == DIR_STOP) return;

    int cx = p->x_pos + SPRITE_W / 2;
    int cy = p->y_pos + SPRITE_H / 2;
    int tile_row = pixel_to_tile_row(cx);
    int tile_col = pixel_to_tile_col(cy);
    int intra_x = cx - tile_to_pixel_x(tile_row);
    int intra_y = cy - tile_to_pixel_y(tile_col);

    int dig_row = tile_row;
    int dig_col = tile_col;

    switch (p->direction) {
    case DIR_UP:
        if (intra_y != 5) return;
        dig_col = tile_col - 1;
        break;
    case DIR_DOWN:
        if (intra_y != 5) return;
        dig_col = tile_col + 1;
        break;
    case DIR_LEFT:
        if (intra_x != 5) return;
        dig_row = tile_row - 1;
        break;
    case DIR_RIGHT:
        if (intra_x != 5) return;
        dig_row = tile_row + 1;
        break;
    default:
        return;
    }

    if (dig_row < 0 || dig_row >= MAP_ROWS || dig_col < 0 || dig_col >= MAP_COLS)
        return;

    uint16_t hp = map->collision[dig_row][dig_col];

    if (hp == 0 || hp == 30000) return;

    /* Non-wall blockers — placed bombs — take the push path
     * (seg_1000:3656-3706): dig damage drains collision with no
     * degradation visuals and the tile is NEVER cleared; once collision
     * drops below 2, each further push attempt tries to slide the bomb one
     * tile. The drained value rests at 1, not 0: the original's signed
     * collision goes ≤0 and the push triggers at <2, but in the port
     * collision 0 means open floor (walkable via move_passable, placeable
     * by bomb_place), which must not happen while the bomb still sits
     * there. */
    if (!tile_in_dig_wall_set(map->tiles[dig_row][dig_col])) {
        if (hp < 2) {
            bomb_try_push(map, dig_col, dig_row, p->direction);
        } else {
            uint16_t push_damage = (uint16_t)(p->digging_power + p->bonus_stat);
            if (push_damage < 1) push_damage = 1;
            map->collision[dig_row][dig_col] =
                (push_damage >= hp) ? 1 : (uint16_t)(hp - push_damage);
        }
        return;
    }

    uint16_t damage = (uint16_t)(p->digging_power + p->bonus_stat);
    if (damage < 1) damage = 1;

    TraceLog(LOG_DEBUG, "DIG:   dig_power=%d bonus=%d => damage=%u",
             p->digging_power, p->bonus_stat, damage);

    if (damage >= hp) {
        map->collision[dig_row][dig_col] = 0;
        map->tiles[dig_row][dig_col] = '0';
        TraceLog(LOG_DEBUG, "DIG:   DESTROYED tile at (%d,%d)", dig_row, dig_col);
        return;
    }

    hp -= damage;
    map->collision[dig_row][dig_col] = hp;
    TraceLog(LOG_DEBUG, "DIG:   hp: %u -> %u", hp + damage, hp);

    uint8_t tile = map->tiles[dig_row][dig_col];
    if (!tile_is_diggable_wall(tile)) return;

    bool is_reinforced = (tile >= 0xAC && tile <= 0xAE);

    if (!is_reinforced) {
        if (hp < 500) {
            map->tiles[dig_row][dig_col] = tile_is_wall_group1(tile) ? '5' : 'q';
        } else if (hp < 1000) {
            bool is_original_wall = (tile >= 0x37 && tile <= 0x39) ||
                                    (tile >= 0x41 && tile <= 0x46);
            if (is_original_wall) {
                map->tiles[dig_row][dig_col] = tile_is_wall_group1(tile) ? '6' : 'p';
            }
        }
    } else {
        if (hp < 2001) {
            map->tiles[dig_row][dig_col] = 0xAE;
        } else if (hp < 4001) {
            map->tiles[dig_row][dig_col] = 0xAD;
        }
    }
}

bool player_check_pickup(Player *p, TileMap *map, int row, int col)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return false;

    uint8_t tile = map->tiles[row][col];
    int32_t cash_add = 0;

    /* Original FUN_1000_5073 head (seg_1000:3351-3362): when crossing a
     * tile center toward a passable tile, bump the tiles-walked stat
     * (match-stats dword 9) and, with darkness active, cast the player's
     * vision fan. This is the ONLY reveal trigger — there is no per-frame
     * or round-start reveal. Entities never reach this path
     * (their stats pointer is null in the original; the port keeps entity
     * movement separate). */
    if (tile == '0' || tile == 'f' || tile == 0xAF) {
        p->match_stats[STAT_TILES_WALKED] += 1;
        if (map->darkness_enabled) {
            visibility_reveal_player(map, p);
        }
    }

    switch (tile) {
    case 0x92: cash_add = 15;  break;
    case 0x93: cash_add = 25;  break;
    case 0x94: cash_add = 15;  break;
    case 0x95: cash_add = 10;  break;
    case 0x96: cash_add = 30;  break;
    case 0x97: cash_add = 35;  break;
    case 0x98: cash_add = 50;  break;
    case 0x99: cash_add = 65;  break;
    case 0x9A: cash_add = 100; break;
    case 's':  cash_add = 1000; break;  /* 0x73 */
    case 'm':
        sfx_play(SFX_PICAXE);
        p->health = p->max_health;
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    case 0xB3:
        if (g_num_active_players == 1) {
            p->lives++;
        }
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    case 0x8F:
        sfx_play(SFX_PICAXE);
        p->digging_power += 1;
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    case 0x90:
        sfx_play(SFX_PICAXE);
        p->digging_power += 3;
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    case 0x91:
        sfx_play(SFX_PICAXE);
        p->digging_power += 5;
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    case 0x9C: {
        int current_idx = 32000;
        int total = 0;

        for (int c = 0; c < MAP_COLS; c++) {
            for (int r = 0; r < MAP_ROWS; r++) {
                if (map->tiles[r][c] == 0x9C) {
                    if (r == row && c == col) {
                        current_idx = total;
                    }
                    total++;
                }
            }
        }

        if (total <= 1) return false;

        int target_idx = 0;
        for (int attempt = 0; attempt < 21; attempt++) {
            target_idx = mb_random(total);
            if (target_idx != current_idx) break;
        }

        int count = 0;
        int dest_row = 0, dest_col = 0;
        bool found = false;
        for (int c = 0; c < MAP_COLS && !found; c++) {
            for (int r = 0; r < MAP_ROWS && !found; r++) {
                if (map->tiles[r][c] == 0x9C) {
                    if (count == target_idx) {
                        dest_row = r;
                        dest_col = c;
                        found = true;
                    }
                    count++;
                }
            }
        }

        if (found) {
            p->x_pos = tile_to_pixel_x(dest_row);
            p->y_pos = tile_to_pixel_y(dest_col);
        }
        return true;
    }
    case 0xB4:
        if (map->overlay[row][col] < 2) {
            map_shop_toggle_open(map);
        }
        return false;
    case 0xB5:
        if (map->overlay[row][col] < 2) {
            map_shop_toggle_close(map);
        }
        return false;
    case 0x79: {
        static const int tier1[] = { 9, 15, 18, 3, 4 };
        static const int tier2[] = { 4, 5, 17, 19, 10, 20, 12, 0 };
        static const int tier3[] = { 0, 1, 2, -1, -1, 7, 14, 6, 11, 13, 16, 8, -1 };

        sfx_play(SFX_PICAXE);

        int tier = mb_random(5);
        int weapon_idx, qty;

        if (tier == 0) {
            weapon_idx = tier1[mb_random(5)];
            qty = 1 + mb_random(3);
        } else if (tier == 1) {
            weapon_idx = tier2[mb_random(8)];
            qty = 1 + mb_random(6);
        } else {
            weapon_idx = tier3[mb_random(13)];
            qty = 3 + mb_random(11);
        }

        if (weapon_idx >= 0 && weapon_idx < WEAPON_SLOTS) {
            p->weapons[weapon_idx] += qty;
        }

        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    }
    default:
        return false;
    }

    if (cash_add > 0) {
        sfx_play(SFX_KILI);
        /* Treasure credits this round's earnings (original +0xE6,
         * seg_1000:3503-3506), NOT the wallet — the wallet only changes at
         * round end (FUN_1000_a17c) and in the shop. The pickup handler
         * also counts the treasure in the match-stats block (+0x10,
         * seg_1000:3513-3517). */
        p->earned += cash_add;
        p->match_stats[STAT_TREASURES] += 1;
        map->tiles[row][col] = '0';
        map->collision[row][col] = 0;
        return true;
    }

    return false;
}
