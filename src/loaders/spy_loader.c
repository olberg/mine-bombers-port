#include "spy_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLANE_SIZE    38400  /* 80 bytes/row * 480 rows */
#define PALETTE_BYTES 768
#define NUM_PLANES    4
#define BYTES_PER_ROW 80     /* 640 pixels / 8 bits per byte */

/* Decode one RLE plane from src into dest (PLANE_SIZE bytes).
 * Returns number of bytes consumed from src, or 0 on error. */
static size_t decode_plane(const uint8_t *src, size_t src_len, uint8_t *dest)
{
    size_t si = 0;
    size_t di = 0;

    while (di < PLANE_SIZE && si < src_len) {
        uint8_t b = src[si++];
        if (b == 0x01) {
            if (si + 2 > src_len) return 0;
            uint8_t value = src[si++];
            uint8_t count = src[si++];
            for (int i = 0; i < count && di < PLANE_SIZE; i++) {
                dest[di++] = value;
            }
        } else {
            dest[di++] = b;
        }
    }
    return si;
}

Image LoadSPY(const char *path, uint8_t palette_out[768], uint8_t *indexed_out)
{
    Image img = { 0 };

    FILE *f = fopen(path, "rb");
    if (!f) return img;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < PALETTE_BYTES + 4) {
        fclose(f);
        return img;
    }

    /* Read palette (256 entries x 3 bytes, 8-bit RGB) */
    uint8_t palette[PALETTE_BYTES];
    if (fread(palette, 1, PALETTE_BYTES, f) != PALETTE_BYTES) {
        fclose(f);
        return img;
    }
    if (palette_out) {
        memcpy(palette_out, palette, PALETTE_BYTES);
    }

    /* Read compressed data */
    size_t data_size = file_size - PALETTE_BYTES;
    if (data_size > 200000) data_size = 200000;
    uint8_t *compressed = malloc(data_size);
    if (!compressed) {
        fclose(f);
        return img;
    }
    size_t read_count = fread(compressed, 1, data_size, f);
    fclose(f);

    /* Decode 4 bitplanes */
    uint8_t *planes[NUM_PLANES];
    for (int i = 0; i < NUM_PLANES; i++) {
        planes[i] = malloc(PLANE_SIZE);
        if (!planes[i]) {
            for (int j = 0; j < i; j++) free(planes[j]);
            free(compressed);
            return img;
        }
    }

    size_t offset = 0;
    for (int p = 0; p < NUM_PLANES; p++) {
        size_t consumed = decode_plane(compressed + offset, read_count - offset, planes[p]);
        if (consumed == 0) {
            for (int i = 0; i < NUM_PLANES; i++) free(planes[i]);
            free(compressed);
            return img;
        }
        offset += consumed;
    }
    free(compressed);

    /* Combine 4 bitplanes into 4-bit color indices.
     * Each byte in a plane represents 8 pixels (1 bit per pixel, MSB first).
     * Plane N contributes bit N of the color index. */
    uint8_t *pixels_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    if (!pixels_indexed) {
        for (int i = 0; i < NUM_PLANES; i++) free(planes[i]);
        return img;
    }

    for (int y = 0; y < SPY_HEIGHT; y++) {
        for (int x = 0; x < SPY_WIDTH; x++) {
            int byte_off = y * BYTES_PER_ROW + x / 8;
            int bit = 7 - (x % 8);
            uint8_t color = 0;
            for (int p = 0; p < NUM_PLANES; p++) {
                color |= ((planes[p][byte_off] >> bit) & 1) << p;
            }
            pixels_indexed[y * SPY_WIDTH + x] = color;
        }
    }

    for (int i = 0; i < NUM_PLANES; i++) free(planes[i]);

    if (indexed_out) {
        memcpy(indexed_out, pixels_indexed, SPY_WIDTH * SPY_HEIGHT);
    }

    /* Convert indexed to RGBA */
    uint8_t *rgba = malloc(SPY_WIDTH * SPY_HEIGHT * 4);
    if (!rgba) {
        free(pixels_indexed);
        return img;
    }

    for (int i = 0; i < SPY_WIDTH * SPY_HEIGHT; i++) {
        uint8_t idx = pixels_indexed[i];
        rgba[i * 4 + 0] = palette[idx * 3 + 0];
        rgba[i * 4 + 1] = palette[idx * 3 + 1];
        rgba[i * 4 + 2] = palette[idx * 3 + 2];
        rgba[i * 4 + 3] = 255;
    }
    free(pixels_indexed);

    img.data = rgba;
    img.width = SPY_WIDTH;
    img.height = SPY_HEIGHT;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    return img;
}
