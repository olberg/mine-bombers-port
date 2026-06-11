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

bool visibility_tile_blocks_los(uint8_t tile)
{
    /* Passable tiles don't block LOS.
     * From decompiled FUN_1000_4a51 (seg_1000:3131):
     * the ray checks a visibility-blocking table — tiles that are
     * passable ('0', 'f', 0xAF) and most pickups/items don't block.
     * Walls ('1', '5'-'9', 'A'-'F', 'p', 'q', 0xAC-0xAE, 'l') block. */
    switch (tile) {
    case '0': case 'f': case 0xAF:  /* floor, decorative floor */
        return false;
    case '2': case '3': case '4':  /* floor variants */
        return false;
    case '5': case '6':  /* damaged walls (partially see-through) — still block */
        return true;
    case 'k':  /* exit tile */
        return false;
    default:
        break;
    }

    /* Walls block */
    if (tile == '1') return true;  /* indestructible */
    if (tile >= '7' && tile <= '9') return true;
    if (tile >= 'A' && tile <= 'F') return true;
    if (tile == 'p' || tile == 'q') return true;
    if (tile == 'l') return true;  /* gate */
    if (tile >= 0xAC && tile <= 0xAE) return true;  /* reinforced */
    if (tile == 'B') return true;

    /* Items, bombs, treasures, pickups, effects: don't block */
    return false;
}

/*
 * Cast a single visibility ray from (src_col, src_row) toward (dst_col, dst_row).
 * Uses Bresenham line algorithm. Reveals each tile along the way.
 * Stops when hitting a wall that blocks LOS.
 *
 * Decompiled ref: FUN_1000_4a51 (seg_1000:3082-3152).
 */
static void cast_visibility_ray(TileMap *map, int src_col, int src_row,
                                 int dst_col, int dst_row)
{
    int dx = abs(dst_col - src_col);
    int dy = abs(dst_row - src_row);
    int sx = (src_col < dst_col) ? 1 : -1;
    int sy = (src_row < dst_row) ? 1 : -1;
    int err = dx - dy;

    int col = src_col;
    int row = src_row;

    /* Maximum number of steps to prevent infinite loops */
    int max_steps = dx + dy + 1;

    for (int step = 0; step < max_steps; step++) {
        /* Bounds check */
        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS)
            break;

        /* Reveal this tile */
        visibility_reveal_tile(map, row, col);

        /* Check if this tile blocks further visibility */
        uint8_t tile = map->tiles[row][col];
        if (visibility_tile_blocks_los(tile))
            break;

        /* Reached destination */
        if (col == dst_col && row == dst_row)
            break;

        /* Bresenham step */
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            col += sx;
        }
        if (e2 < dx) {
            err += dx;
            row += sy;
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

    /* Clamp starting positions */
    if (sweep_start_row < 0) sweep_start_row = 0;
    if (sweep_start_col < 0) sweep_start_col = 0;

    /* Cast 40 rays (0x28) in the view cone fan.
     * Decompiled ref: seg_1000:3230-3238. */
    int ray_row = sweep_start_row;
    int ray_col = sweep_start_col;

    for (int i = 0; i < 40; i++) {
        /* Clamp ray target to map bounds */
        int tr = ray_row;
        int tc = ray_col;
        if (tr < 0) tr = 0;
        if (tr >= MAP_ROWS) tr = MAP_ROWS - 1;
        if (tc < 0) tc = 0;
        if (tc >= MAP_COLS) tc = MAP_COLS - 1;

        cast_visibility_ray(map, player_col, player_row, tc, tr);

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
