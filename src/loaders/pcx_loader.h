#ifndef PCX_LOADER_H
#define PCX_LOADER_H

#include "raylib.h"

/* Load a PCX file (.PPM portrait). Returns empty Image on failure.
 * All pixels are opaque (alpha=255). If the 256-color palette at EOF is
 * missing, falls back to the 16-color EGA palette in the PCX header. */
Image LoadPCX(const char *path);

#endif
