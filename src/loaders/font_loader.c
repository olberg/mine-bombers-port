#include "font_loader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FON_FILE_SIZE 2048
#define CHARS_TOTAL   256
#define GLYPH_W       8
#define GLYPH_H       8
#define ATLAS_COLS    16
#define ATLAS_ROWS    16
#define ATLAS_W       (ATLAS_COLS * GLYPH_W)  /* 128 */
#define ATLAS_H       (ATLAS_ROWS * GLYPH_H)  /* 128 */

BitmapFont LoadFON(const char *path, bool gpu)
{
    BitmapFont font = { 0 };
    font.glyph_w = GLYPH_W;
    font.glyph_h = GLYPH_H;

    FILE *f = fopen(path, "rb");
    if (!f) return font;

    uint8_t raw[FON_FILE_SIZE];
    if (fread(raw, 1, FON_FILE_SIZE, f) != FON_FILE_SIZE) {
        fclose(f);
        return font;
    }
    fclose(f);

    /* Build RGBA atlas: white pixels where bit is set, transparent where not */
    uint8_t *rgba = calloc(ATLAS_W * ATLAS_H * 4, 1);
    if (!rgba) return font;

    for (int ch = 0; ch < CHARS_TOTAL; ch++) {
        int grid_x = (ch % ATLAS_COLS) * GLYPH_W;
        int grid_y = (ch / ATLAS_COLS) * GLYPH_H;

        for (int row = 0; row < GLYPH_H; row++) {
            uint8_t bits = raw[ch * GLYPH_H + row];
            for (int col = 0; col < GLYPH_W; col++) {
                if (bits & (0x80 >> col)) {
                    int px = grid_x + col;
                    int py = grid_y + row;
                    int idx = (py * ATLAS_W + px) * 4;
                    rgba[idx + 0] = 255;
                    rgba[idx + 1] = 255;
                    rgba[idx + 2] = 255;
                    rgba[idx + 3] = 255;
                }
                /* else: stays (0,0,0,0) = transparent */
            }
        }
    }

    font.atlas_img.data = rgba;
    font.atlas_img.width = ATLAS_W;
    font.atlas_img.height = ATLAS_H;
    font.atlas_img.mipmaps = 1;
    font.atlas_img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    if (gpu) {
        font.atlas = LoadTextureFromImage(font.atlas_img);
    }

    return font;
}

void UnloadFON(BitmapFont *font)
{
    if (font->atlas_img.data) {
        UnloadImage(font->atlas_img);
        font->atlas_img.data = NULL;
    }
    if (font->atlas.id > 0) {
        UnloadTexture(font->atlas);
        font->atlas.id = 0;
    }
}

void DrawTextFON(BitmapFont *font, const char *text, int x, int y, Color color)
{
    if (!text || font->atlas.id == 0) return;

    int cx = x;
    for (const char *p = text; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        int grid_x = (ch % ATLAS_COLS) * GLYPH_W;
        int grid_y = (ch / ATLAS_COLS) * GLYPH_H;

        Rectangle src = { (float)grid_x, (float)grid_y, (float)GLYPH_W, (float)GLYPH_H };
        Rectangle dst = { (float)cx, (float)y, (float)GLYPH_W, (float)GLYPH_H };
        DrawTexturePro(font->atlas, src, dst, (Vector2){0, 0}, 0.0f, color);
        cx += GLYPH_W;
    }
}
