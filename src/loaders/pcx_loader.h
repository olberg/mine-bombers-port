#ifndef PCX_LOADER_H
#define PCX_LOADER_H

#include "raylib.h"

/* Load a PCX file (.PPM portrait). Returns empty Image on failure.
 * The image is one column wider than the PCX: the extra column holds the
 * pixel the original's run drawing leaves just right of each row that ends
 * in a run (transparent on other rows). Every other pixel is opaque.
 * If the 256-color palette at EOF is
 * missing, falls back to the 16-color EGA palette in the PCX header.
 * Pixel indices are reduced to 4 bits (the original draws in a 16-colour
 * planar mode). */
Image LoadPCX(const char *path);

#endif
