#include "sound_config.h"
#include "loaders/spy_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "audio/music.h"
#include "audio/sfx.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FADE_STEPS  7

/* Sound config file format (original 6 bytes):
 * [0] sound_card_type  (unused in port — always 0)
 * [1] irq              (unused in port — always 0)
 * [2] dma              (unused in port — always 0)
 * [3] base_port_mult   (unused in port — always 0)
 * [4] features         (bit 0 = music, bit 1 = SFX)
 * [5] mix_rate_mult    (unused in port — always 22) */
#define SOUND_CFG_SIZE 6

/* Menu items */
#define SC_ITEM_MUSIC  0
#define SC_ITEM_SFX    1
#define SC_ITEM_EXIT   2
#define SC_ITEMS       3

/* Layout: centered text on OPTIONS5.SPY background */
#define MENU_X     240
#define VALUE_X    420
#define MENU_Y     200
#define MENU_STEP   24

typedef enum {
    SC_FADE_IN,
    SC_ACTIVE,
    SC_FADE_OUT
} SCState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static BitmapFont font;

static int current_item;
static SCState state;

void sound_config_init(void)
{
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/OPTIONS5.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    font = LoadFON("assets/FONTTI.FON", true);

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    current_item = 0;
    state = SC_FADE_IN;
}

SoundConfigResult sound_config_update(void)
{
    switch (state) {
    case SC_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = SC_ACTIVE;
        }
        break;

    case SC_ACTIVE:
        if (input_pressed(INPUT_DOWN)) {
            current_item++;
            if (current_item >= SC_ITEMS) current_item = 0;
        }
        if (input_pressed(INPUT_UP)) {
            current_item--;
            if (current_item < 0) current_item = SC_ITEMS - 1;
        }

        if (input_pressed(INPUT_LEFT) || input_pressed(INPUT_RIGHT) ||
            input_pressed(INPUT_CONFIRM)) {
            switch (current_item) {
            case SC_ITEM_MUSIC:
                music_set_enabled(!music_is_enabled());
                break;
            case SC_ITEM_SFX:
                sfx_set_enabled(!sfx_is_enabled());
                break;
            case SC_ITEM_EXIT:
                if (input_pressed(INPUT_CONFIRM)) {
                    sound_config_save("assets/SOUNDCFG.DAT");
                    palette_start_fade_out(FADE_STEPS);
                    state = SC_FADE_OUT;
                }
                break;
            }
        }

        if (input_pressed(INPUT_CANCEL)) {
            sound_config_save("assets/SOUNDCFG.DAT");
            palette_start_fade_out(FADE_STEPS);
            state = SC_FADE_OUT;
        }
        break;

    case SC_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            return SOUND_CONFIG_DONE;
        }
        break;
    }

    return SOUND_CONFIG_ACTIVE;
}

static void draw_shadow_text(BitmapFont *f, const char *text, int x, int y)
{
    Color c12 = palette_get_color(12);
    Color c4  = palette_get_color(4);
    Color c8  = palette_get_color(8);
    DrawTextFON(f, text, x - 1, y, c12);
    DrawTextFON(f, text, x + 1, y, c4);
    DrawTextFON(f, text, x, y, c8);
}

void sound_config_draw(void)
{
    DrawTexture(bg_tex, 0, 0, WHITE);

    if (state == SC_FADE_IN && palette_is_fading()) return;

    Color highlight = palette_get_color(14);
    Color text_color = palette_get_color(8);

    /* Title */
    draw_shadow_text(&font, "SOUND CONFIGURATION", 200, MENU_Y - 40);

    /* Menu items */
    static const char *labels[SC_ITEMS] = { "Music", "Effects", "Back" };

    for (int i = 0; i < SC_ITEMS; i++) {
        int y = MENU_Y + i * MENU_STEP;
        Color c = (i == current_item) ? highlight : text_color;

        /* Cursor indicator */
        if (i == current_item) {
            DrawTextFON(&font, ">", MENU_X - 16, y, c);
        }

        DrawTextFON(&font, labels[i], MENU_X, y, c);

        /* Value */
        if (i == SC_ITEM_MUSIC) {
            DrawTextFON(&font, music_is_enabled() ? "ON" : "OFF", VALUE_X, y, c);
        } else if (i == SC_ITEM_SFX) {
            DrawTextFON(&font, sfx_is_enabled() ? "ON" : "OFF", VALUE_X, y, c);
        }
    }
}

void sound_config_cleanup(void)
{
    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;

    UnloadFON(&font);
}

bool sound_config_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        /* Default: both music and SFX enabled (features = 3) */
        music_set_enabled(true);
        sfx_set_enabled(true);
        return false;
    }

    uint8_t data[SOUND_CFG_SIZE];
    size_t n = fread(data, 1, SOUND_CFG_SIZE, f);
    fclose(f);

    if (n < SOUND_CFG_SIZE) {
        music_set_enabled(true);
        sfx_set_enabled(true);
        return false;
    }

    /* Byte 4 = features bitmask: bit 0 = music, bit 1 = SFX */
    uint8_t features = data[4];
    music_set_enabled((features & 1) != 0);
    sfx_set_enabled((features & 2) != 0);

    return true;
}

bool sound_config_save(const char *path)
{
    uint8_t data[SOUND_CFG_SIZE] = {0};

    /* Bytes 0-3: hardware config (unused in port) */
    data[0] = 0;   /* sound_card_type = none */
    data[1] = 0;   /* irq */
    data[2] = 0;   /* dma */
    data[3] = 0;   /* base_port_mult */

    /* Byte 4: features bitmask */
    uint8_t features = 0;
    if (music_is_enabled()) features |= 1;
    if (sfx_is_enabled())   features |= 2;
    data[4] = features;

    /* Byte 5: mix rate multiplier (unused, keep original default) */
    data[5] = 22;

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    size_t n = fwrite(data, 1, SOUND_CFG_SIZE, f);
    fclose(f);

    return n == SOUND_CFG_SIZE;
}
