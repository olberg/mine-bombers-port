#ifndef VOC_LOADER_H
#define VOC_LOADER_H

#include "raylib.h"

/* Load a raw unsigned 8-bit PCM file (the original .VOC files have no header).
 * Converts to signed 16-bit PCM at 11025 Hz mono, returns a Raylib Sound.
 * On failure, returns a Sound with frameCount == 0. */
Sound LoadVOC(const char *path);

#endif
