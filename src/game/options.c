#include "options.h"
#include "config.h"
#include "loaders/spy_loader.h"
#include "loaders/sprite_sheet.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FADE_STEPS     7
/* Options submenu items (matching FUN_1000_1017, seg_1000:570 and
 * process_menu_selection entries at seg_1010:7158).
 * 0-6:   numeric settings (speed, treasures, rounds, time, players, frame, lives)
 * 7-10:  toggles (darkness, free market, selling, winner-by)
 * 11:    Key config
 * 12:    Sound config
 * 13:    Map Select (originally FUN_1010_e231, seg_1000:544-549 item 0xC)
 * 14:    Exit
 */
#define OPTIONS_ITEMS          15
#define OPT_ITEM_KEY_CONFIG    11
#define OPT_ITEM_SOUND_CONFIG  12
#define OPT_ITEM_MAP_SELECT    13
#define OPT_ITEM_EXIT          14

/* Cursor positioning from decompiled FUN_1000_00ff:
 * blit_sprite(0, ptr, (item+4)*0x18+6, 0xD9)
 * Y = (item + 4) * 24 + 6, X = 217 */
#define CURSOR_BASE_Y  102   /* (0 + 4) * 24 + 6 = 102 */
#define CURSOR_STEP     24   /* 0x18 */
#define CURSOR_X       217   /* 0xD9 */

/* Options cursor sprite from SIKA.SPY (DAT_1038_067c):
 * capture_screen_region(buf, 0x6d, 0xe7, 99, 0xcd)
 * = (X=205, Y=99) to (X=231, Y=109) → 27x11 pixels.
 * This is a small arrow indicator, NOT the 65x20 menu shovel. */
#define OPT_CURSOR_SX  205
#define OPT_CURSOR_SY   99
#define OPT_CURSOR_W     27
#define OPT_CURSOR_H     11

/* Value text positions from decompiled FUN_1000_0140 (page 1 drawing).
 * Stack frame: *(param_1-8)=0x60 (Y base), *(param_1-6)=0xC0 (X base).
 * Text X: items 0-3 at 0xC0+0xD0=400, items 4-6 at 0xC0+0xD8=408.
 * Text Y: 0x60 + item_offset = cursor_Y + 1. */
#define TEXT_X_0_3  400
#define TEXT_X_4_6  408

typedef enum { OPT_FADE_IN, OPT_ACTIVE, OPT_FADE_OUT } OptionsState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static Image cursor_img;
static Texture2D cursor_tex;

static BitmapFont font;

static int current_item;
static OptionsState state;

/* Extract the options cursor sprite from SIKA.SPY (small arrow, not the menu shovel) */
static void load_cursor(void)
{
    uint8_t sheet_palette[768];
    uint8_t *sheet_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    Image sheet_img = LoadSPY("assets/SIKA.SPY", sheet_palette, sheet_indexed);
    cursor_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                               OPT_CURSOR_SX, OPT_CURSOR_SY,
                               OPT_CURSOR_W, OPT_CURSOR_H, sheet_palette);
    cursor_tex = LoadTextureFromImage(cursor_img);
    free(sheet_indexed);
    UnloadImage(sheet_img);
}

void options_init(void)
{
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/OPTIONS5.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    load_cursor();
    font = LoadFON("assets/FONTTI.FON", true);

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    current_item = 0;
    state = OPT_FADE_IN;
}

/* Adjust setting value for the current item in the given direction (-1 or +1) */
static void adjust_value(int dir)
{
    switch (current_item) {
    case 0: /* starting_cash: +-100, range [0, 2650] */
        g_config.starting_cash += dir * 100;
        if (g_config.starting_cash < 0) g_config.starting_cash = 0;
        if (g_config.starting_cash > 2650) g_config.starting_cash = 2650;
        break;
    case 1: /* config_byte: +-1, min 0 */
        g_config.config_byte = (uint8_t)(g_config.config_byte + dir);
        break;
    case 2: /* total_rounds: +-1, min 1 */
        g_config.total_rounds += dir;
        if (g_config.total_rounds < 1) g_config.total_rounds = 1;
        break;
    case 3: { /* time limit: +-0x111, range [1, 0x60AE] */
        int32_t t = (int32_t)g_config.time_limit_hi << 16 | (uint16_t)g_config.time_limit_lo;
        t += dir * 0x111;
        if (t < 1) t = 1;
        if (t > 0x60AE) t = 0x60AE;
        g_config.time_limit_lo = (int16_t)(t & 0xFFFF);
        g_config.time_limit_hi = (int16_t)(t >> 16);
        break;
    }
    case 4: /* num_players: +-1, range [1, 4] */
        g_config.num_players = (uint8_t)(g_config.num_players + dir);
        if (g_config.num_players < 1) g_config.num_players = 1;
        if (g_config.num_players > 4) g_config.num_players = 4;
        break;
    case 5: /* frame_delay: reversed — left=+1 (slower), right=-1 (faster), range [0, 33] */
        g_config.frame_delay -= dir;  /* reversed direction */
        if (g_config.frame_delay < 0) g_config.frame_delay = 0;
        if (g_config.frame_delay > 33) g_config.frame_delay = 33;
        break;
    case 6: /* starting_lives: +-1, min 0 */
        g_config.starting_lives = (uint8_t)(g_config.starting_lives + dir);
        break;
    case 7: case 8: case 9: case 10: /* option toggles: darkness/freemarket/selling/winnerby */
        g_config.option_toggle[current_item - 7] ^= 1;
        break;
    default:
        break;
    }
}

OptionsResult options_update(void)
{
    switch (state) {
    case OPT_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = OPT_ACTIVE;
        }
        break;

    case OPT_ACTIVE:
        /* Navigation */
        if (input_pressed(INPUT_DOWN)) {
            current_item++;
            if (current_item >= OPTIONS_ITEMS) current_item = 0;
        }
        if (input_pressed(INPUT_UP)) {
            current_item--;
            if (current_item < 0) current_item = OPTIONS_ITEMS - 1;
        }

        /* Value adjustment */
        if (input_pressed(INPUT_LEFT)) {
            adjust_value(-1);
        }
        if (input_pressed(INPUT_RIGHT)) {
            adjust_value(1);
        }

        /* Enter/Confirm */
        if (input_pressed(INPUT_CONFIRM)) {
            if (current_item == OPT_ITEM_KEY_CONFIG) {
                config_save("assets/OPTIONS.CFG");
                return OPTIONS_KEY_CONFIG;
            } else if (current_item == OPT_ITEM_SOUND_CONFIG) {
                config_save("assets/OPTIONS.CFG");
                return OPTIONS_SOUND_CONFIG;
            } else if (current_item == OPT_ITEM_MAP_SELECT) {
                config_save("assets/OPTIONS.CFG");
                return OPTIONS_MAP_SELECT;
            } else if (current_item == OPT_ITEM_EXIT) {
                config_save("assets/OPTIONS.CFG");
                palette_start_fade_out(FADE_STEPS);
                state = OPT_FADE_OUT;
            }
        }

        /* ESC exits */
        if (input_pressed(INPUT_CANCEL)) {
            config_save("assets/OPTIONS.CFG");
            palette_start_fade_out(FADE_STEPS);
            state = OPT_FADE_OUT;
        }

        /* D/d = reset defaults */
        if (IsKeyPressed(KEY_D)) {
            config_set_defaults();
        }
        break;

    case OPT_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            return OPTIONS_DONE;
        }
        break;
    }

    return OPTIONS_ACTIVE;
}

void options_draw(void)
{
    DrawTexture(bg_tex, 0, 0, WHITE);

    if (state == OPT_FADE_IN && palette_is_fading()) return;

    /* Draw cursor */
    int cursor_y = CURSOR_BASE_Y + current_item * CURSOR_STEP;
    DrawTexture(cursor_tex, CURSOR_X, cursor_y, WHITE);

    /* Draw current values for items 0-6 */
    Color text_color = palette_get_color(8);
    char buf[32];

    for (int i = 0; i <= 6; i++) {
        int text_y = CURSOR_BASE_Y + i * CURSOR_STEP + 1;  /* cursor_Y + 1 */
        int text_x = (i <= 3) ? TEXT_X_0_3 : TEXT_X_4_6;

        switch (i) {
        case 0:
            snprintf(buf, sizeof(buf), "%d", g_config.starting_cash);
            break;
        case 1:
            snprintf(buf, sizeof(buf), "%d", g_config.config_byte);
            break;
        case 2:
            snprintf(buf, sizeof(buf), "%d", g_config.total_rounds);
            break;
        case 3: {
            /* Displayed as value/60 ":" value%60 like the original
             * (decompiled seg_1000:190-221). NOTE the VALUE counts PIT
             * ticks (54.9 ms each), so this "MM:SS" reads ~11x
             * the real round length — a quirk of the original's own
             * display, kept verbatim. Whether the original's options
             * screen really formats it this way is unverified (on-screen
             * capture pending). */
            int total = (int)g_config.time_limit_lo | ((int)g_config.time_limit_hi << 16);
            if (total <= 0) {
                snprintf(buf, sizeof(buf), "00:00");
            } else {
                int minutes = total / 60;
                int seconds = total % 60;
                snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);
            }
            break;
        }
        case 4:
            snprintf(buf, sizeof(buf), "%d", g_config.num_players);
            break;
        case 5:
            snprintf(buf, sizeof(buf), "%d%%", (33 - g_config.frame_delay) * 3 + 1);
            break;
        case 6:
            snprintf(buf, sizeof(buf), "%d%%", g_config.starting_lives);
            break;
        }

        DrawTextFON(&font, buf, text_x, text_y, text_color);
    }

    /* Items 7-10: option toggle indicators (text placeholder at correct positions).
     * [0]=Darkness, [1]=FreeMarket, [2]=Selling, [3]=WinnerBy.
     * Original uses small sprite indicators at X=443 (off) or X=377 (on). */
    for (int i = 0; i < 4; i++) {
        int text_y = CURSOR_BASE_Y + (7 + i) * CURSOR_STEP + 1;
        int text_x = g_config.option_toggle[i] ? 377 : 443;
        snprintf(buf, sizeof(buf), "%s", g_config.option_toggle[i] ? "ON" : "OFF");
        DrawTextFON(&font, buf, text_x, text_y, text_color);
    }

    /* OPTIONS5.SPY background only has label rows for items 0-13. We have
     * inserted a Map Select entry at row 13 (pushing Exit to row 14), so
     * overlay new labels over the slots that moved. */
    Color label_col = palette_get_color(1);
    int ms_y = CURSOR_BASE_Y + OPT_ITEM_MAP_SELECT * CURSOR_STEP;
    int ex_y = CURSOR_BASE_Y + OPT_ITEM_EXIT       * CURSOR_STEP;
    DrawRectangle(240, ms_y, 200, 20, BLACK);
    DrawRectangle(240, ex_y, 200, 20, BLACK);
    DrawTextFON(&font, "MAP SELECT", 248, ms_y + 4, label_col);
    DrawTextFON(&font, "EXIT",       248, ex_y + 4, label_col);
}

void options_cleanup(void)
{
    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;

    UnloadTexture(cursor_tex);
    UnloadImage(cursor_img);

    UnloadFON(&font);
}
