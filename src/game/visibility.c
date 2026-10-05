#include "game/visibility.h"
#include "game/movement.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include <stdlib.h>
#include <string.h>

/* Layer4 bit masks */
#define VIS_HIDDEN_BIT  0x01  /* bit 0: tile not yet revealed */

void visibility_init(TileMap *map)
{
    /* Mark all tiles as hidden (decompiled seg_1010:7436-7449).
     * The original first ORs bit 0 on each tile, then fills the entire
     * array with 1 via FUN_1030_1c8f. We just memset to 1, preserving
     * higher bits (bit 2 = shop gate marker) by OR-ing. */
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            map->layer4[row][col] |= VIS_HIDDEN_BIT;
        }
    }
    visibility_snapshot(map);
}

void visibility_snapshot(TileMap *map)
{
    memcpy(map->seen_tiles, map->tiles, sizeof(map->seen_tiles));
}

void visibility_reveal_changed(TileMap *map)
{
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            if (map->tiles[row][col] != map->seen_tiles[row][col]) {
                map->seen_tiles[row][col] = map->tiles[row][col];
                map->layer4[row][col] &= ~VIS_HIDDEN_BIT;
            }
        }
    }
}

bool visibility_is_revealed(const TileMap *map, int row, int col)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS)
        return false;
    return (map->layer4[row][col] & VIS_HIDDEN_BIT) == 0;
}

void visibility_reveal_tile(TileMap *map, int row, int col)
{
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS)
        return;
    map->layer4[row][col] &= ~VIS_HIDDEN_BIT;
}

/* Bitmap of the set at seg_1000:4A31 (32 bytes, bit n of byte n/8): the
 * tiles a vision ray passes through (FUN_1000_4a51, seg_1000:3131).
 * Floor and most things lying on it are in it; sand, stone, walls,
 * monsters and fire are not. */
static const uint8_t SEE_THROUGH_SET[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x80, 0x03, 0xf8, 0x0f, 0x88, 0xf1,
    0x0f, 0xfc, 0xff, 0xf7, 0xef, 0x8f, 0x30, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

bool visibility_tile_blocks_los(uint8_t tile)
{
    return !((SEE_THROUGH_SET[tile >> 3] >> (tile & 7)) & 1);
}

static int sign_of(int v)
{
    return (v > 0) - (v < 0);
}

/*
 * Cast one vision ray from tile (row, col) toward (dst_row, dst_col),
 * revealing each tile on the way, the one that stops it included.
 *
 * FUN_1000_4a51 (seg_1000:3082-3152) walks the line with a numerator
 * that starts at half the longer side: each step is either diagonal or
 * along the longer axis. The original does it in Pascal reals; every
 * value is a whole number, so integers give the same path.
 */
static void cast_visibility_ray(TileMap *map, int row, int col,
                                int dst_row, int dst_col)
{
    int d_row = dst_row - row;
    int d_col = dst_col - col;
    int diag_row = sign_of(d_row), diag_col = sign_of(d_col);
    int axis_row = sign_of(d_row), axis_col = 0;
    int longest = abs(d_row);
    int shortest = abs(d_col);
    if (!(longest > shortest)) {
        axis_row = 0;
        axis_col = sign_of(d_col);
        longest = abs(d_col);
        shortest = abs(d_row);
    }
    int numerator = longest / 2;

    for (int i = 0; i <= longest; i++) {
        /* The original has no bounds test: the wall ring stops every ray. */
        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS)
            break;

        visibility_reveal_tile(map, row, col);
        if (visibility_tile_blocks_los(map->tiles[row][col]))
            break;

        numerator += shortest;
        if (!(numerator < longest)) {
            numerator -= longest;
            row += diag_row;
            col += diag_col;
        } else {
            row += axis_row;
            col += axis_col;
        }
    }
}

void visibility_reveal_player(TileMap *map, const Player *p)
{
    if (!map || !p || p->dead) return;

    /* Get player tile position.
     * VGA convention: row (first index) = screen X, col (second index) = screen Y.
     * pixel_to_tile_row(x) → row, pixel_to_tile_col(y) → col. */
    int cx = p->x_pos + SPRITE_W / 2;
    int cy = p->y_pos + SPRITE_H / 2;
    int player_row = pixel_to_tile_row(cx);
    int player_col = pixel_to_tile_col(cy);

    /* No separate self/neighbor reveal here: the original reveals the
     * player's own tile as each ray's first step, and the neighbors via
     * the forward walk below (FUN_1000_4d25 has no 3x3 block). */

    /* Determine view cone parameters based on facing direction.
     * Decompiled ref: FUN_1000_4d25 (seg_1000:3193-3219).
     *
     * VGA convention: row = screen X, col = screen Y.
     * DOWN/UP move along col axis (screen Y), LEFT/RIGHT along row axis (screen X).
     * The cone extends in the facing direction, and rays sweep perpendicular to it.
     *
     * DIR_ values use the original encoding: RIGHT=1, LEFT=2, UP=3, DOWN=4
     *.
     */
    int cone_dr, cone_dc; /* direction the cone extends (per ray) */
    int sweep_start_row, sweep_start_col;

    /* The original reads the CURRENT direction (+0xA4, FUN_1000_4d25
     * local_68) and returns without revealing anything when it is not
     * 1-4 — a stopped player casts no fan (seg_1000:3212-3214). */
    int dir = p->direction;
    if (dir == DIR_STOP) return;

    switch (dir) {
    case DIR_DOWN:   /* cone extends +col (screen Y), sweep along row axis */
        cone_dr = 1; cone_dc = 0;
        sweep_start_row = player_row - 20;
        sweep_start_col = player_col + 20;
        break;
    case DIR_UP:     /* cone extends -col (screen Y), sweep along row axis */
        cone_dr = 1; cone_dc = 0;
        sweep_start_row = player_row - 20;
        sweep_start_col = player_col - 20;
        break;
    case DIR_LEFT:   /* cone extends -row (screen X), sweep along col axis */
        cone_dr = 0; cone_dc = 1;
        sweep_start_row = player_row - 20;
        sweep_start_col = player_col - 20;
        break;
    case DIR_RIGHT:  /* cone extends +row (screen X), sweep along col axis */
        cone_dr = 0; cone_dc = 1;
        sweep_start_row = player_row + 20;
        sweep_start_col = player_col - 20;
        break;
    default:
        return;
    }

    /* Only the start of the sweep is clamped, and only at zero
     * (seg_1000:3220-3228); targets past the far edges stay where they
     * are, which keeps the fan's angle. */
    if (sweep_start_row < 0) sweep_start_row = 0;
    if (sweep_start_col < 0) sweep_start_col = 0;

    /* Cast 40 rays (0x28) in the view cone fan.
     * Decompiled ref: seg_1000:3230-3238. */
    int ray_row = sweep_start_row;
    int ray_col = sweep_start_col;

    for (int i = 0; i < 40; i++) {
        cast_visibility_ray(map, player_row, player_col, ray_row, ray_col);

        /* Advance ray target along the sweep */
        ray_row += cone_dr;
        ray_col += cone_dc;
    }

    /* Second pass: walk forward from player along the facing direction,
     * revealing 8 neighbors at each step through passable tiles.
     * Decompiled ref: seg_1000:3240-3305. */
    int walk_dr = 0, walk_dc = 0;
    switch (dir) {
    case DIR_DOWN:  walk_dc =  1; break;  /* screen Y+ → col+ */
    case DIR_UP:    walk_dc = -1; break;  /* screen Y- → col- */
    case DIR_LEFT:  walk_dr = -1; break;  /* screen X- → row- */
    case DIR_RIGHT: walk_dr =  1; break;  /* screen X+ → row+ */
    }

    int wr = player_row;
    int wc = player_col;

    while (wr >= 0 && wr < MAP_ROWS && wc >= 0 && wc < MAP_COLS) {
        uint8_t tile = map->tiles[wr][wc];
        if (tile != '0' && tile != 'f' && tile != 0xAF)
            break;

        /* Reveal all 8 neighbors (decompiled local_10e cases 1-8) */
        visibility_reveal_tile(map, wr, wc - 1);      /* 1: left */
        visibility_reveal_tile(map, wr, wc + 1);      /* 2: right */
        visibility_reveal_tile(map, wr + 1, wc);      /* 3: below */
        visibility_reveal_tile(map, wr - 1, wc);      /* 4: above */
        visibility_reveal_tile(map, wr - 1, wc - 1);  /* 5: top-left */
        visibility_reveal_tile(map, wr + 1, wc + 1);  /* 6: bottom-right */
        visibility_reveal_tile(map, wr + 1, wc - 1);  /* 7: bottom-left */
        visibility_reveal_tile(map, wr - 1, wc + 1);  /* 8: top-right */

        wr += walk_dr;
        wc += walk_dc;
    }
}
