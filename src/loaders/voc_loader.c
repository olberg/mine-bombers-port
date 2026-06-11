#include "voc_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

Sound LoadVOC(const char *path)
{
    Sound sound = {0};

    FILE *f = fopen(path, "rb");
    if (!f) return sound;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return sound;
    }

    /* Read raw unsigned 8-bit PCM */
    uint8_t *raw = malloc(size);
    if (!raw) {
        fclose(f);
        return sound;
    }
    size_t nread = fread(raw, 1, size, f);
    fclose(f);

    if ((long)nread != size) {
        free(raw);
        return sound;
    }

    /* Convert unsigned 8-bit to signed 16-bit: (byte - 128) << 8
     * This matches the original: *pbVar1 - 0x80 */
    int16_t *pcm = malloc(size * sizeof(int16_t));
    if (!pcm) {
        free(raw);
        return sound;
    }
    for (long i = 0; i < size; i++) {
        pcm[i] = (int16_t)(raw[i] - 128) << 8;
    }
    free(raw);

    /* Build Raylib Wave and load as Sound */
    Wave wave = {0};
    wave.frameCount = (unsigned int)size;
    wave.sampleRate = 11025;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm;

    sound = LoadSoundFromWave(wave);

    free(pcm);
    return sound;
}
