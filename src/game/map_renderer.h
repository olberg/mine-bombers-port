#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

#include "game/map.h"
#include <stdbool.h>

/* Initialize map renderer (call after sprites_init). */
void map_renderer_init(void);

/* Set the current map to render. */
void map_renderer_set_map(const TileMap *map);

/* Draw entire map with optional vertical pixel offset (for screen shake).
 * Original uses a fixed viewport — no scrolling camera.  The entire map
 * fits on screen (45 cols × 10px + 30px HUD = 480px).
 * y_offset is only non-zero during screen shake. */
void map_renderer_draw(int y_offset);

/* True when the tile is drawn black: darkness is on, the tile is still hidden
 * (layer4 bit 0) and it is not part of the outer ring, which is always shown. */
bool map_renderer_tile_hidden(const TileMap *map, int row, int col);

/* Convert pixel position to tile array indices.
 * Original VGA: row (first index) = screen X, col (second index) = screen Y.
 *   pixel_to_tile_row(x_pixel) → first array index  (row)
 *   pixel_to_tile_col(y_pixel) → second array index (col) */
int pixel_to_tile_col(int y);
int pixel_to_tile_row(int x);

/* Convert tile array indices to pixel position.
 *   tile_to_pixel_x(row) → screen X
 *   tile_to_pixel_y(col) → screen Y */
int tile_to_pixel_x(int row);
int tile_to_pixel_y(int col);

void map_renderer_cleanup(void);

#endif
