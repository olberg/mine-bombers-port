#ifndef FONT_LOADER_H
#define FONT_LOADER_H

#include "raylib.h"

typedef struct {
    Texture2D atlas;   /* 128x128 texture (16x16 grid of 8x8 glyphs) */
    Image atlas_img;   /* Keep image for non-GPU contexts (tests) */
    int glyph_w;       /* 8 */
    int glyph_h;       /* 8 */
} BitmapFont;

/* Load a .FON file (2048 bytes: 256 chars x 8 bytes each, 8x8 1bpp MSB-left).
 * If gpu is true, also creates atlas texture. If false, only creates atlas_img. */
BitmapFont LoadFON(const char *path, bool gpu);

/* Unload font resources. */
void UnloadFON(BitmapFont *font);

/* Draw text using the bitmap font with color tinting. Requires GPU (atlas texture). */
void DrawTextFON(BitmapFont *font, const char *text, int x, int y, Color color);

#endif
