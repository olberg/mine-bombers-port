#include "map.h"
#include "game/bombs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "util/prng.h"

/* Map file format: 45 lines of 64 bytes each, CRLF terminated.
 * Each line = one column: line N = column N, bytes = rows 0-63.
 * Total: 45 * (64 + 2) = 2970 bytes. */
#define MAP_LINE_LEN (MAP_ROWS + 2)  /* 64 data bytes + CR + LF */
#define MAP_FILE_SIZE (MAP_COLS * MAP_LINE_LEN)  /* 2970 */

bool map_load(TileMap *map, const char *path)
{
    if (!map || !path) return false;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    uint8_t buf[MAP_FILE_SIZE];
    size_t n = fread(buf, 1, MAP_FILE_SIZE, f);
    fclose(f);

    if (n != MAP_FILE_SIZE) return false;

    /* Parse: each line is a column (col), bytes within line are rows */
    memset(map, 0, sizeof(*map));
    for (int col = 0; col < MAP_COLS; col++) {
        const uint8_t *line = buf + col * MAP_LINE_LEN;
        for (int row = 0; row < MAP_ROWS; row++) {
            map->tiles[row][col] = line[row];
        }
    }

    return true;
}

/* Initialize collision HP values from tile types.
 * Exact values from decompiled seg_1010:7319-7398.
 * Each tile type has a specific HP that determines destructibility. */
void map_init_collision(TileMap *map)
{
    if (!map) return;

    /* Initialize overlay map. Proximity mines ('o' / 0x6F) preplaced on the
     * map get a random startup fuse in [0, 29]; everything else starts at 0.
     * Decompiled ref: FUN_1010_c5f4 (seg_1010:7292-7319), called at each
     * round start before the shop. Without this, preplaced proximity mines
     * never arm on round start. */
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            if (map->tiles[row][col] == 'o') {
                map->overlay[row][col] = (uint16_t)mb_random(30);
            } else {
                map->overlay[row][col] = 0;
            }
        }
    }
    memset(map->bomb_owner, 0xFF, sizeof(map->bomb_owner));
    memset(map->layer4, 0, sizeof(map->layer4));

    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = map->tiles[row][col];
            uint16_t hp = 0;

            switch (tile) {
            case '1': hp = 30000; break;      /* indestructible */
            case '2': hp = 0x16;  break;      /* 22 - fragile deco */
            case '3': hp = 0x17;  break;      /* 23 */
            case '4': hp = 0x18;  break;      /* 24 */
            case '5': hp = 0x6C;  break;      /* 108 - heavily damaged wall */
            case '6': hp = 0x15B; break;      /* 347 - moderately damaged wall */
            case '7': hp = 0x4CB; break;      /* 1227 - standard wall */
            case '8': hp = 0x4CB; break;      /* 1227 */
            case '9': hp = 0x4CB; break;      /* 1227 */
            case 'A': hp = 0x4CB; break;      /* 1227 */
            case 'B': hp = 0x18;  break;      /* 24 - fragile wall */
            case 'C': hp = 2000;  break;
            case 'D': hp = 0x866; break;      /* 2150 */
            case 'E': hp = 0x898; break;      /* 2200 */
            case 'F': hp = 0x834; break;      /* 2100 */
            case 0x9B: hp = 400;  break;      /* special tile */
            case 'o':  hp = 400;  break;      /* proximity mine preplaced */
            case 'p':  hp = 1000; break;      /* damaged wall stage */
            case 'q':  hp = 500;  break;      /* heavily damaged wall stage */
            case 0xAC: hp = 8000; break;      /* reinforced wall */
            case 0xAD: hp = 4000; break;      /* reinforced wall stage 2 */
            case 0xAE: hp = 2000; break;      /* reinforced wall stage 3 */
            default:   hp = 0;    break;      /* passable */
            }

            map->collision[row][col] = hp;
        }
    }
}

uint8_t map_get_tile(const TileMap *map, int row, int col)
{
    if (!map || row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS)
        return '0';  /* 0x30 = empty floor, matches original bounds check */
    return map->tiles[row][col];
}

/* --- Random map generation (FUN_1008_1263, seg_1008_sound.c:643) ---
 *
 * Three stages matching the original:
 *   1. FUN_1008_0073: Fill entire map with floor tiles ('0')
 *   2. FUN_1008_00b4: Place random obstacle clusters ('C' walls)
 *   3. FUN_1008_04bb: Finalize wall types, place treasures/items, add border
 *
 * Coordinate system: col = 0..44 (MAP_COLS), row = 0..63 (MAP_ROWS)
 * Decompiled FUN_1008_0002(param_1, tile, col, row) sets tiles[row][col].
 */

/* Bounds-checked set tile (matching FUN_1008_0002) */
static void rmap_set(TileMap *map, uint8_t tile, int col, int row)
{
    if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
        map->tiles[row][col] = tile;
    }
}

/* Bounds-checked get tile (matching FUN_1008_0034) */
static uint8_t rmap_get(const TileMap *map, int col, int row)
{
    if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
        return map->tiles[row][col];
    }
    return '0';
}

/* RNG matching FUN_1030_19de: returns random int in [0, range) */
static int rmap_random(int range)
{
    if (range <= 0) return 0;
    return mb_random(range);
}

/* Stage 1: Fill map with floor tiles (FUN_1008_0073, seg_1008:42) */
static void rmap_clear(TileMap *map)
{
    for (int col = 0; col <= 0x2c; col++) {
        for (int row = 0; row <= 0x3f; row++) {
            rmap_set(map, '0', col, row);
        }
    }
}

/* Stage 2: Place random obstacle clusters (FUN_1008_00b4, seg_1008:65)
 * Creates 29-40 clusters of 'C' (destructible wall) tiles using random
 * patterns, with a probability-based continuation loop. */
static void rmap_generate_obstacles(TileMap *map)
{
    int num_clusters = rmap_random(0xb) + 0x1d;  /* 29-40 clusters */

    for (; num_clusters > 0; num_clusters--) {
        int row = rmap_random(0x3e) + 1;   /* 1..63 */
        int col = rmap_random(0x2b) + 1;   /* 1..43 */
        bool active = true;

        while (active) {
            int pattern = rmap_random(9);

            switch (pattern) {
            case 0:
                rmap_set(map, 'C', col, row);
                break;
            case 1:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col + 1, row);
                break;
            case 2:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col - 1, row);
                break;
            case 3:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col, row + 1);
                break;
            case 4:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col - 1, row);
                rmap_set(map, 'C', col + 1, row);
                break;
            case 5:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col - 1, row);
                rmap_set(map, 'C', col + 1, row);
                rmap_set(map, 'C', col, row - 1);
                break;
            case 6:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col - 1, row);
                rmap_set(map, 'C', col + 1, row);
                rmap_set(map, 'C', col, row - 1);
                rmap_set(map, 'C', col, row + 1);
                break;
            case 7:
                rmap_set(map, 'C', col, row);
                rmap_set(map, 'C', col - 1, row);
                rmap_set(map, 'C', col + 1, row);
                rmap_set(map, 'C', col, row - 1);
                rmap_set(map, 'C', col, row + 1);
                break;
            case 8:
                rmap_set(map, 'C', col - 1, row);
                rmap_set(map, 'C', col + 1, row);
                rmap_set(map, 'C', col, row - 1);
                rmap_set(map, 'C', col - 1, row - 1);
                rmap_set(map, 'C', col + 1, row + 1);
                rmap_set(map, 'C', col + 1, row - 1);
                rmap_set(map, 'C', col - 1, row + 1);
                break;
            }

            /* Continuation probability: threshold = random(10) + 93 (0x5d)
             * Continue if random(100) < threshold (i.e. ~93% chance) */
            int threshold = rmap_random(10) + 0x5d;
            int roll = rmap_random(100);
            if (threshold < roll) {
                active = false;
            }

            /* Drift the cluster center position, bouncing off edges */
            if (row < 1 || row > 0x3d) {
                row += (row < 1) ? 1 : -1;
            } else {
                row += (1 - rmap_random(3));
            }

            if (col < 1 || col > 0x2a) {
                col += (col < 1) ? 1 : -1;
            } else {
                col += (1 - rmap_random(3));
            }
        }
    }
}

/* Stage 3: Finalize wall types and place items (FUN_1008_04bb, seg_1008:184)
 *
 * 4-pass processing:
 *   Pass 1: Convert isolated 'C' (no neighbors) to 'B' (fragile wall)
 *   Pass 2: Convert 'C' at L-corners to specific wall types ('7','8','9','A')
 *           Also convert '0' at L-corners to corner pieces
 *   Pass 3: Same as pass 2 but with extended pattern matching (refines corners)
 *   Pass 4: Randomize remaining 'C' to 'C'/'D'/'E'/'F' variants;
 *           Randomize remaining '0' to '0'/'2'/'3'/'4' (floor deco)
 *
 * Then: place treasures, scatter bonus items, add indestructible border. */
static void rmap_finalize(TileMap *map, int treasure_count)
{
    for (int pass = 1; pass <= 4; pass++) {
        for (int col = 0; col <= 0x2c; col++) {
            for (int row = 0; row <= 0x3f; row++) {
                uint8_t tile = rmap_get(map, col, row);

                if (tile == 'C') {
                    if (pass == 1) {
                        /* Isolated 'C' surrounded by '0' → 'B' */
                        if (rmap_get(map, col, row + 1) == '0' &&
                            rmap_get(map, col, row - 1) == '0' &&
                            rmap_get(map, col + 1, row) == '0' &&
                            rmap_get(map, col - 1, row) == '0') {
                            rmap_set(map, 'B', col, row);
                        }
                    } else if (pass == 4) {
                        /* Randomize remaining 'C' to C/D/E/F variants */
                        int v = rmap_random(4);
                        rmap_set(map, (uint8_t)('C' + v), col, row);
                    } else if (pass == 2) {
                        /* L-corner detection: check 2 adjacent neighbors
                         * for 'C' and 2 opposite for '0' */
                        uint8_t down  = rmap_get(map, col, row + 1);
                        uint8_t up    = rmap_get(map, col, row - 1);
                        uint8_t right = rmap_get(map, col + 1, row);
                        uint8_t left  = rmap_get(map, col - 1, row);

                        if (down == 'C' && right == 'C' && up == '0' && left == '0')
                            rmap_set(map, '7', col, row);
                        else if (down == 'C' && left == 'C' && up == '0' && right == '0')
                            rmap_set(map, 'A', col, row);
                        else if (up == 'C' && right == 'C' && down == '0' && left == '0')
                            rmap_set(map, '8', col, row);
                        else if (up == 'C' && left == 'C' && down == '0' && right == '0')
                            rmap_set(map, '9', col, row);
                    } else if (pass == 3) {
                        /* Extended corner matching using already-placed wall types */
                        uint8_t down  = rmap_get(map, col, row + 1);
                        uint8_t up    = rmap_get(map, col, row - 1);
                        uint8_t right = rmap_get(map, col + 1, row);
                        uint8_t left  = rmap_get(map, col - 1, row);

                        if (down == '8' && right == 'C' && up == '0' && left == '0')
                            rmap_set(map, '7', col, row);
                        else if (down == 'C' && right == 'A' && up == '0' && left == '0')
                            rmap_set(map, '7', col, row);
                        else if (down == 'C' && left == '7' && up == '0' && right == '0')
                            rmap_set(map, 'A', col, row);
                        else if (down == '9' && left == 'C' && up == '0' && right == '0')
                            rmap_set(map, 'A', col, row);
                        else if (up == 'C' && right == '9' && down == '0' && left == '0')
                            rmap_set(map, '8', col, row);
                        else if (up == '7' && right == 'C' && down == '0' && left == '0')
                            rmap_set(map, '8', col, row);
                        else if (up == 'C' && left == '8' && down == '0' && right == '0')
                            rmap_set(map, '9', col, row);
                        else if (up == 'A' && left == 'C' && down == '0' && right == '0')
                            rmap_set(map, '9', col, row);
                        else if (down == '8' && right == 'A' && up == '0' && left == '0')
                            rmap_set(map, '7', col, row);
                        else if (up == '7' && right == '9' && down == '0' && left == '0')
                            rmap_set(map, '8', col, row);
                        else if (up == 'A' && left == '8' && down == '0' && right == '0')
                            rmap_set(map, '9', col, row);
                        else if (down == '9' && left == '7' && up == '0' && right == '0')
                            rmap_set(map, 'A', col, row);
                    }
                } else if (tile == '0') {
                    if (pass == 2) {
                        /* Floor L-corner detection: same pattern matching for '0' */
                        uint8_t down  = rmap_get(map, col, row + 1);
                        uint8_t up    = rmap_get(map, col, row - 1);
                        uint8_t right = rmap_get(map, col + 1, row);
                        uint8_t left  = rmap_get(map, col - 1, row);

                        if (down == 'C' && right == 'C' && up == '0' && left == '0')
                            rmap_set(map, '7', col, row);
                        else if (down == 'C' && left == 'C' && up == '0' && right == '0')
                            rmap_set(map, 'A', col, row);
                        else if (up == 'C' && right == 'C' && down == '0' && left == '0')
                            rmap_set(map, '8', col, row);
                        else if (up == 'C' && left == 'C' && down == '0' && right == '0')
                            rmap_set(map, '9', col, row);
                    } else if (pass == 4) {
                        /* Randomize floor: 1/3 chance of deco tile */
                        int v = rmap_random(3);
                        if (v == 0) rmap_set(map, '2', col, row);
                        else if (v == 1) rmap_set(map, '3', col, row);
                        else if (v == 2) rmap_set(map, '4', col, row);
                    }
                }
            }
        }
    }

    /* --- Place treasures (seg_1008:443-564) ---
     * treasure_count items are placed, each with a weighted random type.
     * First 21 (0x15) items must land on destructible walls (C/D/E/F);
     * remaining can be placed anywhere. */
    int num_treasures = treasure_count;
    int placed_on_wall = 0;

    /* Weighted treasure type table (cumulative weights from decompiled):
     * index 0: weight 0x12(18)  → 0x8f (stat gem 1)
     * index 1: weight 0x0c(12)  → 0x90 (stat gem 2)
     * index 2: weight 0x08(8)   → 0x91 (stat gem 3)
     * index 3-7: weight 200 each → 0x92-0x96 (money bags)
     * index 8:  weight 0xb4(180) → 0x97
     * index 9:  weight 0xa0(160) → 0x98
     * index 10: weight 0x8c(140) → 0x99
     * index 11: weight 0x50(80)  → 0x9a
     * index 12: weight 3         → 0x73 ('s', gold bar)
     */
    static const int treasure_weights[] = {
        0x12, 0x0c, 0x08, 200, 200, 200, 200, 200, 0xb4, 0xa0, 0x8c, 0x50, 3
    };
    static const uint8_t treasure_tiles[] = {
        0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x73
    };

    for (int i = 1; i <= num_treasures && num_treasures > 0; i++) {
        /* Pick treasure type using weighted random selection */
        int total_weight = 0x641;  /* 1601 = sum of weights */
        int roll = rmap_random(total_weight);
        int cumul = 0;
        int type_idx = 0;
        for (int j = 0; j < 13; j++) {
            int prev = cumul;
            cumul += treasure_weights[j];
            if (roll >= prev && roll < cumul) {
                type_idx = j + 1;
                break;
            }
        }
        /* Decompiled uses 1-based index; map to tile */
        uint8_t treasure_tile;
        if (type_idx >= 1 && type_idx <= 13)
            treasure_tile = treasure_tiles[type_idx - 1];
        else
            treasure_tile = 0x73;  /* fallback: gold bar */

        /* Find placement position */
        int row = rmap_random(0x40);    /* 0..63 */
        int col = rmap_random(0x2d);    /* 0..44 */

        if (placed_on_wall < 0x15) {
            /* First 21 treasures must go on destructible walls */
            int attempts = 0;
            while (attempts < 0xb40) {
                uint8_t t = rmap_get(map, col, row);
                if (t == 'C' || t == 'D' || t == 'E' || t == 'F')
                    break;
                if (row < 0x3f) {
                    row++;
                } else {
                    row = 0;
                    col++;
                }
                if (col > 0x2c) {
                    row = rmap_random(0x40);
                    col = rmap_random(col);
                }
                attempts++;
            }
            rmap_set(map, treasure_tile, col, row);
            placed_on_wall++;
        } else {
            rmap_set(map, treasure_tile, col, row);
        }
    }

    /* --- Place ground deco inside walls (seg_1008:565-595) ---
     * 300 iterations: find a deco tile ('2'/'3'/'4'), replace with '5' or '6'. */
    for (int i = 1; i <= 300; i++) {
        int row = rmap_random(0x40);
        int col = rmap_random(0x2d);
        int attempts = 0;
        while (attempts < 0xb40) {
            uint8_t t = rmap_get(map, col, row);
            if (t == '2' || t == '3' || t == '4')
                break;
            if (row < 0x3f) {
                row++;
            } else {
                row = 0;
                col++;
            }
            if (col > 0x2c) {
                row = rmap_random(0x40);
                col = rmap_random(col);
            }
            attempts++;
        }
        int v = rmap_random(2);
        rmap_set(map, (v == 0) ? '6' : '5', col, row);
    }

    /* --- Scatter extra 'B' walls (seg_1008:596-601) ---
     * While random(100) > 70 (0x46), place a 'B' wall */
    while (rmap_random(100) > 0x46) {
        int row = rmap_random(0x3f) + 1;
        int col = rmap_random(0x2c) + 1;
        rmap_set(map, 'B', col, row);
    }

    /* --- Scatter mystery boxes 'y'/0x79 (seg_1008:602-607) ---
     * While random(100) > 70, place a mystery box */
    while (rmap_random(100) > 0x46) {
        int row = rmap_random(0x3f) + 1;
        int col = rmap_random(0x2c) + 1;
        rmap_set(map, 0x79, col, row);
    }

    /* --- Scatter health restore 'm'/0x6d (seg_1008:608-613) ---
     * While random(100) > 65 (0x41), place a health tile */
    while (rmap_random(100) > 0x41) {
        int row = rmap_random(0x3f) + 1;
        int col = rmap_random(0x2c) + 1;
        rmap_set(map, 0x6d, col, row);
    }

    /* --- Scatter teleporters 0x9C (seg_1008:614-623) ---
     * While random(100) > 70, place TWO teleporter tiles per iteration */
    while (rmap_random(100) > 0x46) {
        int row = rmap_random(0x3f) + 1;
        int col = rmap_random(0x2c) + 1;
        rmap_set(map, 0x9c, col, row);
        row = rmap_random(0x3f) + 1;
        col = rmap_random(0x2c) + 1;
        rmap_set(map, 0x9c, col, row);
    }

    /* --- Add indestructible border '1' (seg_1008:624-637) --- */
    /* Top and bottom rows */
    for (int col = 0; col <= 0x2c; col++) {
        rmap_set(map, '1', col, 0);
        rmap_set(map, '1', col, 0x3f);
    }
    /* Left and right columns */
    for (int row = 0; row <= 0x3f; row++) {
        rmap_set(map, '1', 0, row);
        rmap_set(map, '1', 0x2c, row);
    }
}

void map_generate_random(TileMap *map, int treasure_count)
{
    if (!map) return;
    memset(map, 0, sizeof(*map));

    /* Pascal Randomize before each generation (seg_1008:77 calls
     * FUN_1030_1a73 = clock reseed). NOT mb_prng_init — that re-reads
     * MB_SEED and made every random map of a session identical. */
    mb_prng_randomize();

    rmap_clear(map);
    rmap_generate_obstacles(map);
    rmap_finalize(map, treasure_count);
}

/*
 * Shop gate toggle: open gates (FUN_1010_1019, seg_1010:642-682).
 * Triggered when player steps on 0xB4 tile.
 * Scans entire map: 0xB4 → 0xB5 (with cooldown), 'l' → '0' (floor).
 */
void map_shop_toggle_open(TileMap *map)
{
    if (!map) return;
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = map->tiles[row][col];
            if (tile == 0xB4 || tile == 'l') {
                if (tile == 0xB4) {
                    map->overlay[row][col] = 0x28;
                    map->tiles[row][col] = 0xB5;
                }
                if (map->tiles[row][col] == 'l' || tile == 'l') {
                    map->tiles[row][col] = '0';
                    map->layer4[row][col] |= 2;
                }
            }
        }
    }
}

/*
 * Shop gate toggle: close gates (FUN_1010_1156, seg_1010:686-728).
 * Triggered when player steps on 0xB5 tile.
 * Scans entire map: 0xB5 → 0xB4 (with cooldown), layer4-bit-2 → 'l' (gate).
 *
 * Gate closure has a random detonation chance (seg_1010:707-714):
 * when restoring gate tiles, there's a probability that whatever tile
 * currently occupies the gate position gets detonated first (e.g. if a
 * player placed a bomb on the opened gate). The detonation uses the
 * FUN_1010_165b handler. The original tests (DS:0x1136 & Random()) != 0;
 * we approximate with mb_random(2) (~50% probability).
 */
void map_shop_toggle_close(TileMap *map)
{
    if (!map) return;
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = map->tiles[row][col];
            if (tile == 0xB5 || (map->layer4[row][col] & 2)) {
                if (tile == 0xB5) {
                    map->overlay[row][col] = 0x28;
                    map->tiles[row][col] = 0xB4;
                }
                if (map->layer4[row][col] & 2) {
                    /* Random detonation chance (seg_1010:708-714):
                     * Original tests (DS:0x1136 & Random()) != 0 then calls
                     * FUN_1010_165b on the current tile. We approximate with
                     * mb_random(2) (~50% probability). */
                    if (mb_random(2) != 0) {
                        bomb_detonate(map, col, row);
                    }
                    map->tiles[row][col] = 'l';
                }
            }
        }
    }
}

/*
 * Sum the cash value of all treasures still on the map.
 * Port of FUN_1000_6cd5 (seg_1000:4216-4282): scans the full 64x45 grid,
 * adding the pickup value of every treasure tile ('s' = 0x73 worth 1000,
 * 0x92-0x9A worth 15/25/15/10/30/35/50/65/100 ? same table as the pickup
 * handler at seg_1000:3421-3461).
 */
int32_t map_treasure_value_remaining(const TileMap *map)
{
    int32_t sum = 0;
    if (!map) return 0;
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            switch (map->tiles[row][col]) {
            case 0x92: sum += 15;   break;
            case 0x93: sum += 25;   break;
            case 0x94: sum += 15;   break;
            case 0x95: sum += 10;   break;
            case 0x96: sum += 30;   break;
            case 0x97: sum += 35;   break;
            case 0x98: sum += 50;   break;
            case 0x99: sum += 65;   break;
            case 0x9A: sum += 100;  break;
            case 's':  sum += 1000; break;
            default: break;
            }
        }
    }
    return sum;
}
