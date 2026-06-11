#ifndef SPRITE_SHEET_H
#define SPRITE_SHEET_H

#include "raylib.h"
#include <stdint.h>

/* Extract a sub-region from indexed pixel data and convert to RGBA Image.
 * pixels: indexed pixel buffer (sheet_w * sheet_h bytes)
 * palette: 768-byte VGA 6-bit palette */
Image ExtractSprite(const uint8_t *pixels, int sheet_w, int sheet_h,
                    int x, int y, int w, int h,
                    const uint8_t palette[768]);

#endif
