#ifndef MAP_H
#define MAP_H

#include <stdint.h>
#include <stdbool.h>

#define MAP_COLS     45
#define MAP_ROWS     64
#define TILE_SIZE    10
/* Original VGA convention: rows (0-63) map to screen X, cols (0-44) map to
 * screen Y.  The 30-pixel offset is on the Y-axis (pushes the map below the
 * HUD), not on X.  Confirmed via draw_map_tile → blit_sprite parameter order
 * analysis: blit_sprite(page, sprite, seg, Y, X) where Y = col*10+30 and
 * X = row*10.  See decompiled seg_1010:4880-4910, 5290-5298. */
#define MAP_Y_OFFSET 30

typedef struct {
    uint8_t  tiles[MAP_ROWS][MAP_COLS];
    uint16_t collision[MAP_ROWS][MAP_COLS];
    uint16_t overlay[MAP_ROWS][MAP_COLS];
    uint8_t  bomb_owner[MAP_ROWS][MAP_COLS]; /* player index (0-3) who placed bomb; 0xFF = none */
    uint8_t  layer4[MAP_ROWS][MAP_COLS];     /* per-tile flags (g_map_layer4): bit 2 = shop gate marker */
    int      screen_shake;  /* shake counter (seg_1010:7705-7725): decrements each frame,
                             * odd values offset render by shake_value pixels vertically */
    int      palette_flash; /* palette flash counter: >0 means render white overlay.
                             * Used by mine detonation (set_palette_to_white, seg_1010:1213).
                             * Decrements each frame; while >0, draw white overlay at full alpha. */
    bool     darkness_enabled; /* runtime mirror of g_minimap_enabled (seg_1010:7197-7202):
                                * SP always on, MP per Darkness option. Gates the
                                * movement-driven visibility reveal. */
} TileMap;

/* Load a .MNE or .MNL map file. Returns true on success. */
bool map_load(TileMap *map, const char *path);

/* Initialize collision HP values from tile types. Call after map_load. */
void map_init_collision(TileMap *map);

/* Bounds-checked tile access. Returns '0' (0x30) if out of bounds. */
uint8_t map_get_tile(const TileMap *map, int row, int col);

/* Generate a random map procedurally (FUN_1008_1263).
 * treasure_count = number of treasure items to place (from g_config_byte).
 * Call map_init_collision() after this. */
void map_generate_random(TileMap *map, int treasure_count);

/* Shop gate toggle functions (seg_1010:642-728).
 * Maps use 'l' tiles as gates and 0xB4/0xB5 as toggle switches.
 * Stepping on 0xB4 opens gates: 0xB4→0xB5, 'l'→'0' (floor).
 * Stepping on 0xB5 closes gates: 0xB5→0xB4, floor→'l'.
 * Overlay is set to 0x28 (40 frames) as cooldown. */
void map_shop_toggle_open(TileMap *map);
void map_shop_toggle_close(TileMap *map);

/* Total cash value of treasures still on the map ('s' and 0x92-0x9A).
 * Port of FUN_1000_6cd5 (seg_1000:4216-4282). Used by round_apply_scoring
 * for the sole-survivor bonus. */
int32_t map_treasure_value_remaining(const TileMap *map);

#endif
