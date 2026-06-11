/* Singleplayer campaign completion congratulations screen.
 * Decompiled ref: singleplayer_game_complete() at seg_1000:6957-6979.
 *
 * Flow: clear palette → load CONGRATU.SPY → fade in → play SFX_KILI
 *       → wait 3 seconds → wait for keypress → fade out → done.
 */

#include "game/sp_complete.h"
#include "loaders/spy_loader.h"
#include "gfx/palette.h"
#include "audio/sfx.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>

#define FADE_STEPS    7
#define WAIT_FRAMES  180  /* 3 seconds at 60fps (matches delay_wait(3000)) */

typedef enum {
    SPC_FADE_IN,
    SPC_WAIT_TIMER,
    SPC_WAIT_KEY,
    SPC_FADE_OUT,
    SPC_FINISHED
} SpCompleteState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;
static SpCompleteState state;
static int timer;

void sp_complete_init(void)
{
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/CONGRATU.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);
    state = SPC_FADE_IN;
    timer = 0;

    /* Play congratulations sound effect.
     * Original: trigger_sound_effect(7,1,11000,0)
     * Trigger #7 = kili.voc */
    sfx_play(SFX_KILI);
}

SpCompleteResult sp_complete_update(void)
{
    switch (state) {
    case SPC_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = SPC_WAIT_TIMER;
            timer = WAIT_FRAMES;
        }
        break;

    case SPC_WAIT_TIMER:
        timer--;
        if (timer <= 0) {
            state = SPC_WAIT_KEY;
        }
        break;

    case SPC_WAIT_KEY:
        if (input_pressed(INPUT_CONFIRM) || input_pressed(INPUT_CANCEL) ||
            input_pressed(INPUT_ANY_KEY)) {
            palette_start_fade_out(FADE_STEPS);
            state = SPC_FADE_OUT;
        }
        break;

    case SPC_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = SPC_FINISHED;
            return SPC_DONE;
        }
        break;

    case SPC_FINISHED:
        return SPC_DONE;
    }

    return SPC_ACTIVE;
}

void sp_complete_draw(void)
{
    if (bg_tex.id) {
        DrawTexture(bg_tex, 0, 0, WHITE);
    }
}

void sp_complete_cleanup(void)
{
    if (bg_tex.id) UnloadTexture(bg_tex);
    if (bg_img.data) UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;
    bg_tex = (Texture2D){0};
    bg_img = (Image){0};
}
