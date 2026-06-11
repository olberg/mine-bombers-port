#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "game/player.h"
#include "game/map.h"
#include <stdbool.h>

/* Direction constants — values AND semantics match the original.
 * Ground truth: move_player dispatch (seg_1000:3898/3939/3981/4021):
 * 1 = x+1 (RIGHT, bound 635), 2 = x-1 (LEFT), 3 = y-1 (UP, bound 36),
 * 4 = y+1 (DOWN, bound 475); key handlers seg_1000:2608-2630 and the
 * monster spawner seg_1000:4625-4636 agree. The previous values here
 * (1=DOWN 2=UP 3=LEFT 4=RIGHT) were a consistent relabel that leaked at
 * original-data boundaries (spawn variants, arrow tiles). */
#define DIR_STOP  0
#define DIR_RIGHT 1
#define DIR_LEFT  2
#define DIR_UP    3
#define DIR_DOWN  4

/* Check if a tile is passable (floor, corpse, floor variant). */
bool tile_is_passable(uint8_t tile);

/* Move player one pixel in their current direction. Handles collision.
 * Also triggers pickup checks when crossing tile centers (matching original
 * seg_1000:3920-4043 behavior).
 * Returns true if the player actually moved. */
bool player_move(Player *p, TileMap *map);

/* Apply digging damage to the wall tile adjacent to the player in their
 * movement direction. Called each frame the player pushes against a wall.
 * Subtracts (digging_power + bonus_stat) from wall collision HP.
 * Handles wall degradation visuals and destruction. (seg_1000:3709-3764) */
void player_dig(Player *p, TileMap *map);

/* Process pickup at a specific tile position.
 * Handles treasure, health, weapon pickups, teleporters, gate switches.
 * Clears tile to '0' for consumable pickups.
 * Returns true if something was picked up.
 *
 * In the original (seg_1000:3920-4043), this is called from within the
 * movement function when the player crosses the tile center (intra == 5),
 * checking the tile AHEAD in the movement direction — not the current tile.
 * This prevents teleporters (0x9C) from re-triggering every frame. */
bool player_check_pickup(Player *p, TileMap *map, int row, int col);

#endif
