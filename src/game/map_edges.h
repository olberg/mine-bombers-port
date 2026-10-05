#ifndef MAP_EDGES_H
#define MAP_EDGES_H

#include "game/map.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Terrain edge sprites.
 *
 * The original draws every tile with one fixed sprite and then paints thin
 * edge strips (4 px wide on the left/right, 3 px tall on the top/bottom)
 * onto sand and stone tiles wherever they border open ground.  The strips
 * live in the persistent screen buffer, so what is visible depends on what
 * was drawn since the tile itself was last redrawn:
 *
 *   - redraw_game_screen (seg_1000:2906) paints plain edges around every
 *     "opening" tile when the level is first drawn (not in darkness);
 *   - digging a wall away calls draw_map_edges (seg_1000:2823) for the
 *     dug tile (seg_1000:3718);
 *   - a fire tile spawned by an explosion calls FUN_1010_0caa
 *     (seg_1010:541) which paints the "lit" variants on its neighbours;
 *   - redrawing a tile (mark_tile_for_redraw, seg_1010:5258) wipes whatever
 *     strips were on it.
 *
 * EdgeState keeps, per tile, the strips currently painted on it in paint
 * order.  map_edges_update derives the original's draw events by comparing
 * the map against the previous call, so the simulation needs no hooks.
 */

typedef enum {
    EDGE_LEFT = 0,   /* strip on the tile's low-row (screen X) side */
    EDGE_RIGHT,
    EDGE_TOP,        /* strip on the tile's low-col (screen Y) side */
    EDGE_BOTTOM
} EdgeSide;

/* Sprite ids: group * 4 + side. */
enum {
    EDGE_GROUP_LIT_SAND   = 0,  /* DAT_1038_0454..0460 */
    EDGE_GROUP_LIT_ROCK   = 1,  /* DAT_1038_0464..0470 */
    EDGE_GROUP_PLAIN_SAND = 2,  /* DAT_1038_0474..0480 */
    EDGE_GROUP_PLAIN_ROCK = 3,  /* DAT_1038_0484..0490 */
    EDGE_SPRITE_COUNT     = 16
};

typedef struct {
    int src_x, src_y;   /* rectangle in the SIKA.SPY sheet */
    int w, h;
    int dx, dy;         /* offset inside the tile, in screen pixels */
} EdgeSpriteInfo;

#define EDGE_MAX_LAYERS 8

typedef struct {
    uint8_t layers[MAP_ROWS][MAP_COLS][EDGE_MAX_LAYERS]; /* sprite id + 1, 0 = unused */
    uint8_t seen_tiles[MAP_ROWS][MAP_COLS];
    uint8_t seen_hidden[MAP_ROWS][MAP_COLS];
    bool    primed;
} EdgeState;

/* Source rectangle and in-tile offset of an edge sprite (id 0..15). */
const EdgeSpriteInfo *map_edges_sprite_info(int sprite);

/* Tiles that count as open ground for the level-start pass: the set tested
 * in redraw_game_screen (seg_1000:2948, set constant at seg_1000:471A). */
bool map_edges_tile_opens(uint8_t tile);

/* Sprite id (or -1) that draw_map_edges paints on a tile of type
 * `neighbour` on the given side when an opening tile touches that side. */
int map_edges_plain_sprite(uint8_t neighbour, EdgeSide side);

/* Same for FUN_1010_0caa, which paints around a freshly spawned fire tile. */
int map_edges_lit_sprite(uint8_t neighbour, EdgeSide side);

/* Terrain that a player digging away turns into floor ('0'), the case where
 * the original calls draw_map_edges (seg_1000:3698-3718). */
bool map_edges_tile_is_dig_source(uint8_t tile);

void map_edges_reset(EdgeState *st);

/* Apply the level-start pass (redraw_game_screen): wipe everything, then
 * paint plain edges around every opening tile unless darkness is on. */
void map_edges_start_round(EdgeState *st, const TileMap *map);

/* Bring the state up to date with the map; primes it on first use. */
void map_edges_update(EdgeState *st, const TileMap *map);

/* Strips painted on a tile, in paint order; returns how many were written
 * (at most EDGE_MAX_LAYERS) into out[] as sprite ids. */
int map_edges_tile_layers(const EdgeState *st, int row, int col, uint8_t *out);

/* Direct drawing events, exposed for tests. */
void map_edges_draw_around(EdgeState *st, const TileMap *map, int row, int col);
void map_edges_draw_lit(EdgeState *st, const TileMap *map, int row, int col);

#endif
