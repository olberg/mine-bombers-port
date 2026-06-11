#ifndef AI_H
#define AI_H

#include "game/entity.h"
#include "game/map.h"
#include "game/player.h"
#include <stdbool.h>

/*
 * Monster AI system. Runs expanding-spiral searches to find targets,
 * then moves entities toward items/players or away from hazards.
 *
 * Decision frequency: every 26 frames.
 * Random direction changes: every 33 (if blocked) or 121 frames (unconditional).
 */

/* Search radii (in tiles) */
#define AI_ITEM_RADIUS     5
#define AI_PLAYER_RADIUS  10
#define AI_HAZARD_RADIUS  63

/* Decision timing */
#define AI_DECISION_TICK  26
#define AI_RANDOM_TICK    33
#define AI_RANDOM_TICK2  121

/* Random move chance (3% = 3 out of 100) for axis swap in approach */
#define AI_RANDOM_SWAP_CHANCE 3

/* Bomb placement: minimum clear tiles in direction */
#define AI_BOMB_CLEAR_TILES   5

/* Search result */
typedef struct {
    bool found;
    int  tile_col;
    int  tile_row;
    int  player_index;  /* index of found player (-1 if not a player search) */
} AiSearchResult;

/*
 * Main AI update for a single entity. Call every frame;
 * internally checks frame_counter for decision timing.
 * entity_head: linked list of all entities (for bomb safety checks).
 * treasure_count: number of treasures remaining on map (gates hazard search).
 */
void ai_update(Entity *e, TileMap *map,
               const Player players[], int num_players,
               int frame_counter, Entity *entity_head,
               int treasure_count);

/*
 * Expanding square spiral search for collectible items.
 * Searches weapon tiles, treasure, etc. within radius.
 */
AiSearchResult ai_find_item(const TileMap *map, int center_col, int center_row,
                             int radius);

/*
 * Expanding square spiral search for a player.
 * Returns position of the nearest found player.
 */
AiSearchResult ai_find_player(const Player players[], int num_players,
                               int center_col, int center_row, int radius,
                               uint8_t ignore_owner);

/*
 * Expanding square spiral search for hazards (active bombs).
 * Returns position of nearest bomb/hazard tile.
 */
AiSearchResult ai_find_hazard(const TileMap *map, int center_col, int center_row,
                               int radius);

/*
 * Set entity direction to move toward target tile.
 * Picks axis with larger distance; 3% random chance to swap axes.
 */
void ai_move_toward(Entity *e, int target_col, int target_row,
                    const TileMap *map);

/*
 * Set entity direction to move away from threat tile.
 * If both axes blocked, picks a random direction.
 */
void ai_move_away(Entity *e, int threat_col, int threat_row,
                  const TileMap *map);

/*
 * Check if entity's direction is blocked by an impassable tile.
 */
bool ai_is_blocked(const Entity *e, const TileMap *map);

/*
 * Check if placing a bomb in the entity/player's current direction is safe.
 * Counts clear tiles ahead (up to 10). Returns true if 5+ clear tiles
 * and no allies in the blast line (from decompiled FUN_1000_8c05).
 * direction: current movement direction (DIR_DOWN/UP/LEFT/RIGHT).
 * x_pos, y_pos: current pixel position.
 * owner_idx: player index to exclude from ally check (0-3).
 * entity_head: linked list of entities for friendly-fire check (can be NULL).
 * self: entity to exclude from the entity-in-path check (can be NULL).
 */
bool ai_should_place_bomb(const TileMap *map, int x_pos, int y_pos,
                          uint8_t direction, const Player players[],
                          int num_players, uint8_t owner_idx,
                          const Entity *entity_head, const Entity *self);

/*
 * Place a directional arrow bomb at the entity's current tile.
 * Writes arrow tile (0xA5-0xA8) based on entity direction and sets
 * overlay=1 for immediate detonation next frame.
 * Only places if safety check passes and an enemy player shares
 * one axis (row or column) but is not on the exact same tile (XOR check).
 * From decompiled entity_interaction (seg_1000:5658-5703) + FUN_1000_88ba.
 */
void ai_entity_place_bomb(Entity *e, TileMap *map,
                          const Player players[], int num_players,
                          const Entity *entity_head);

#endif
