#ifndef SPY_LOADER_H
#define SPY_LOADER_H

#include "raylib.h"
#include <stdint.h>
#include <stdbool.h>

#define SPY_WIDTH  640
#define SPY_HEIGHT 480

/* Load a .SPY image file (640x480, 16-color, 4 bitplanes).
 * Returns a 640x480 RGBA Image and fills palette_out with the raw palette (768 bytes).
 * Also fills indexed_out (if non-NULL) with 640x480 bytes of palette indices (0-15).
 * On failure, returns an Image with width=0. */
Image LoadSPY(const char *path, uint8_t palette_out[768], uint8_t *indexed_out);

#endif
