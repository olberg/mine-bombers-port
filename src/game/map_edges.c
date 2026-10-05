#include "game/map_edges.h"
#include <string.h>

/* Edge sprite rectangles, from the capture_screen_region calls at
 * seg_1010:4812-4890 (arguments are Y2, X2, Y1, X1 on the 320x240 sheet).
 * Left/right strips are 4x10, top/bottom strips 10x3; right strips sit at
 * +6 px, bottom strips at +7 px inside the tile. */
static const EdgeSpriteInfo EDGE_SPRITES[EDGE_SPRITE_COUNT] = {
    /* DAT_1038_0454, 0458, 045c, 0460 */
    { 194, 117, 4, 10, 0, 0 }, { 200, 117, 4, 10, 6, 0 },
    { 194, 128, 10, 3, 0, 0 }, { 194, 132, 10, 3, 0, 7 },
    /* DAT_1038_0464, 0468, 046c, 0470 */
    { 205, 117, 4, 10, 0, 0 }, { 211, 117, 4, 10, 6, 0 },
    { 205, 128, 10, 3, 0, 0 }, { 205, 132, 10, 3, 0, 7 },
    /* DAT_1038_0474, 0478, 047c, 0480 */
    { 194,  98, 4, 10, 0, 0 }, { 200,  98, 4, 10, 6, 0 },
    { 194, 109, 10, 3, 0, 0 }, { 194, 113, 10, 3, 0, 7 },
    /* DAT_1038_0484, 0488, 048c, 0490 */
    { 148,  60, 4, 10, 0, 0 }, { 154,  60, 4, 10, 6, 0 },
    { 148,  71, 10, 3, 0, 0 }, { 148,  75, 10, 3, 0, 7 },
};

const EdgeSpriteInfo *map_edges_sprite_info(int sprite)
{
    if (sprite < 0 || sprite >= EDGE_SPRITE_COUNT) return NULL;
    return &EDGE_SPRITES[sprite];
}

/* Bitmap of the set at seg_1000:471A (32 bytes, bit n of byte n/8). */
static const uint8_t OPENING_SET[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x04, 0x00, 0x80, 0x03, 0xf8, 0x3f, 0x88, 0xf3,
    0x0f, 0xfc, 0xff, 0xf7, 0xff, 0x8f, 0x30, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

bool map_edges_tile_opens(uint8_t tile)
{
    return (OPENING_SET[tile >> 3] >> (tile & 7)) & 1;
}

static bool in_range(uint8_t t, uint8_t lo, uint8_t hi)
{
    return t >= lo && t <= hi;
}

int map_edges_plain_sprite(uint8_t t, EdgeSide side)
{
    /* draw_map_edges, seg_1000:2823-2879.  Sand-like tiles '2'-'A' use the
     * first group, 'C'-'F' stone the second; which sides of '7'-'A' get a
     * strip depends on where their corner piece is already solid. */
    bool sand = false;
    switch (side) {
    case EDGE_LEFT:   sand = in_range(t, 0x32, 0x37) || t == 0x41; break;
    case EDGE_RIGHT:  sand = in_range(t, 0x32, 0x36) || t == 0x38 || t == 0x39; break;
    case EDGE_TOP:    sand = in_range(t, 0x32, 0x38); break;
    case EDGE_BOTTOM: sand = in_range(t, 0x32, 0x36) || t == 0x39 || t == 0x41; break;
    }
    if (sand) return EDGE_GROUP_PLAIN_SAND * 4 + side;
    if (in_range(t, 0x43, 0x46)) return EDGE_GROUP_PLAIN_ROCK * 4 + side;
    return -1;
}

int map_edges_lit_sprite(uint8_t t, EdgeSide side)
{
    /* FUN_1010_0caa, seg_1010:586-634. */
    if (in_range(t, 0x32, 0x36)) return EDGE_GROUP_LIT_SAND * 4 + side;
    if (in_range(t, 0x37, 0x39) || t == 0x41 || in_range(t, 0x43, 0x46))
        return EDGE_GROUP_LIT_ROCK * 4 + side;
    return -1;
}

bool map_edges_tile_is_dig_source(uint8_t t)
{
    /* Tiles FUN_1000_5073 clears to floor through the dig path: the sand
     * and wall types plus their degraded stages 'p' and 'q'. */
    return in_range(t, 0x32, 0x39) || t == 0x41 || in_range(t, 0x43, 0x46) ||
           t == 0x70 || t == 0x71;
}

void map_edges_reset(EdgeState *st)
{
    memset(st, 0, sizeof(*st));
}

static void paint(EdgeState *st, int row, int col, int sprite)
{
    uint8_t *l = st->layers[row][col];
    uint8_t id = (uint8_t)(sprite + 1);
    int n = 0;
    /* Repainting a strip puts it on top again. */
    for (int i = 0; i < EDGE_MAX_LAYERS && l[i]; i++) {
        if (l[i] != id) l[n++] = l[i];
    }
    if (n >= EDGE_MAX_LAYERS) n = EDGE_MAX_LAYERS - 1;
    l[n++] = id;
    while (n < EDGE_MAX_LAYERS) l[n++] = 0;
}

static bool inside(int row, int col)
{
    return row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS;
}

static void paint_neighbours(EdgeState *st, const TileMap *map, int row, int col,
                             int (*sprite_for)(uint8_t tile, EdgeSide side))
{
    /* draw_map_edges(col, row): the tile at row+1 gets its low-row side
     * painted, row-1 its high-row side, col+1 its low-col side, col-1 its
     * high-col side. */
    static const struct { int dr, dc; EdgeSide side; } NEIGHBOURS[4] = {
        { 1, 0, EDGE_LEFT }, { -1, 0, EDGE_RIGHT },
        { 0, 1, EDGE_TOP },  { 0, -1, EDGE_BOTTOM },
    };
    for (int i = 0; i < 4; i++) {
        int r = row + NEIGHBOURS[i].dr;
        int c = col + NEIGHBOURS[i].dc;
        if (!inside(r, c)) continue;
        int s = sprite_for(map->tiles[r][c], NEIGHBOURS[i].side);
        if (s >= 0) paint(st, r, c, s);
    }
}

void map_edges_draw_around(EdgeState *st, const TileMap *map, int row, int col)
{
    paint_neighbours(st, map, row, col, map_edges_plain_sprite);
}

void map_edges_draw_lit(EdgeState *st, const TileMap *map, int row, int col)
{
    paint_neighbours(st, map, row, col, map_edges_lit_sprite);
}

static bool tile_hidden(const TileMap *map, int row, int col)
{
    return map->darkness_enabled && (map->layer4[row][col] & 0x01);
}

static void snapshot(EdgeState *st, const TileMap *map)
{
    memcpy(st->seen_tiles, map->tiles, sizeof(st->seen_tiles));
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            st->seen_hidden[r][c] = tile_hidden(map, r, c);
}

void map_edges_start_round(EdgeState *st, const TileMap *map)
{
    map_edges_reset(st);
    st->primed = true;
    snapshot(st, map);
    if (map->darkness_enabled) return;
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            if (map_edges_tile_opens(map->tiles[row][col]))
                map_edges_draw_around(st, map, row, col);
        }
    }
}

void map_edges_update(EdgeState *st, const TileMap *map)
{
    if (!st->primed) {
        map_edges_start_round(st, map);
        return;
    }

    /* Redraws first: a tile whose sprite is drawn again loses its strips,
     * whichever of this frame's events painted them. */
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            bool changed = map->tiles[row][col] != st->seen_tiles[row][col];
            bool revealed = st->seen_hidden[row][col] && !tile_hidden(map, row, col);
            if (changed || revealed)
                memset(st->layers[row][col], 0, EDGE_MAX_LAYERS);
        }
    }

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t old_tile = st->seen_tiles[row][col];
            uint8_t tile = map->tiles[row][col];
            if (tile == old_tile) continue;
            if (tile == 0x84 && old_tile != 0x85 && old_tile != 0xA4) {
                map_edges_draw_lit(st, map, row, col);
            } else if (tile == '0' && map_edges_tile_is_dig_source(old_tile)) {
                map_edges_draw_around(st, map, row, col);
            }
        }
    }

    snapshot(st, map);
}

int map_edges_tile_layers(const EdgeState *st, int row, int col, uint8_t *out)
{
    if (!inside(row, col)) return 0;
    int n = 0;
    for (int i = 0; i < EDGE_MAX_LAYERS && st->layers[row][col][i]; i++)
        out[n++] = (uint8_t)(st->layers[row][col][i] - 1);
    return n;
}
