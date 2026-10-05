#include "game/entity.h"
#include "game/movement.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include "audio/sfx.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Names of the four monster templates (seg_1010:5578-5611; strings at
 * seg_1010:9D76, 9D80, 9D8D, 9D98) and of the Robot (9D9E). */
static const char *ENTITY_NAMES[ENTITY_TYPE_COUNT] = {
    "KarvaMies", "HarmaaPeikko", "LimaPeikko", "Alien"
};
#define ROBOT_NAME "Robot"

/*
 * Decode spawn tile 'G'-'V' into type and initial direction.
 * Within each group of 4 (from decompiled seg_1000:4626-4637):
 *   offset 0 → direction 1 (DIR_RIGHT): G/K/O/S
 *   offset 1 → direction 2 (DIR_LEFT):  H/L/P/T
 *   offset 2 → direction 3 (DIR_UP):    I/M/Q/U
 *   offset 3 → direction 4 (DIR_DOWN):  J/N/R/V
 */
static bool decode_spawn_tile(uint8_t tile, uint8_t *out_type, uint8_t *out_dir)
{
    if (tile < 'G' || tile > 'V') return false;

    int index = tile - 'G';          /* 0-15 */
    *out_type = (uint8_t)(index / 4); /* 0-3 */

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

/* The fields both spawners set (seg_1000:4600-4622, 2527-2573). */
static void body_init(Entity *e, const char *name, int tile_col, int tile_row)
{
    Player *b = &e->body;
    strncpy(b->name, name, sizeof(b->name) - 1);
    b->dead = 0;
    b->has_stats = 0;

    /* The original stores the tile centre and draws at centre - 5; the
     * port stores the top-left corner (row→X, col→Y). */
    b->x_pos = (int16_t)(tile_row * TILE_SIZE);
    b->y_pos = (int16_t)(tile_col * TILE_SIZE + MAP_Y_OFFSET);

    /* +0xA2 starts at 1 in every template (DAT_1038_20b4 and siblings) */
    b->anim_frame = 1;
    e->next = NULL;
}

Entity *entity_spawn(uint8_t spawn_tile, int tile_col, int tile_row)
{
    uint8_t type, dir;
    if (!decode_spawn_tile(spawn_tile, &type, &dir)) return NULL;

    Entity *e = entity_alloc();
    if (!e) return NULL;

    body_init(e, ENTITY_NAMES[type], tile_col, tile_row);
    e->type = type;
    e->body.health = ENTITY_ATTACK[type];
    e->body.max_health = ENTITY_HEALTH[type];
    e->body.digging_power = 0;
    e->body.bonus_stat = ENTITY_DIG[type];
    e->body.awake = 0;  /* starts dormant */
    e->body.direction = dir;
    e->body.last_direction = dir;
    e->speed_divisor = ENTITY_SPEED[type];
    e->owner_player = 0xFF; /* no owner (map-spawned) */

    return e;
}

Entity *entity_spawn_creature(int owner, const Player *owner_player,
                              int tile_col, int tile_row)
{
    Entity *e = entity_alloc();
    if (!e) return NULL;

    body_init(e, ROBOT_NAME, tile_col, tile_row);
    e->type = ENTITY_TYPE_ROBOT;
    e->body.health = ROBOT_ATTACK;
    e->body.max_health = ROBOT_HEALTH;
    e->body.awake = 1;  /* template +0x103 = 1 (DAT_1038_2329) */
    e->body.direction = DIR_DOWN;
    e->body.last_direction = DIR_DOWN;
    e->speed_divisor = ROBOT_SPEED;
    e->owner_player = (uint8_t)owner;

    if (owner_player) {
        /* Dig strength, direction and facing come from the owner
         * (seg_1000:2551-2562, 2574-2575). While the owner's money bomb
         * loan is running its +0xA8 carries the loan's 300, which the
         * Robot does not get. */
        e->body.bonus_stat = owner_player->bonus_stat;
        e->body.digging_power = owner_player->digging_power;
        if (owner_player->money_bomb_counter != 0) {
            e->body.digging_power = (int16_t)(e->body.digging_power - 300);
        }
        e->body.direction = owner_player->direction;
        e->body.last_direction = owner_player->last_direction;
    }

    return e;
}

bool entity_is_robot(const Entity *e)
{
    return e && e->type == ENTITY_TYPE_ROBOT;
}

void entity_kill(Entity *e)
{
    if (e) e->body.dead = 1;
}

bool entity_move(Entity *e, TileMap *map, Player players[], int num_players)
{
    if (!e || e->body.dead) return false;

    int32_t earned_before = e->body.earned;
    int16_t dig_before = e->body.digging_power;

    /* The same two calls the round loop makes for a player. */
    bool moved = player_move(&e->body, map);
    player_dig(&e->body, map);

    /* What an awake Robot picks up is added to its owner's dig power and
     * round earnings as well as to its own (seg_1000:3463-3496). */
    if (entity_is_robot(e) && e->body.awake && players &&
        e->owner_player < num_players) {
        Player *owner = &players[e->owner_player];
        owner->earned += e->body.earned - earned_before;
        owner->digging_power =
            (int16_t)(owner->digging_power + e->body.digging_power - dig_before);
    }

    return moved;
}

void entities_round_start_step(Entity *head, TileMap *map,
                               Player players[], int num_players)
{
    for (Entity *e = head; e != NULL; e = e->next) {
        entity_move(e, map, players, num_players);
    }
}

/* Tile under a monster or player: the original divides the centre
 * coordinate (+0xEE, +0xF0) by 10. */
static int centre_row(const Player *p)
{
    return pixel_to_tile_row(p->x_pos + SPRITE_W / 2);
}

static int centre_col(const Player *p)
{
    return pixel_to_tile_col(p->y_pos + SPRITE_H / 2);
}

void entity_deal_damage(const Entity *e, Player players[], int num_players)
{
    int e_row = centre_row(&e->body);
    int e_col = centre_col(&e->body);

    for (int i = 0; i < num_players; i++) {
        /* The owner test applies to a Robot only (name compare at
         * seg_1000:5842); +0xFD of a map monster is never set. */
        if (entity_is_robot(e) && e->owner_player == (uint8_t)i) continue;

        if (centre_row(&players[i]) == e_row &&
            centre_col(&players[i]) == e_col && players[i].health > 0) {
            players[i].health -= e->body.health;
        }
    }
}

static void move_and_face(Entity *e, TileMap *map, Player players[],
                          int num_players, int frame_counter)
{
    /* Speed 6 → moves 5 of every 6 frames; speed 100 → 99 of every 100
     * (seg_1000:5901-5903). */
    if (e->speed_divisor > 0 && (frame_counter % e->speed_divisor) != 0) {
        entity_move(e, map, players, num_players);
    }
    if (e->body.direction != DIR_STOP) {
        e->body.last_direction = e->body.direction;
    }
}

void entity_tick(Entity *e, TileMap *map, Player players[], int num_players,
                 int frame_counter)
{
    if (!e || e->body.dead || !e->body.awake) return;
    entity_deal_damage(e, players, num_players);
    move_and_face(e, map, players, num_players, frame_counter);
}

void entities_update(Entity *head, TileMap *map,
                     Player players[], int num_players,
                     int frame_counter)
{
    for (Entity *e = head; e != NULL; e = e->next) {
        if (e->body.dead || !e->body.awake) continue;
        move_and_face(e, map, players, num_players, frame_counter);
    }
}

void entities_deal_damage(Entity *head, Player players[], int num_players)
{
    for (Entity *e = head; e != NULL; e = e->next) {
        if (e->body.dead || !e->body.awake) continue;
        entity_deal_damage(e, players, num_players);
    }
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
    int e_row = centre_row(&e->body);
    int e_col = centre_col(&e->body);

    for (int ring = 1; ring < 8; ring++) {
        int spread = ring - 1;
        for (int offset = -spread; offset <= spread; offset++) {
            int check_row, check_col;
            switch (e->body.direction) {
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
        if (e->body.dead || e->body.awake) continue;

        int e_row = centre_row(&e->body);
        int e_col = centre_col(&e->body);

        for (int i = 0; i < num_players; i++) {
            if (players[i].dead) continue;

            int p_row = centre_row(&players[i]);
            int p_col = centre_col(&players[i]);

            bool activated = false;

            /* Method 1: same-tile proximity (< 20px on both axes)
             * Decompiled ref: seg_1000:4766-4781 */
            int dx = abs(e->body.x_pos - players[i].x_pos);
            int dy = abs(e->body.y_pos - players[i].y_pos);
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
                e->body.awake = 1;
                sfx_play(SFX_KARJAISU);
                break;  /* only activate once */
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
    e->next = NULL;
    while (*head_ptr) {
        head_ptr = &(*head_ptr)->next;
    }
    *head_ptr = e;
}

int entities_count_alive(const Entity *head)
{
    int count = 0;
    for (const Entity *e = head; e != NULL; e = e->next) {
        if (!e->body.dead) count++;
    }
    return count;
}
