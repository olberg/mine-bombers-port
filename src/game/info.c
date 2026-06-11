#include "info.h"
#include "loaders/spy_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>

#define FADE_STEPS  7
#define INFO_IMAGES 5

/* Image order from decompiled FUN_1000_10e0 (verified against MB.EXE string table):
 * 1. INFO1.SPY  — weapons reference
 * 2. INFO3.SPY  — tools reference
 * 3. SHAPET.SPY — tile type reference
 * 4. INFO2.SPY  — credits
 * 5. CODES.SPY  — cheat codes (only shown if Tab pressed on image 4) */
static const char *info_files[INFO_IMAGES] = {
    "assets/INFO1.SPY",
    "assets/INFO3.SPY",
    "assets/SHAPET.SPY",
    "assets/INFO2.SPY",
    "assets/CODES.SPY",
};

typedef enum {
    INFO_FADE_IN,
    INFO_WAIT_KEY,
    INFO_FADE_OUT,
    INFO_NEXT,
    INFO_FINISHED
} InfoState;

static Image img;
static Texture2D tex;
static uint8_t pal[768];
static uint8_t *indexed;

static int current_image;
static InfoState state;
static bool esc_pressed;

static void load_image(int index)
{
    if (indexed) {
        free(indexed);
        indexed = NULL;
    }
    if (tex.id) {
        UnloadTexture(tex);
        tex = (Texture2D){0};
    }
    if (img.data) {
        UnloadImage(img);
        img = (Image){0};
    }

    indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    img = LoadSPY(info_files[index], pal, indexed);
    tex = LoadTextureFromImage(img);
    palette_init(pal);
}

void info_init(void)
{
    indexed = NULL;
    tex = (Texture2D){0};
    img = (Image){0};

    current_image = 0;
    esc_pressed = false;

    load_image(0);
    palette_start_fade_in(FADE_STEPS);
    state = INFO_FADE_IN;
}

InfoResult info_update(void)
{
    switch (state) {
    case INFO_FADE_IN:
        palette_update();
        palette_apply_to_pixels(indexed, (uint8_t *)img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(tex, img.data);
        if (!palette_is_fading()) {
            state = INFO_WAIT_KEY;
        }
        break;

    case INFO_WAIT_KEY:
        if (input_pressed(INPUT_CANCEL)) {
            esc_pressed = true;
            palette_start_fade_out(FADE_STEPS);
            state = INFO_FADE_OUT;
        } else if (input_pressed(INPUT_ANY_KEY)) {
            /* Image 4 (index 3, INFO2/credits): only advance to CODES if Tab pressed */
            if (current_image == 3 && !IsKeyPressed(KEY_TAB)) {
                esc_pressed = true;  /* skip codes screen */
            }
            palette_start_fade_out(FADE_STEPS);
            state = INFO_FADE_OUT;
        }
        break;

    case INFO_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(indexed, (uint8_t *)img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(tex, img.data);
        if (!palette_is_fading()) {
            state = INFO_NEXT;
        }
        break;

    case INFO_NEXT:
        if (esc_pressed || current_image >= INFO_IMAGES - 1) {
            state = INFO_FINISHED;
            return INFO_DONE;
        }
        current_image++;
        load_image(current_image);
        palette_start_fade_in(FADE_STEPS);
        state = INFO_FADE_IN;
        break;

    case INFO_FINISHED:
        return INFO_DONE;
    }

    return INFO_ACTIVE;
}

void info_draw(void)
{
    if (tex.id) {
        DrawTexture(tex, 0, 0, WHITE);
    }
}

void info_cleanup(void)
{
    if (tex.id) UnloadTexture(tex);
    if (img.data) UnloadImage(img);
    free(indexed);
    indexed = NULL;
    tex = (Texture2D){0};
    img = (Image){0};
}
