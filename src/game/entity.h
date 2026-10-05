#ifndef ENTITY_H
#define ENTITY_H

#include "game/player.h"
#include "game/map.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Monsters. As in the original, a monster IS a player struct: the same
 * 0x109-byte record, kept on a singly linked list (next pointer at +0x104)
 * and moved by the same routine as a player (move_player, seg_1000:3860,
 * called per monster at seg_1000:5901-5903). So a monster walks by the same
 * centre-snap rules, digs with its own dig strength, pushes bombs, and
 * takes whatever it pushes into: treasure, crates, medkits, teleporters,
 * switches.
 *
 * Monsters come from map tiles 'G'-'V' (four types, each in four facing
 * directions: right, left, up, down) or from the Robot weapon ('n').
 *
 * Fields of `body` that mean something else for a monster:
 *   body.health      (+0x1B)  contact damage per frame
 *   body.max_health  (+0x1D)  hit points
 *   body.awake       (+0x103) has noticed a player; dormant until then
 *   body.bonus_stat  (+0xAC)  dig strength
 *   body.has_stats            always 0 (the +0xFF stats pointer is null)
 */

/* Monster types, in template order (seg_1010:5578-5621) */
#define ENTITY_TYPE_1      0  /* G-J: KarvaMies */
#define ENTITY_TYPE_2      1  /* K-N: HarmaaPeikko */
#define ENTITY_TYPE_3      2  /* O-R: LimaPeikko */
#define ENTITY_TYPE_4      3  /* S-V: Alien */
#define ENTITY_TYPE_COUNT  4
#define ENTITY_TYPE_ROBOT  4  /* spawned by a player; drawn with its owner's sprites */

/* Speed per type: a monster moves on the frames where
 * frame % speed != 0 (+0x108; DAT_1038_211a/2224/2438/2542). */
static const uint8_t ENTITY_SPEED[ENTITY_TYPE_COUNT] = { 6, 3, 2, 100 };

/* Hit points per type (+0x1D; DAT_1038_202f/2139/234d/2457). */
static const int16_t ENTITY_HEALTH[ENTITY_TYPE_COUNT] = { 29, 29, 10, 66 };

/* Contact damage per type (+0x1B; DAT_1038_202d/2137/234b/2455). */
static const int16_t ENTITY_ATTACK[ENTITY_TYPE_COUNT] = { 2, 3, 1, 5 };

/* Dig strength per type (+0xAC; DAT_1038_20be/21c8/23dc/24e6). Their
 * +0xA8 is 0, so this is the whole hit a monster takes out of a wall. */
static const int16_t ENTITY_DIG[ENTITY_TYPE_COUNT] = { 5, 12, 12, 52 };

/* Robot template (0x1038:0x2226): contact damage 1, 100 hit points,
 * speed 100 (DAT_1038_2241/2243/232e). */
#define ROBOT_ATTACK  1
#define ROBOT_HEALTH  100
#define ROBOT_SPEED   100

/* Activation distance in pixels (absolute, both X and Y) */
#define ENTITY_ACTIVATION_DIST  20

typedef struct Entity {
    Player   body;              /* see the field notes above */
    uint8_t  type;              /* ENTITY_TYPE_* */
    uint8_t  speed_divisor;     /* +0x108 */
    uint8_t  owner_player;      /* Robot: owner's index 0-3 (+0xFD holds
                                   1-4); 0xFF for map monsters */
    struct Entity *next;
} Entity;

/*
 * Spawn a monster from a map tile character ('G'-'V').
 * Returns a newly allocated Entity, or NULL if tile is not a spawn tile.
 * It starts dormant.
 */
Entity *entity_spawn(uint8_t spawn_tile, int tile_col, int tile_row);

/*
 * Spawn a Robot for player index `owner` (FUN_1000_3b40, seg_1000:2494).
 * It starts awake, takes the owner's direction, facing and dig strength,
 * and its finds are credited to the owner. `owner_player` may be NULL.
 */
Entity *entity_spawn_creature(int owner, const Player *owner_player,
                              int tile_col, int tile_row);

/* True for a monster made by the Robot weapon (the original tests the
 * name "Robot"). */
bool entity_is_robot(const Entity *e);

/*
 * Mark entity as dead. Does NOT free memory or unlink from list.
 */
void entity_kill(Entity *e);

/*
 * One call of the shared movement routine for a monster: a pixel step in
 * its direction when the way is open, and at a tile centre the tile ahead
 * is dug, pushed or picked up. A Robot's treasure and dig gems are also
 * credited to its owner in `players` (seg_1000:3463-3496).
 * Returns true if the monster moved.
 */
bool entity_move(Entity *e, TileMap *map, Player players[], int num_players);

/*
 * Round start with darkness off: every monster, asleep or not, gets one
 * call of the movement routine (FUN_1000_758a, seg_1000:7131-7134).
 */
void entities_round_start_step(Entity *head, TileMap *map,
                               Player players[], int num_players);

/*
 * Contact damage from one monster to the players on its tile
 * (monster_player_collision, seg_1000:5836-5899). A Robot spares its owner.
 */
void entity_deal_damage(const Entity *e, Player players[], int num_players);

/*
 * One awake, living monster's share of a frame, in the original's order
 * (seg_1000:5836-5905): contact damage, the move (on frames where
 * frame % speed != 0), then facing follows direction. The AI decision
 * comes after this, from the caller.
 */
void entity_tick(Entity *e, TileMap *map, Player players[], int num_players,
                 int frame_counter);

/* The move and facing part of entity_tick for every awake, living monster. */
void entities_update(Entity *head, TileMap *map,
                     Player players[], int num_players,
                     int frame_counter);

/* entity_deal_damage for every awake, living monster. */
void entities_deal_damage(Entity *head, Player players[], int num_players);

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
 * Free all entities in the linked list. Sets *head_ptr to NULL.
 */
void entities_cleanup(Entity **head_ptr);

/*
 * Append an entity at the end of the linked list, as the original's
 * spawners do (seg_1000:4585-4599, 2506-2523); list order is the order
 * monsters act in.
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
