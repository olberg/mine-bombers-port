#include "sprite_sheet.h"
#include <stdlib.h>

Image ExtractSprite(const uint8_t *pixels, int sheet_w, int sheet_h,
                    int x, int y, int w, int h,
                    const uint8_t palette[768])
{
    Image img = { 0 };
    if (!pixels || !palette) return img;
    if (x < 0 || y < 0 || x + w > sheet_w || y + h > sheet_h) return img;
    if (w <= 0 || h <= 0) return img;

    uint8_t *rgba = malloc(w * h * 4);
    if (!rgba) return img;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t idx = pixels[(y + row) * sheet_w + (x + col)];
            int di = (row * w + col) * 4;
            rgba[di + 0] = palette[idx * 3 + 0];
            rgba[di + 1] = palette[idx * 3 + 1];
            rgba[di + 2] = palette[idx * 3 + 2];
            rgba[di + 3] = (idx == 0) ? 0 : 255; /* index 0 = transparent */
        }
    }

    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}
