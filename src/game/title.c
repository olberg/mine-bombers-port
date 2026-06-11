#include "title.h"
#include "loaders/spy_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>

#define FADE_STEPS 7

static Image title_img;
static Texture2D title_tex;
static uint8_t title_palette[768];
static uint8_t *title_indexed; /* 640x480 indexed pixels */
static TitleState state;

void title_init(void)
{
    title_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    title_img = LoadSPY("assets/TITLEBE.SPY", title_palette, title_indexed);
    title_tex = LoadTextureFromImage(title_img);

    palette_init(title_palette);
    palette_start_fade_in(FADE_STEPS);
    state = TITLE_FADE_IN;
}

TitleState title_update(void)
{
    switch (state) {
    case TITLE_FADE_IN:
        palette_update();
        /* Update texture with faded palette */
        palette_apply_to_pixels(title_indexed, (uint8_t *)title_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(title_tex, title_img.data);
        if (!palette_is_fading()) {
            state = TITLE_WAIT;
        }
        break;

    case TITLE_WAIT:
        if (input_pressed(INPUT_ANY_KEY) || input_pressed(INPUT_CONFIRM) ||
            input_pressed(INPUT_CANCEL) || input_pressed(INPUT_QUIT)) {
            palette_start_fade_out(FADE_STEPS);
            state = TITLE_FADE_OUT;
        }
        break;

    case TITLE_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(title_indexed, (uint8_t *)title_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(title_tex, title_img.data);
        if (!palette_is_fading()) {
            state = TITLE_DONE;
        }
        break;

    case TITLE_DONE:
        break;
    }

    return state;
}

void title_draw(void)
{
    DrawTexture(title_tex, 0, 0, WHITE);
}

void title_cleanup(void)
{
    UnloadTexture(title_tex);
    UnloadImage(title_img);
    free(title_indexed);
    title_indexed = NULL;
}
