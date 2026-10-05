#ifndef MAP_THUMBNAIL_H
#define MAP_THUMBNAIL_H

#include "game/map.h"
#include <stdint.h>

/*
 * One-pixel-per-tile picture of a map, 64 wide and 45 high. The original
 * draws it in two places: the shop's NEXT LEVEL panel (FUN_1010_b227, at
 * 0x120, 0x33) and the map picker's preview box (FUN_1010_db96, at 0x14a, 7).
 * There is no such map during a round.
 */
#define MAP_THUMBNAIL_SHOP_X  288   /* 0x120 */
#define MAP_THUMBNAIL_SHOP_Y   51   /* 0x33 */

/* Palette index a tile is drawn with (FUN_1010_dab7, seg_1010:8150-8199). */
uint8_t map_thumbnail_color(uint8_t tile);

/* Draw the map with its top-left corner at (x, y), in the current palette.
 * VGA convention: row (0-63) runs along screen X, col (0-44) along screen Y. */
void map_thumbnail_draw(const TileMap *map, int x, int y);

#endif
