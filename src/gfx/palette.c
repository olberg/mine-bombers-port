#include "palette.h"
#include <string.h>

static uint8_t target_palette[768];  /* 8-bit values (0-255) */
static uint8_t current_palette[768]; /* 8-bit values (0-255) */
static int fade_steps;
static int fade_current_step;
static bool fading;
static bool fade_direction_in; /* true = fade in, false = fade out */

void palette_init(const uint8_t raw_palette[768])
{
    memcpy(target_palette, raw_palette, 768);
    memcpy(current_palette, raw_palette, 768);
    fading = false;
}

void palette_start_fade_in(int steps)
{
    fade_steps = steps;
    fade_current_step = 0;
    fading = true;
    fade_direction_in = true;
    /* Start from black */
    memset(current_palette, 0, 768);
}

void palette_start_fade_out(int steps)
{
    fade_steps = steps;
    fade_current_step = 0;
    fading = true;
    fade_direction_in = false;
    /* Start from target */
    memcpy(current_palette, target_palette, 768);
}

bool palette_is_fading(void)
{
    return fading;
}

void palette_update(void)
{
    if (!fading) return;

    fade_current_step++;
    if (fade_current_step >= fade_steps) {
        fading = false;
        if (fade_direction_in) {
            memcpy(current_palette, target_palette, 768);
        } else {
            memset(current_palette, 0, 768);
        }
        return;
    }

    /* Original formula: color[j] = (target[j] / steps) * step
     * For fade_in: step goes 1..steps
     * For fade_out: step goes (steps-1)..0 */
    for (int j = 0; j < 768; j++) {
        int step;
        if (fade_direction_in) {
            step = fade_current_step;
        } else {
            step = fade_steps - fade_current_step;
        }
        current_palette[j] = (uint8_t)((target_palette[j] / fade_steps) * step);
    }
}

Color palette_get_color(uint8_t index)
{
    Color c;
    c.r = current_palette[index * 3 + 0];
    c.g = current_palette[index * 3 + 1];
    c.b = current_palette[index * 3 + 2];
    c.a = 255;
    return c;
}

void palette_apply_to_pixels(const uint8_t *src_indexed, uint8_t *dst_rgba, int pixel_count)
{
    for (int i = 0; i < pixel_count; i++) {
        uint8_t idx = src_indexed[i];
        dst_rgba[i * 4 + 0] = current_palette[idx * 3 + 0];
        dst_rgba[i * 4 + 1] = current_palette[idx * 3 + 1];
        dst_rgba[i * 4 + 2] = current_palette[idx * 3 + 2];
        dst_rgba[i * 4 + 3] = 255;
    }
}
