/* Singleplayer level complete / game over screen.
 * Decompiled ref: singleplayer_level_complete() at seg_1000:6434-6455.
 *
 * Flow: load GAMEOVER.SPY → fade in → wait 1 second → wait for keypress
 *       → fade out → done.
 *
 * Shown after every SP round: when a level is completed (but not the final
 * level 15) and when the player dies (game over). The separate congratulations
 * screen (CONGRATU.SPY) is used for campaign completion.
 */

#include "game/sp_level_complete.h"
#include "loaders/spy_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>

#define FADE_STEPS    7
#define WAIT_FRAMES   60  /* 1 second at 60fps (matches delay_wait(1000)) */

typedef enum {
    SPLC_FADE_IN_S,
    SPLC_WAIT_TIMER,
    SPLC_WAIT_KEY,
    SPLC_FADE_OUT_S,
    SPLC_FINISHED
} SpLevelCompleteState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;
static SpLevelCompleteState state;
static int timer;

void sp_level_complete_init(void)
{
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/GAMEOVER.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);
    state = SPLC_FADE_IN_S;
    timer = 0;
}

SpLevelCompleteResult sp_level_complete_update(void)
{
    switch (state) {
    case SPLC_FADE_IN_S:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = SPLC_WAIT_TIMER;
            timer = WAIT_FRAMES;
        }
        break;

    case SPLC_WAIT_TIMER:
        timer--;
        if (timer <= 0) {
            state = SPLC_WAIT_KEY;
        }
        break;

    case SPLC_WAIT_KEY:
        if (input_pressed(INPUT_CONFIRM) || input_pressed(INPUT_CANCEL) ||
            input_pressed(INPUT_ANY_KEY)) {
            palette_start_fade_out(FADE_STEPS);
            state = SPLC_FADE_OUT_S;
        }
        break;

    case SPLC_FADE_OUT_S:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = SPLC_FINISHED;
            return SPLC_DONE;
        }
        break;

    case SPLC_FINISHED:
        return SPLC_DONE;
    }

    return SPLC_ACTIVE;
}

void sp_level_complete_draw(void)
{
    if (bg_tex.id) {
        DrawTexture(bg_tex, 0, 0, WHITE);
    }
}

void sp_level_complete_cleanup(void)
{
    if (bg_tex.id) UnloadTexture(bg_tex);
    if (bg_img.data) UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;
    bg_tex = (Texture2D){0};
    bg_img = (Image){0};
}
