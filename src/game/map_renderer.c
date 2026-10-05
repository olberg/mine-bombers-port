#include "game/map_renderer.h"
#include "game/map_edges.h"
#include "game/sprites.h"
#include "raylib.h"
#include <stddef.h>

static const TileMap *current_map = NULL;
static EdgeState edges;

void map_renderer_init(void)
{
    current_map = NULL;
    map_edges_reset(&edges);
}

void map_renderer_set_map(const TileMap *map)
{
    current_map = map;
    map_edges_reset(&edges);
}

/* Fog of war: with darkness on, only revealed tiles (layer4 bit 0 clear) are
 * drawn; hidden ones render black (seg_1010:670-672).  The outer ring of
 * tiles (rows 0 and 63, columns 0 and 44) is always drawn: redraw_game_screen
 * paints it directly when darkness is on (FUN_1010_97b5, seg_1010:5279-5340). */
bool map_renderer_tile_hidden(const TileMap *map, int row, int col)
{
    if (!map->darkness_enabled)
        return false;
    if (row == 0 || row == MAP_ROWS - 1 || col == 0 || col == MAP_COLS - 1)
        return false;
    return (map->layer4[row][col] & 0x01) != 0;
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

    map_edges_update(&edges, current_map);

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            int px = row * TILE_SIZE;
            int py = col * TILE_SIZE + MAP_Y_OFFSET + y_offset;

            if (map_renderer_tile_hidden(current_map, row, col)) {
                DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, BLACK);
            } else {
                sprites_draw_tile(current_map->tiles[row][col], px, py);
            }

            /* Edge strips are painted over the tile, hidden or not: the
             * original draws them into the screen buffer regardless. */
            uint8_t layers[EDGE_MAX_LAYERS];
            int n = map_edges_tile_layers(&edges, row, col, layers);
            for (int i = 0; i < n; i++) {
                const EdgeSpriteInfo *e = map_edges_sprite_info(layers[i]);
                sprites_draw_region(e->src_x, e->src_y, e->w, e->h,
                                    px + e->dx, py + e->dy);
            }
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
}
