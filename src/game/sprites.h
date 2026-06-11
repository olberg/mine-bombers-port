#ifndef SPRITES_H
#define SPRITES_H

#include "raylib.h"
#include <stdint.h>
#include <stdbool.h>

#define SPRITE_W 10
#define SPRITE_H 10

/*
 * Sprite system: loads SIKA.SPY as a texture atlas and provides
 * source rectangle lookups for each tile byte value.
 *
 * The original extracts 10x10 sprites from known (x, y) positions
 * in the sprite sheet via load_tile_resources()/capture_screen_region().
 */

/* Initialize sprite system: load SIKA.SPY, build lookup table. */
bool sprites_init(void);

/* SIKA.SPY's palette (768 bytes, 8-bit RGB) — the in-game palette.
 * Valid after sprites_init(); round_init installs it into the palette
 * module so in-round HUD draws use the right colors. */
const uint8_t *sprites_get_palette(void);

/* Clean up sprite system. */
void sprites_cleanup(void);

/* Get the source rectangle in the atlas for a tile byte value.
 * Returns floor ('0') rectangle if tile has no sprite. */
Rectangle sprites_get_tile_rect(uint8_t tile_byte);

/* Draw a tile sprite at the given native-resolution position. */
void sprites_draw_tile(uint8_t tile_byte, int x, int y);

/* Get the atlas texture (for batch drawing). */
Texture2D sprites_get_atlas(void);

/* Check if a tile byte has a valid sprite mapping. */
bool sprites_has_tile(uint8_t tile_byte);

/*
 * Player sprite support.
 * Player sprites are 10x10, arranged horizontally in the sheet:
 *   X = x_base + direction * 40 + frame * 10
 *   Y = y_base
 * Sets 0-3: WALK sets for players 1-4
 * Sets 4-7: DIG sets for players 1-4 (drawn while pushing a diggable wall)
 * Sets 8-11: Monster sprite variants
 * Mapping: walk variant = player_index, dig variant = 4 + player_index
 */
#define PLAYER_SPRITE_DIRS   4   /* right, left, up, down */
#define PLAYER_SPRITE_FRAMES 4

/* Get source rectangle for a player sprite.
 * color_variant: 0-11 (12 color sets)
 * direction: sprite sheet band index (0=right, 1=left, 2=up, 3=down)
 * frame: 0-3 animation frame */
Rectangle sprites_get_player_rect(int color_variant, int direction, int frame);

#endif
