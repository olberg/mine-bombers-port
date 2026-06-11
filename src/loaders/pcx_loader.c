#include "loaders/pcx_loader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  manufacturer;   /* should be 0x0A */
    uint8_t  version;
    uint8_t  encoding;       /* 1 = RLE */
    uint8_t  bpp;            /* bits per pixel per plane */
    int16_t  xmin, ymin;
    int16_t  xmax, ymax;
    int16_t  hdpi, vdpi;
    uint8_t  ega_palette[48];
    uint8_t  reserved;
    uint8_t  num_planes;
    int16_t  bytes_per_line;
    int16_t  palette_type;
    int16_t  hscreen_size;
    int16_t  vscreen_size;
    uint8_t  padding[54];
} PCXHeader;
#pragma pack(pop)

Image LoadPCX(const char *path)
{
    Image img = {0};

    FILE *f = fopen(path, "rb");
    if (!f) return img;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 128) {
        fclose(f);
        return img;
    }

    /* Read header */
    PCXHeader hdr;
    if (fread(&hdr, sizeof(PCXHeader), 1, f) != 1) {
        fclose(f);
        return img;
    }

    if (hdr.manufacturer != 0x0A) {
        fclose(f);
        return img;
    }

    int w = hdr.xmax - hdr.xmin + 1;
    int h = hdr.ymax - hdr.ymin + 1;

    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        fclose(f);
        return img;
    }

    /* Try reading 256-color palette from end of file.
     * Original (FUN_1010_73bd) conditionally skips palette loading
     * based on a caller-provided flag. For the port, we always try
     * the 256-color palette first and fall back to the 16-color
     * EGA palette from the header if the marker is missing. */
    uint8_t palette[768];
    bool have_palette = false;
    if (file_size >= 128 + 769) {
        fseek(f, file_size - 769, SEEK_SET);
        uint8_t marker;
        if (fread(&marker, 1, 1, f) == 1 && marker == 0x0C) {
            if (fread(palette, 1, 768, f) == 768) {
                have_palette = true;
            }
        }
    }
    if (!have_palette) {
        /* Fall back to 16-color EGA palette from header */
        memset(palette, 0, sizeof(palette));
        memcpy(palette, hdr.ega_palette, 48);
    }

    /* Decode RLE pixel data */
    fseek(f, 128, SEEK_SET);
    int total_pixels = w * h;
    uint8_t *indexed = (uint8_t *)malloc(total_pixels);
    if (!indexed) {
        fclose(f);
        return img;
    }

    int decoded = 0;
    while (decoded < total_pixels) {
        int byte = fgetc(f);
        if (byte == EOF) break;

        if ((byte & 0xC0) == 0xC0) {
            int count = byte & 0x3F;
            int value = fgetc(f);
            if (value == EOF) break;
            for (int i = 0; i < count && decoded < total_pixels; i++) {
                indexed[decoded++] = (uint8_t)value;
            }
        } else {
            indexed[decoded++] = (uint8_t)byte;
        }
    }
    fclose(f);

    /* Convert indexed to RGBA */
    uint8_t *rgba = (uint8_t *)malloc(w * h * 4);
    if (!rgba) {
        free(indexed);
        return img;
    }

    for (int i = 0; i < w * h; i++) {
        uint8_t idx = indexed[i];
        rgba[i * 4 + 0] = palette[idx * 3 + 0];
        rgba[i * 4 + 1] = palette[idx * 3 + 1];
        rgba[i * 4 + 2] = palette[idx * 3 + 2];
        rgba[i * 4 + 3] = 255;  /* all opaque (original renders without transparency) */
    }

    free(indexed);

    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    return img;
}
