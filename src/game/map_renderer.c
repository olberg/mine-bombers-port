#include "game/map_renderer.h"
#include "game/sprites.h"
#include "raylib.h"
#include <stddef.h>

static const TileMap *current_map = NULL;
static bool darkness_mode = false;

void map_renderer_init(void)
{
    current_map = NULL;
    darkness_mode = false;
}

void map_renderer_set_map(const TileMap *map)
{
    current_map = map;
}

void map_renderer_set_darkness(bool enabled)
{
    darkness_mode = enabled;
}

/* Draw the map.  The original VGA convention (confirmed via draw_map_tile →
 * blit_sprite parameter analysis) is:
 *   screen X = row * 10          (row 0-63 → 0-630px, fills 640px width)
 *   screen Y = col * 10 + 30     (col 0-44 → 30-470px, below HUD)
 * The entire map fits on screen — no scrolling camera.
 * y_offset is only non-zero during screen shake (seg_1010:7705-7725). */
void map_renderer_draw(int y_offset)
{
    if (!current_map) return;

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            int px = row * TILE_SIZE;
            int py = col * TILE_SIZE + MAP_Y_OFFSET + y_offset;

            if (darkness_mode && (current_map->layer4[row][col] & 0x01)) {
                DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, BLACK);
                continue;
            }

            uint8_t tile = current_map->tiles[row][col];
            sprites_draw_tile(tile, px, py);
        }
    }
}

/* Pixel ↔ tile conversions.
 * Original VGA convention: row (first array index, 0-63) = screen X,
 * col (second array index, 0-44) = screen Y with MAP_Y_OFFSET.
 *
 * pixel_to_tile_row(x_pixel) → first array index  (row)
 * pixel_to_tile_col(y_pixel) → second array index (col)
 * tile_to_pixel_x(row)       → screen X
 * tile_to_pixel_y(col)       → screen Y
 *
 * CALLERS must pass x_pixel to pixel_to_tile_row and y_pixel to
 * pixel_to_tile_col — this is the transpose from the old convention. */

int pixel_to_tile_col(int y)
{
    return (y - MAP_Y_OFFSET) / TILE_SIZE;
}

int pixel_to_tile_row(int x)
{
    return x / TILE_SIZE;
}

int tile_to_pixel_x(int row)
{
    return row * TILE_SIZE;
}

int tile_to_pixel_y(int col)
{
    return col * TILE_SIZE + MAP_Y_OFFSET;
}

void map_renderer_cleanup(void)
{
    current_map = NULL;
    darkness_mode = false;
}
