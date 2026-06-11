#include "game/entity.h"
#include "game/movement.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include "audio/sfx.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Names for the four monster types (from decompiled data) */
static const char *ENTITY_NAMES[ENTITY_TYPE_COUNT] = {
    "KarvaMies", "Creature", "Creature", "Alien"
};

/*
 * Decode spawn tile 'G'-'V' into type and initial direction.
 * Within each group of 4 (from decompiled seg_1000:4626-4637):
 *   offset 0 → direction 1 (DIR_RIGHT): G/K/O/S
 *   offset 1 → direction 2 (DIR_LEFT):  H/L/P/T
 *   offset 2 → direction 3 (DIR_UP):    I/M/Q/U
 *   offset 3 → direction 4 (DIR_DOWN):  J/N/R/V
 * (The values were always 1-4, but an earlier
 * DIR_* relabel made variant-0 monsters walk down instead of right.)
 */
static bool decode_spawn_tile(uint8_t tile, uint8_t *out_type, uint8_t *out_dir)
{
    if (tile < 'G' || tile > 'V') return false;

    int index = tile - 'G';          /* 0-15 */
    *out_type = (uint8_t)(index / 4); /* 0-3 */

    /* Direction within the group of 4 (from decompiled seg_1000:4626-4637) */
    switch (index % 4) {
    case 0: *out_dir = DIR_RIGHT; break;  /* G/K/O/S */
    case 1: *out_dir = DIR_LEFT;  break;  /* H/L/P/T */
    case 2: *out_dir = DIR_UP;    break;  /* I/M/Q/U */
    case 3: *out_dir = DIR_DOWN;  break;  /* J/N/R/V */
    }
    return true;
}

static int live_allocs;

static Entity *entity_alloc(void)
{
    Entity *e = calloc(1, sizeof(Entity));
    if (e) live_allocs++;
    return e;
}

Entity *entity_spawn(uint8_t spawn_tile, int tile_col, int tile_row)
{
    uint8_t type, dir;
    if (!decode_spawn_tile(spawn_tile, &type, &dir)) return NULL;

    Entity *e = entity_alloc();
    if (!e) return NULL;

    strncpy(e->name, ENTITY_NAMES[type], sizeof(e->name) - 1);
    e->type = type;
    e->health = ENTITY_HEALTH[type];
    e->max_health = ENTITY_HEALTH[type];
    e->attack_power = ENTITY_ATTACK[type];
    e->dead = 0;
    e->active = 0;  /* starts dormant */

    /* Position: top-left of tile (row→X, col→Y per original VGA convention).
     * Original stores center (+5) but draws at pos-5; port skips the offset. */
    e->x_pos = (int16_t)(tile_row * TILE_SIZE);
    e->y_pos = (int16_t)(tile_col * TILE_SIZE + MAP_Y_OFFSET);

    e->direction = dir;
    e->prev_direction = dir;
    e->speed_divisor = ENTITY_SPEED[type];

    e->anim_state = 0;
    e->owner_player = 0xFF; /* no owner (map-spawned) */
    e->next = NULL;

    return e;
}

Entity *entity_spawn_creature(int owner, int tile_col, int tile_row)
{
    Entity *e = entity_alloc();
    if (!e) return NULL;

    /* Creature spawner (weapon 'n', FUN_1000_3b40) copies the bot template
     * at 0x1038:0x2226: attack=1, health=100, speed=100.
     * Values set at seg_1010:5630/5633/5634 (DAT_1038_232e/2241/2243). */
    strncpy(e->name, "Creature", sizeof(e->name) - 1);
    e->type = ENTITY_TYPE_2;
    e->health = 100;
    e->max_health = 100;
    e->attack_power = 1;
    e->dead = 0;
    e->active = 1;  /* player-spawned creatures start active */

    e->x_pos = (int16_t)(tile_row * TILE_SIZE);
    e->y_pos = (int16_t)(tile_col * TILE_SIZE + MAP_Y_OFFSET);

    e->direction = DIR_DOWN;
    e->prev_direction = DIR_DOWN;
    e->speed_divisor = 100;

    e->anim_state = 0;
    e->owner_player = (uint8_t)owner;
    e->next = NULL;

    return e;
}

void entity_kill(Entity *e)
{
    if (e) e->dead = 1;
}

bool entity_move(Entity *e, const TileMap *map)
{
    if (!e || e->dead || e->direction == DIR_STOP) return false;

    int dx = 0, dy = 0;
    switch (e->direction) {
    case DIR_DOWN:  dy = 1;  break;
    case DIR_UP:    dy = -1; break;
    case DIR_LEFT:  dx = -1; break;
    case DIR_RIGHT: dx = 1;  break;
    }

    int new_x = e->x_pos + dx;
    int new_y = e->y_pos + dy;

    /* Check target tile for collision (entity is ~10x10 sprite).
     * VGA convention: row from screen X, col from screen Y. */
    int check_col, check_row;

    if (dx > 0) {
        check_row = pixel_to_tile_row(new_x + SPRITE_W - 1);
        check_col = pixel_to_tile_col(new_y);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
        check_col = pixel_to_tile_col(new_y + SPRITE_H - 1);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
    } else if (dx < 0) {
        check_row = pixel_to_tile_row(new_x);
        check_col = pixel_to_tile_col(new_y);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
        check_col = pixel_to_tile_col(new_y + SPRITE_H - 1);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
    }

    if (dy > 0) {
        check_col = pixel_to_tile_col(new_y + SPRITE_H - 1);
        check_row = pixel_to_tile_row(new_x);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
        check_row = pixel_to_tile_row(new_x + SPRITE_W - 1);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
    } else if (dy < 0) {
        check_col = pixel_to_tile_col(new_y);
        check_row = pixel_to_tile_row(new_x);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
        check_row = pixel_to_tile_row(new_x + SPRITE_W - 1);
        if (!tile_is_passable(map_get_tile(map, check_row, check_col))) return false;
    }

    e->x_pos = (int16_t)new_x;
    e->y_pos = (int16_t)new_y;
    e->prev_direction = e->direction;
    e->anim_state = (e->anim_state + 1) % 8;

    return true;
}

/*
 * Check if all tiles in a rectangular area between two tile positions
 * are passable (floor/corpse/item). Used for line-of-sight activation.
 * Matching decompiled player_collision_check (seg_1000:4782-4912):
 * scans all tiles in the bounding rectangle between entity and player,
 * returns true only if all are passable ('0', 'f', 0xAF).
 */
static bool rect_all_passable(const TileMap *map, int r1, int c1, int r2, int c2)
{
    int rmin = r1 < r2 ? r1 : r2;
    int rmax = r1 > r2 ? r1 : r2;
    int cmin = c1 < c2 ? c1 : c2;
    int cmax = c1 > c2 ? c1 : c2;

    for (int r = rmin; r <= rmax; r++) {
        for (int c = cmin; c <= cmax; c++) {
            if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) return false;
            uint8_t tile = map->tiles[r][c];
            if (tile != '0' && tile != 'f' && tile != 0xAF) return false;
        }
    }
    return true;
}

/*
 * Check if a player falls within the entity's directional fan (7-tile
 * expanding cone in facing direction). Matching decompiled
 * player_collision_check (seg_1000:4914-4977): for each ring distance
 * 1..7, the fan spans -(ring-1)..+(ring-1) tiles on the perpendicular
 * axis. Direction determines which axis is primary (depth) and which
 * is secondary (width).
 */
static bool check_directional_fan_activation(const Entity *e, int p_row, int p_col)
{
    int e_row = pixel_to_tile_row(e->x_pos);
    int e_col = pixel_to_tile_col(e->y_pos);

    for (int ring = 1; ring < 8; ring++) {
        int spread = ring - 1;
        for (int offset = -spread; offset <= spread; offset++) {
            int check_row, check_col;
            switch (e->direction) {
            case DIR_RIGHT: /* +row, offset on col */
                check_row = e_row + ring;
                check_col = e_col + offset;
                break;
            case DIR_LEFT: /* -row, offset on col */
                check_row = e_row - ring;
                check_col = e_col + offset;
                break;
            case DIR_DOWN: /* +col, offset on row */
                check_row = e_row + offset;
                check_col = e_col + ring;
                break;
            case DIR_UP: /* -col, offset on row */
                check_row = e_row + offset;
                check_col = e_col - ring;
                break;
            default:
                continue;
            }
            if (check_row == p_row && check_col == p_col) {
                return true;
            }
        }
    }
    return false;
}

void entities_update(Entity *head, const TileMap *map,
                     const Player players[], int num_players,
                     int frame_counter)
{
    for (Entity *e = head; e != NULL; e = e->next) {
        if (e->dead || !e->active) continue;

        /* Movement throttled by speed_divisor:
         * Entity moves when (frame_counter % speed_divisor != 0).
         * Speed 6 → moves 5 of every 6 frames.
         * Speed 100 → moves 99 of every 100 frames.
         * Decompiled ref: monster_player_collision (seg_1000:5901-5903) */
        if (e->speed_divisor > 0 && (frame_counter % e->speed_divisor) != 0) {
            entity_move(e, map);
        }
    }
}

/*
 * Activate dormant entities that detect nearby players.
 * Called every 5 frames, matching decompiled player_collision_check
 * (seg_1000:4699-4996, called at main loop lines 7253-7259).
 *
 * Three detection methods (from decompiled code):
 * 1. Same-tile proximity: player within 20px on both axes → activate
 * 2. Rectangular LOS: if entity shares exactly one axis (row XOR col)
 *    with a player, scan all tiles in the rectangle between them. If
 *    all passable → activate.
 * 3. Directional fan: 7-tile expanding cone in entity's facing direction.
 *    If player found within the cone → activate.
 *
 * Plays SFX_KARJAISU (trigger #11) on activation.
 */
void entities_activate(Entity *head, const Player players[], int num_players,
                       const TileMap *map)
{
    for (Entity *e = head; e != NULL; e = e->next) {
        if (e->dead || e->active) continue;

        int e_row = pixel_to_tile_row(e->x_pos);
        int e_col = pixel_to_tile_col(e->y_pos);

        for (int i = 0; i < num_players; i++) {
            if (players[i].dead) continue;

            int p_row = pixel_to_tile_row(players[i].x_pos + SPRITE_W / 2);
            int p_col = pixel_to_tile_col(players[i].y_pos + SPRITE_H / 2);

            bool activated = false;

            /* Method 1: same-tile proximity (< 20px on both axes)
             * Decompiled ref: seg_1000:4766-4781 */
            int dx = abs(e->x_pos - players[i].x_pos);
            int dy = abs(e->y_pos - players[i].y_pos);
            if (dx < ENTITY_ACTIVATION_DIST && dy < ENTITY_ACTIVATION_DIST) {
                activated = true;
            }

            /* Method 2: rectangular line-of-sight
             * Decompiled ref: seg_1000:4782-4912
             * Condition: entity shares exactly one tile axis with player
             * (same row XOR same col). If all tiles in the bounding
             * rectangle are passable → activate. */
            if (!activated) {
                bool same_row = (e_row == p_row);
                bool same_col = (e_col == p_col);
                if (same_row != same_col) {  /* XOR: exactly one axis shared */
                    if (rect_all_passable(map, e_row, e_col, p_row, p_col)) {
                        activated = true;
                    }
                }
            }

            /* Method 3: directional fan (7-tile expanding cone)
             * Decompiled ref: seg_1000:4914-4977 */
            if (!activated) {
                if (check_directional_fan_activation(e, p_row, p_col)) {
                    activated = true;
                }
            }

            if (activated) {
                e->active = 1;
                sfx_play(SFX_KARJAISU);
                break;  /* only activate once */
            }
        }
    }
}

/*
 * Deal damage from active entities to players sharing the same tile.
 * Called EVERY FRAME, matching decompiled monster_player_collision
 * (seg_1000:5803-5900, called at main loop line 7261 outside any
 * frame-counter modulo check).
 *
 * Uses exact tile matching (entity tile == player tile), NOT pixel
 * proximity. Decompiled ref:
 *   entity_row = offset_0xEE / 10   (seg_1000:5834)
 *   entity_col = (offset_0xF0 - 0x1E) / 10  (seg_1000:5835)
 *   player matches when player_row == entity_row AND player_col == entity_col
 *   (seg_1000:5839-5840)
 *
 * Owner check: entity offset 0xFD (owner_player) prevents friendly fire
 * (seg_1000:5843/5859/5875/5891).
 */
void entities_deal_damage(Entity *head, Player players[], int num_players)
{
    for (Entity *e = head; e != NULL; e = e->next) {
        if (e->dead || !e->active) continue;

        /* Both entity and player use raw position / 10 for tile matching,
         * matching decompiled: offset_0xEE / 10 for both (seg_1000:5834,5839) */
        int e_row = pixel_to_tile_row(e->x_pos);
        int e_col = pixel_to_tile_col(e->y_pos);

        for (int i = 0; i < num_players; i++) {
            if (players[i].dead) continue;
            if (e->owner_player == (uint8_t)i) continue;

            int p_row = pixel_to_tile_row(players[i].x_pos);
            int p_col = pixel_to_tile_col(players[i].y_pos);

            if (p_row == e_row && p_col == e_col && players[i].health > 0) {
                players[i].health -= e->attack_power;
            }
        }
    }
}

void entities_cleanup(Entity **head_ptr)
{
    if (!head_ptr) return;

    Entity *e = *head_ptr;
    while (e != NULL) {
        Entity *next = e->next;
        free(e);
        live_allocs--;
        e = next;
    }
    *head_ptr = NULL;
}

int entities_allocated_count(void)
{
    return live_allocs;
}

void entity_list_add(Entity **head_ptr, Entity *e)
{
    if (!head_ptr || !e) return;
    e->next = *head_ptr;
    *head_ptr = e;
}

int entities_count_alive(const Entity *head)
{
    int count = 0;
    for (const Entity *e = head; e != NULL; e = e->next) {
        if (!e->dead) count++;
    }
    return count;
}
