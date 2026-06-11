#include "game/ai.h"
#include "game/bombs.h"
#include "game/movement.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include <stdlib.h>
#include <math.h>
#include "util/prng.h"

/*
 * Check if a tile byte is a collectible item (weapons, treasures).
 * Matches the original pathfind_target search targets.
 */
static bool is_item_tile(uint8_t tile)
{
    /* Weapons: 0x57-0x59 (W, X, Y) */
    if (tile >= 0x57 && tile <= 0x59) return true;
    /* Bomb stages: 0x77-0x78 (w, x) */
    if (tile >= 0x77 && tile <= 0x78) return true;
    /* Special weapons: 0x7F-0x81 */
    if (tile >= 0x7F && tile <= 0x81) return true;
    /* More weapons: 0x8A-0x8E */
    if (tile >= 0x8A && tile <= 0x8E) return true;
    /* Mines/specials: 0x9D-0xA9 */
    if (tile >= 0x9D && tile <= 0xA9) return true;
    /* Random bomb: 0xAB */
    if (tile == 0xAB) return true;
    return false;
}

/*
 * Check if a tile byte is a hazard (active bomb, ticking explosive).
 * Matches the original FUN_1000_8e28 search targets.
 */
static bool is_hazard_tile(uint8_t tile)
{
    if (tile == 0x73) return true;            /* 's' treasure/trap */
    if (tile >= 0x92 && tile <= 0x9A) return true; /* explosives */
    if (tile >= 0x8F && tile <= 0x91) return true; /* stat gems */
    return false;
}

/*
 * Expanding square spiral search pattern.
 * Searches outward from (center_col, center_row) in expanding square rings.
 * Returns first match found at closest radius.
 */
static AiSearchResult spiral_search(const TileMap *map,
                                     int center_col, int center_row,
                                     int radius,
                                     bool (*match_fn)(uint8_t))
{
    AiSearchResult result = { false, 0, 0 };

    for (int r = 1; r <= radius; r++) {
        /* Search the perimeter of the square at distance r */
        for (int d = -r; d <= r; d++) {
            /* Top edge */
            int col = center_col + d;
            int row = center_row - r;
            if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
                if (match_fn(map->tiles[row][col])) {
                    result.found = true;
                    result.tile_col = col;
                    result.tile_row = row;
                    return result;
                }
            }

            /* Bottom edge */
            row = center_row + r;
            if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
                if (match_fn(map->tiles[row][col])) {
                    result.found = true;
                    result.tile_col = col;
                    result.tile_row = row;
                    return result;
                }
            }
        }

        for (int d = -r + 1; d <= r - 1; d++) {
            /* Left edge */
            int col = center_col - r;
            int row = center_row + d;
            if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
                if (match_fn(map->tiles[row][col])) {
                    result.found = true;
                    result.tile_col = col;
                    result.tile_row = row;
                    return result;
                }
            }

            /* Right edge */
            col = center_col + r;
            if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
                if (match_fn(map->tiles[row][col])) {
                    result.found = true;
                    result.tile_col = col;
                    result.tile_row = row;
                    return result;
                }
            }
        }
    }

    return result;
}

AiSearchResult ai_find_item(const TileMap *map, int center_col, int center_row,
                             int radius)
{
    return spiral_search(map, center_col, center_row, radius, is_item_tile);
}

AiSearchResult ai_find_player(const Player players[], int num_players,
                               int center_col, int center_row, int radius,
                               uint8_t ignore_owner)
{
    AiSearchResult result = { false, 0, 0, -1 };
    int best_dist = radius * radius + 1;  /* squared distance for comparison */

    for (int i = 0; i < num_players; i++) {
        if (players[i].dead) continue;
        if ((uint8_t)i == ignore_owner) continue;

        int prow = pixel_to_tile_row(players[i].x_pos + SPRITE_W / 2);
        int pcol = pixel_to_tile_col(players[i].y_pos + SPRITE_H / 2);

        int dc = abs(pcol - center_col);
        int dr = abs(prow - center_row);

        /* Check within radius (Manhattan or Chebyshev - use Chebyshev like original) */
        if (dc <= radius && dr <= radius) {
            int dist = dc * dc + dr * dr;
            if (dist < best_dist) {
                best_dist = dist;
                result.found = true;
                result.tile_col = pcol;
                result.tile_row = prow;
                result.player_index = i;
            }
        }
    }

    return result;
}

AiSearchResult ai_find_hazard(const TileMap *map, int center_col, int center_row,
                               int radius)
{
    return spiral_search(map, center_col, center_row, radius, is_hazard_tile);
}

/*
 * Check if entity can move in the given direction from its current position.
 */
static bool can_move_dir(const Entity *e, uint8_t dir, const TileMap *map)
{
    int dx = 0, dy = 0;
    switch (dir) {
    case DIR_DOWN:  dy = 1;  break;
    case DIR_UP:    dy = -1; break;
    case DIR_LEFT:  dx = -1; break;
    case DIR_RIGHT: dx = 1;  break;
    default: return false;
    }

    int new_x = e->x_pos + dx;
    int new_y = e->y_pos + dy;

    /* Check corners in movement direction (same logic as entity_move).
     * VGA convention: row from screen X, col from screen Y. */
    if (dx > 0) {
        int row = pixel_to_tile_row(new_x + SPRITE_W - 1);
        if (!tile_is_passable(map_get_tile(map, row, pixel_to_tile_col(new_y)))) return false;
        if (!tile_is_passable(map_get_tile(map, row, pixel_to_tile_col(new_y + SPRITE_H - 1)))) return false;
    } else if (dx < 0) {
        int row = pixel_to_tile_row(new_x);
        if (!tile_is_passable(map_get_tile(map, row, pixel_to_tile_col(new_y)))) return false;
        if (!tile_is_passable(map_get_tile(map, row, pixel_to_tile_col(new_y + SPRITE_H - 1)))) return false;
    }

    if (dy > 0) {
        int col = pixel_to_tile_col(new_y + SPRITE_H - 1);
        if (!tile_is_passable(map_get_tile(map, pixel_to_tile_row(new_x), col))) return false;
        if (!tile_is_passable(map_get_tile(map, pixel_to_tile_row(new_x + SPRITE_W - 1), col))) return false;
    } else if (dy < 0) {
        int col = pixel_to_tile_col(new_y);
        if (!tile_is_passable(map_get_tile(map, pixel_to_tile_row(new_x), col))) return false;
        if (!tile_is_passable(map_get_tile(map, pixel_to_tile_row(new_x + SPRITE_W - 1), col))) return false;
    }

    return true;
}

bool ai_is_blocked(const Entity *e, const TileMap *map)
{
    if (!e || e->direction == DIR_STOP) return true;
    return !can_move_dir(e, e->direction, map);
}

void ai_move_toward(Entity *e, int target_col, int target_row,
                    const TileMap *map)
{
    if (!e || !map) return;

    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    int dc = target_col - ecol;
    int dr = target_row - erow;

    /* Determine primary and secondary axes.
     * VGA convention: row = screen X (LEFT/RIGHT), col = screen Y (UP/DOWN). */
    uint8_t primary_dir = DIR_STOP;
    uint8_t secondary_dir = DIR_STOP;

    if (abs(dr) >= abs(dc)) {
        /* Row (screen X) axis is primary */
        primary_dir = (dr > 0) ? DIR_RIGHT : (dr < 0) ? DIR_LEFT : DIR_STOP;
        secondary_dir = (dc > 0) ? DIR_DOWN : (dc < 0) ? DIR_UP : DIR_STOP;
    } else {
        /* Col (screen Y) axis is primary */
        primary_dir = (dc > 0) ? DIR_DOWN : (dc < 0) ? DIR_UP : DIR_STOP;
        secondary_dir = (dr > 0) ? DIR_RIGHT : (dr < 0) ? DIR_LEFT : DIR_STOP;
    }

    /* 3% random chance to swap axes (prevents deterministic loops) */
    if ((mb_random(100)) < AI_RANDOM_SWAP_CHANCE && secondary_dir != DIR_STOP) {
        uint8_t tmp = primary_dir;
        primary_dir = secondary_dir;
        secondary_dir = tmp;
    }

    /* Try primary, then secondary, then stop */
    if (primary_dir != DIR_STOP && can_move_dir(e, primary_dir, map)) {
        e->direction = primary_dir;
    } else if (secondary_dir != DIR_STOP && can_move_dir(e, secondary_dir, map)) {
        e->direction = secondary_dir;
    }
    /* If both blocked, keep current direction (random redirect handles it) */
}

void ai_move_away(Entity *e, int threat_col, int threat_row,
                  const TileMap *map)
{
    if (!e || !map) return;

    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    int dc = ecol - threat_col;  /* inverted: move AWAY */
    int dr = erow - threat_row;

    uint8_t primary_dir = DIR_STOP;
    uint8_t secondary_dir = DIR_STOP;

    if (abs(dr) >= abs(dc)) {
        primary_dir = (dr > 0) ? DIR_RIGHT : (dr < 0) ? DIR_LEFT : DIR_STOP;
        secondary_dir = (dc > 0) ? DIR_DOWN : (dc < 0) ? DIR_UP : DIR_STOP;
    } else {
        primary_dir = (dc > 0) ? DIR_DOWN : (dc < 0) ? DIR_UP : DIR_STOP;
        secondary_dir = (dr > 0) ? DIR_RIGHT : (dr < 0) ? DIR_LEFT : DIR_STOP;
    }

    if (primary_dir != DIR_STOP && can_move_dir(e, primary_dir, map)) {
        e->direction = primary_dir;
    } else if (secondary_dir != DIR_STOP && can_move_dir(e, secondary_dir, map)) {
        e->direction = secondary_dir;
    } else {
        /* Both directions blocked: pick random direction 1-4.
         * Original draws Random(4)+1, so the array must be in VALUE order
         * (1=RIGHT 2=LEFT 3=UP 4=DOWN) for the same draw → same direction. */
        static const uint8_t dirs[] = { DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN };
        e->direction = dirs[mb_random(4)];
    }
}

/*
 * Pick a random direction (1-4). Used for periodic random redirects
 * (seg_1000:5962: FUN_1030_19de(4) + 1 — value order 1=RIGHT..4=DOWN).
 */
static void ai_random_direction(Entity *e)
{
    static const uint8_t dirs[] = { DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN };
    e->direction = dirs[mb_random(4)];
}

/*
 * Count clear tiles in a direction from a position (decompiled FUN_1000_8af0).
 * Also checks for other entities (FUN_1000_894e) and owner player (FUN_1000_89c3)
 * in the path. Returns count of consecutive clear tiles (0 if ally/entity found).
 */
static int count_clear_tiles(const TileMap *map, int start_col, int start_row,
                              int dcol, int drow,
                              const Player players[], int num_players,
                              uint8_t owner_idx,
                              const Entity *entity_head, const Entity *self)
{
    int count = 0;
    int col = start_col;
    int row = start_row;

    for (int i = 0; i < 10; i++) {
        row += drow;
        col += dcol;

        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) break;

        uint8_t tile = map->tiles[row][col];
        if (tile != '0' && tile != 'f' && tile != 0xAF) break;

        count++;

        /* FUN_1000_894e: check if any OTHER entity is at this tile */
        for (const Entity *ent = entity_head; ent != NULL; ent = ent->next) {
            if (ent == self || ent->dead) continue;
            int erow = pixel_to_tile_row(ent->x_pos + SPRITE_W / 2);
            int ecol = pixel_to_tile_col(ent->y_pos + SPRITE_H / 2);
            if (ecol == col && erow == row) {
                return 0;  /* friendly entity in blast path */
            }
        }

        /* FUN_1000_89c3: check if owner player is at this tile */
        if (owner_idx < (uint8_t)num_players && !players[owner_idx].dead) {
            int prow = pixel_to_tile_row(players[owner_idx].x_pos + SPRITE_W / 2);
            int pcol = pixel_to_tile_col(players[owner_idx].y_pos + SPRITE_H / 2);
            if (pcol == col && prow == row) {
                return 0;  /* owner in blast path */
            }
        }
    }

    return count;
}

bool ai_should_place_bomb(const TileMap *map, int x_pos, int y_pos,
                          uint8_t direction, const Player players[],
                          int num_players, uint8_t owner_idx,
                          const Entity *entity_head, const Entity *self)
{
    int row = pixel_to_tile_row(x_pos + SPRITE_W / 2);
    int col = pixel_to_tile_col(y_pos + SPRITE_H / 2);
    int dcol = 0, drow = 0;

    switch (direction) {
    case DIR_DOWN:  dcol = 1;  break;  /* screen Y+ → col+ */
    case DIR_UP:    dcol = -1; break;  /* screen Y- → col- */
    case DIR_LEFT:  drow = -1; break;  /* screen X- → row- */
    case DIR_RIGHT: drow = 1;  break;  /* screen X+ → row+ */
    default: return false;
    }

    int clear = count_clear_tiles(map, col, row, dcol, drow,
                                   players, num_players, owner_idx,
                                   entity_head, self);
    return clear >= AI_BOMB_CLEAR_TILES;
}

/*
 * Place a directional arrow bomb at the entity's tile position.
 * FUN_1000_88ba: writes arrow tile based on entity direction, sets overlay=1.
 * entity_interaction (seg_1000:5658-5703): XOR check per player — only places
 * if a non-owner player shares row OR column but not both (not on same tile).
 */
void ai_entity_place_bomb(Entity *e, TileMap *map,
                          const Player players[], int num_players,
                          const Entity *entity_head)
{
    if (!e || !map || e->dead || !e->active) return;
    if (e->direction == DIR_STOP) return;

    /* Safety check: 5+ clear tiles ahead, no allies/entities in path */
    if (!ai_should_place_bomb(map, e->x_pos, e->y_pos, e->direction,
                               players, num_players, e->owner_player,
                               entity_head, e)) {
        return;
    }

    /* Entity tile position (matching original: offset 0xEE / 0xF0) */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
    if (erow < 0 || erow >= MAP_ROWS || ecol < 0 || ecol >= MAP_COLS) return;

    /* XOR check per player (entity_interaction, seg_1000:5679):
     * Only place bomb if at least one non-owner player shares one axis
     * (same row OR same column) but is NOT on the exact same tile. */
    bool should_place = false;
    for (int i = 0; i < num_players; i++) {
        if (players[i].dead) continue;
        if ((uint8_t)i == e->owner_player) continue;

        int prow = pixel_to_tile_row(players[i].x_pos + SPRITE_W / 2);
        int pcol = pixel_to_tile_col(players[i].y_pos + SPRITE_H / 2);

        bool same_col = (pcol == ecol);
        bool same_row = (prow == erow);

        /* XOR: one axis matches but not both (not on same tile, not on neither axis) */
        if (same_col != same_row) {
            should_place = true;
            break;
        }
    }

    if (!should_place) return;

    /* FUN_1000_88ba: write directional arrow tile + overlay=1 */
    uint8_t arrow;
    switch (e->direction) {
    case DIR_DOWN:  arrow = ARROW_DOWN;  break;
    case DIR_UP:    arrow = ARROW_UP;    break;
    case DIR_LEFT:  arrow = ARROW_LEFT;  break;
    case DIR_RIGHT: arrow = ARROW_RIGHT; break;
    default: return;
    }

    map->tiles[erow][ecol] = arrow;
    map->overlay[erow][ecol] = 1;  /* immediate detonation next frame */
}

void ai_update(Entity *e, TileMap *map,
               const Player players[], int num_players,
               int frame_counter, Entity *entity_head,
               int treasure_count)
{
    if (!e || e->dead || !e->active) return;

    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    /* Random direction changes */
    if (frame_counter % AI_RANDOM_TICK2 == 0) {
        /* Unconditional random redirect every 121 frames */
        ai_random_direction(e);
        return;
    }
    if (frame_counter % AI_RANDOM_TICK == 0) {
        /* Redirect if blocked every 33 frames */
        if (ai_is_blocked(e, map)) {
            ai_random_direction(e);
        }
        return;
    }

    /* Main AI decision every 26 frames */
    if (frame_counter % AI_DECISION_TICK != 0) return;

    /* 1. Search for collectible items (radius 5) */
    AiSearchResult item = ai_find_item(map, ecol, erow, AI_ITEM_RADIUS);
    if (item.found) {
        ai_move_toward(e, item.tile_col, item.tile_row, map);
        return;
    }

    /* 2. Search for players (radius 10) — includes owner */
    AiSearchResult player = ai_find_player(players, num_players,
                                            ecol, erow, AI_PLAYER_RADIUS,
                                            0xFF); /* don't ignore any player */

    /* Original logic (seg_1000:5915-5934):
     * - If no items found, search for players via pathfind_alt
     * - If found player IS the owner (bVar2 == owner) OR no player found (bVar2 == 0):
     *     → search hazards (only if treasures remain) → flee or idle
     * - If found player is an ENEMY (different from owner, non-zero):
     *     → move toward enemy + try to place bomb (NO hazard check)
     */
    bool found_enemy = player.found &&
                       (player.player_index != (int)e->owner_player);

    if (found_enemy) {
        /* Found an enemy player — attack directly, skip hazard search */
        ai_move_toward(e, player.tile_col, player.tile_row, map);
        ai_entity_place_bomb(e, map, players, num_players, entity_head);
    } else {
        /* Found owner or no player — check hazards if treasures remain */
        if (treasure_count > 0) {
            AiSearchResult hazard = ai_find_hazard(map, ecol, erow,
                                                    AI_HAZARD_RADIUS);
            if (hazard.found) {
                ai_move_away(e, hazard.tile_col, hazard.tile_row, map);
                return;
            }
        }
        /* No hazard found (or no treasures) — try to place bomb */
        ai_entity_place_bomb(e, map, players, num_players, entity_head);
    }
}
