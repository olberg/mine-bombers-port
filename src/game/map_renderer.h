#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

#include "game/map.h"
#include <stdbool.h>

/* Initialize map renderer (call after sprites_init). */
void map_renderer_init(void);

/* Set the current map to render. */
void map_renderer_set_map(const TileMap *map);

/* Enable/disable fog-of-war darkness rendering.
 * When enabled, only tiles with layer4 bit 0 == 0 (revealed) are drawn;
 * hidden tiles render as black. Decompiled ref: seg_1010:670-672. */
void map_renderer_set_darkness(bool enabled);

/* Draw entire map with optional vertical pixel offset (for screen shake).
 * Original uses a fixed viewport — no scrolling camera.  The entire map
 * fits on screen (45 cols × 10px + 30px HUD = 480px).
 * y_offset is only non-zero during screen shake. */
void map_renderer_draw(int y_offset);

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
