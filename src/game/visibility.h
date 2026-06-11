#ifndef VISIBILITY_H
#define VISIBILITY_H

#include "game/map.h"
#include "game/player.h"
#include <stdbool.h>

/*
 * Fog-of-war / darkness visibility system.
 *
 * Decompiled ref: seg_1000_game_logic.c:3082-3306 (raycasting),
 *                 seg_1010_graphics.c:5258-5274 (mark_tile_for_redraw),
 *                 seg_1010_graphics.c:7197-7202 (activation logic),
 *                 seg_1010_graphics.c:7436-7449 (init all hidden).
 *
 * The original uses g_map_layer4 bit 0 as a per-tile visibility flag:
 *   bit 0 = 1: tile is hidden (not yet revealed by player line-of-sight)
 *   bit 0 = 0: tile has been revealed
 *
 * Darkness is ALWAYS on in single-player, optional in multiplayer via
 * the "Darkness" config option (g_config.option_toggle[0]).
 *
 * Each frame, a 40-ray Bresenham fan cone is cast in the player's facing
 * direction (20 tiles deep). Additionally, 8 neighbors are revealed at
 * each tile along the player's forward corridor path.
 */

/* Initialize visibility: mark all tiles as hidden (layer4 bit 0 = 1).
 * Called at round start. Decompiled ref: seg_1010:7436-7449. */
void visibility_init(TileMap *map);

/* Cast visibility rays from a player's position in their facing direction.
 * Reveals tiles along 40 Bresenham rays (20 tiles deep) + corridor neighbors.
 * Decompiled ref: FUN_1000_4d25 (seg_1000:3156-3306). */
void visibility_reveal_player(TileMap *map, const Player *p);

/* Check if a tile has been revealed (layer4 bit 0 == 0). */
bool visibility_is_revealed(const TileMap *map, int row, int col);

/* Reveal a single tile (clear layer4 bit 0). */
void visibility_reveal_tile(TileMap *map, int row, int col);

/* Check if tile blocks line-of-sight for visibility rays.
 * Walls and solid objects block; floor, decorations, pickups don't. */
bool visibility_tile_blocks_los(uint8_t tile);

#endif
