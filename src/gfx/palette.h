#ifndef PALETTE_H
#define PALETTE_H

#include "raylib.h"
#include <stdint.h>
#include <stdbool.h>

/* Initialize palette system with a raw 6-bit VGA palette (768 bytes, values 0-63). */
void palette_init(const uint8_t raw_palette[768]);

/* Start fading from black to the target palette over 'steps' frames. */
void palette_start_fade_in(int steps);

/* Start fading from the current palette to black over 'steps' frames. */
void palette_start_fade_out(int steps);

/* Returns true if a fade transition is in progress. */
bool palette_is_fading(void);

/* Advance one fade step. Call once per frame. */
void palette_update(void);

/* Get the current (possibly fading) color for a palette index. */
Color palette_get_color(uint8_t index);

/* Apply the current palette to indexed pixels, writing RGBA to dst.
 * src_indexed: 320*240 bytes of palette indices
 * dst_rgba: 320*240*4 bytes output */
void palette_apply_to_pixels(const uint8_t *src_indexed, uint8_t *dst_rgba, int pixel_count);

#endif
