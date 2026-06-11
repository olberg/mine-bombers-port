#ifndef ENTITY_H
#define ENTITY_H

#include "game/player.h"
#include "game/map.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Monster/entity system. Entities are spawned from map tiles 'G'-'V'
 * or by the creature spawner weapon ('n'). They form a singly-linked list.
 *
 * Four monster types with different speeds:
 *   G-J (0x47-0x4A): speed 6   (moves 5 of every 6 frames)
 *   K-N (0x4B-0x4E): speed 3   (moves 2 of every 3 frames)
 *   O-R (0x4F-0x52): speed 2   (moves 1 of every 2 frames)
 *   S-V (0x53-0x56): speed 100 (moves 99 of every 100 frames)
 *
 * Within each group, the 4 letters encode initial facing direction:
 *   1st = down, 2nd = up, 3rd = left, 4th = right
 */

/* Monster type constants */
#define ENTITY_TYPE_1  0  /* G-J: speed 6 */
#define ENTITY_TYPE_2  1  /* K-N: speed 3 */
#define ENTITY_TYPE_3  2  /* O-R: speed 2 */
#define ENTITY_TYPE_4  3  /* S-V: speed 100 */
#define ENTITY_TYPE_COUNT 4

/* Speed per type (higher = faster; entity moves when frame % speed != 0).
 * Values from original monster templates, seg_1010:5586/5597/5608/5619
 * (DAT_1038_211a/2224/2438/2542). */
static const uint8_t ENTITY_SPEED[ENTITY_TYPE_COUNT] = { 6, 3, 2, 100 };

/* Max health per type, from original templates seg_1010:5587/5598/5610/5621
 * (DAT_1038_202f/2139/234d/2457 at template offset +0x1D). */
static const int16_t ENTITY_HEALTH[ENTITY_TYPE_COUNT] = { 29, 29, 10, 66 };

/* Attack damage per type, from original templates seg_1010:5588/5599/5609/5620
 * (DAT_1038_202d/2137/234b/2455 at template offset +0x1B). */
static const int16_t ENTITY_ATTACK[ENTITY_TYPE_COUNT] = { 2, 3, 1, 5 };

/* Activation distance in pixels (absolute, both X and Y) */
#define ENTITY_ACTIVATION_DIST  20

/* Collision: damage dealt = entity health */

typedef struct Entity {
    /* Identity */
    char     name[26];
    uint8_t  type;              /* ENTITY_TYPE_* */

    /* Stats */
    int16_t  health;
    int16_t  max_health;
    int16_t  attack_power;      /* base damage (= health in original) */
    uint8_t  dead;              /* 0=alive, 1=dead */
    uint8_t  active;            /* 0=dormant, 1=hostile */

    /* Position & movement */
    int16_t  x_pos;             /* pixel X */
    int16_t  y_pos;             /* pixel Y */
    uint8_t  direction;         /* uses DIR_* from movement.h */
    uint8_t  prev_direction;
    uint8_t  speed_divisor;     /* movement throttle */

    /* Animation */
    int16_t  anim_state;

    /* Ownership */
    uint8_t  owner_player;      /* 0-3: who spawned this (prevents friendly fire) */

    /* Linked list */
    struct Entity *next;
} Entity;

/*
 * Spawn an entity from a map tile character ('G'-'V').
 * Returns newly allocated Entity, or NULL if tile is not a spawn tile.
 * The entity starts dormant (active=0).
 */
Entity *entity_spawn(uint8_t spawn_tile, int tile_col, int tile_row);

/*
 * Spawn an entity from the creature spawner weapon.
 * owner = player index (0-3). Inherits some player stats.
 */
Entity *entity_spawn_creature(int owner, int tile_col, int tile_row);

/*
 * Mark entity as dead. Does NOT free memory or unlink from list.
 */
void entity_kill(Entity *e);

/*
 * Move an entity one pixel in its current direction, checking passability.
 * Returns true if entity actually moved.
 */
bool entity_move(Entity *e, const TileMap *map);

/*
 * Update all entities in the linked list:
 *  - Move entities (throttled by speed_divisor)
 *  - Check activation (proximity to players)
 * frame_counter is used for speed throttling.
 */
void entities_update(Entity *head, const TileMap *map,
                     const Player players[], int num_players,
                     int frame_counter);

/*
 * Activate dormant entities that detect nearby players.
 * Called every 5 frames, matching decompiled player_collision_check
 * (seg_1000:4699-4996, called at main loop lines 7253-7259).
 * Three detection methods: proximity, rectangular LOS, directional fan.
 * Plays SFX_KARJAISU on activation.
 */
void entities_activate(Entity *head, const Player players[], int num_players,
                       const TileMap *map);

/*
 * Deal damage from active entities to players sharing the same tile.
 * Called EVERY FRAME, matching decompiled monster_player_collision
 * (seg_1000:5803-5900, called at main loop line 7261).
 * Uses exact tile matching, not pixel proximity.
 * Owner player is immune.
 */
void entities_deal_damage(Entity *head, Player players[], int num_players);

/*
 * Free all entities in the linked list. Sets *head_ptr to NULL.
 */
void entities_cleanup(Entity **head_ptr);

/*
 * Add an entity to the front of the linked list.
 */
void entity_list_add(Entity **head_ptr, Entity *e);

/*
 * Count alive entities in the list.
 */
int entities_count_alive(const Entity *head);

/*
 * Live Entity allocations (allocs minus frees). The autoplay soak asserts
 * this is 0 at match end — leak detection by counter, since ASan is
 * unavailable on this MinGW toolchain.
 */
int entities_allocated_count(void);

#endif
