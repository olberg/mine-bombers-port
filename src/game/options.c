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
/* Options submenu items — 14 rows exactly as the original (FUN_1000_1017:
 * cursor wraps 0..0xD and STARTS on 0xD; OPTIONS5.SPY has all 14 row labels
 * baked in, DOSBox capture 2026-06-11).
 * 0-6:   numeric settings: CASH, TREASURES, ROUNDS, TIME, PLAYERS, SPEED,
 *        BOMB DAMAGE (the background labels; the decompiler names
 *        "starting_lives" / "speed_setting" were myths — see capture)
 * 7-10:  toggles: DARKNESS, FREE MARKET, SELLING, WINNER (by money/by wins)
 * 11:    REDEFINE KEYS (FUN_1010_d4c4)
 * 12:    LOAD LEVELS = map select (FUN_1010_e231, seg_1000:544-549 item 0xC)
 * 13:    MAINMENU (exit)
 * The port's sound-config screen has no original menu row; it opens with F2
 * (port addition, no visual footprint). */
#define OPTIONS_ITEMS          14
#define OPT_ITEM_KEY_CONFIG    11
#define OPT_ITEM_MAP_SELECT    12
#define OPT_ITEM_EXIT          13

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

/* Toggle indicator sprites from SIKA.SPY (seg_1010:4806-4810):
 * DAT_1038_0670 (dim coin, drawn at BOTH positions):
 *   capture_screen_region(buf, 0x34, 0x68, 0x28, 0x5a) = (90,40) 15x13
 * DAT_1038_066c (bright coin, drawn at the active position):
 *   capture_screen_region(buf, 0x41, 0x68, 0x35, 0x5a) = (90,53) 15x13 */
#define TOG_DIM_SX     90
#define TOG_DIM_SY     40
#define TOG_LIT_SX     90
#define TOG_LIT_SY     53
#define TOG_W          15
#define TOG_H          13

/* Toggle blit positions (FUN_1000_0140 items 7-10):
 * X = 0xC0+0xB9 = 377 (left/"on") or 0xC0+0xFB = 443 (right/"off"),
 * Y = 0x60+0xAD+0x18k = 269+24k. Items 7-9: nonzero → left. Item 10
 * (WINNER) is INVERTED: zero ("by money") → left, nonzero → right. */
#define TOG_X_LEFT    377
#define TOG_X_RIGHT   443

/* Value slider bars (FUN_1000_0140 items 0-6): the slot interior spans
 * X 334..499 (X_left = 0xC0+0x8E); the white bar is an INCLUSIVE
 * fill_rect from 334 to 334+offset, rows Y 101+24i..113+24i.
 * Offsets verified against DOSBox pixel measurements (defaults plus
 * cash 1050 / rounds 17 / frame_delay 13):
 *   cash 750→46, 1050→65 = Trunc(v/16)         (max 2650 → 165)
 *   treasures 45→99      = Trunc(v*2.2)        (cap fits: 74 → 162)
 *   rounds 15→45, 17→51  = v*3                 (max 54 → 162)
 *   time 7662→51         = Trunc(ticks/150)    (max 0x60AE → 165)
 *   players 2→55         = (n-1)*0x37
 *   speed fd=8→125, 13→100 = (33-fd)*5         (fd 0 → 165)
 *   damage 100→165       = Trunc(v*1.65)       (100 → 165) */
#define BAR_X         334
#define BAR_MAX_W     166   /* interior 334..499 inclusive */

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

static Image tog_dim_img, tog_lit_img;
static Texture2D tog_dim_tex, tog_lit_tex;

static BitmapFont font;

static int current_item;
static OptionsState state;

/* Extract the options cursor and toggle sprites from SIKA.SPY.
 * The original captures these as INDEX data at startup and blits them while
 * the OPTIONS5 palette is installed — so bake them with bg_palette, not the
 * SIKA sheet palette (the grays/golds differ slightly between the two). */
static void load_cursor(void)
{
    uint8_t sheet_palette[768];
    uint8_t *sheet_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    Image sheet_img = LoadSPY("assets/SIKA.SPY", sheet_palette, sheet_indexed);
    cursor_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                               OPT_CURSOR_SX, OPT_CURSOR_SY,
                               OPT_CURSOR_W, OPT_CURSOR_H, bg_palette);
    cursor_tex = LoadTextureFromImage(cursor_img);
    tog_dim_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                                TOG_DIM_SX, TOG_DIM_SY, TOG_W, TOG_H,
                                bg_palette);
    tog_dim_tex = LoadTextureFromImage(tog_dim_img);
    tog_lit_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                                TOG_LIT_SX, TOG_LIT_SY, TOG_W, TOG_H,
                                bg_palette);
    tog_lit_tex = LoadTextureFromImage(tog_lit_img);
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

    /* Cursor starts on MAINMENU — FUN_1000_1017 inits local_e = 0xD */
    current_item = OPT_ITEM_EXIT;
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
    case 1: /* treasures: +-1, range [0, 74] (bar Trunc(v*2.2) fits 165) */
        if (dir < 0 && g_config.treasures == 0) break;
        if (dir > 0 && g_config.treasures >= 74) break;
        g_config.treasures = (uint8_t)(g_config.treasures + dir);
        break;
    case 2: /* total_rounds: +-1, range [1, 54] (original bound v*3 < 0xA5,
             * seg_1000:447-448) */
        g_config.total_rounds += dir;
        if (g_config.total_rounds < 1) g_config.total_rounds = 1;
        if (g_config.total_rounds > 54) g_config.total_rounds = 54;
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
    case 6: /* BOMB DAMAGE %: +-1, range [0, 100] (original left-guard
             * `!= 0`; default 100 fills the bar exactly) */
        if (dir < 0 && g_config.bomb_damage_pct == 0) break;
        if (dir > 0 && g_config.bomb_damage_pct >= 100) break;
        g_config.bomb_damage_pct = (uint8_t)(g_config.bomb_damage_pct + dir);
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
            } else if (current_item == OPT_ITEM_MAP_SELECT) {
                config_save("assets/OPTIONS.CFG");
                return OPTIONS_MAP_SELECT;
            } else if (current_item == OPT_ITEM_EXIT) {
                config_save("assets/OPTIONS.CFG");
                palette_start_fade_out(FADE_STEPS);
                state = OPT_FADE_OUT;
            }
        }

        /* F2 opens the sound-config screen (port addition: the original
         * configured sound in SETUP.EXE; there is no options row for it). */
        if (IsKeyPressed(KEY_F2)) {
            config_save("assets/OPTIONS.CFG");
            return OPTIONS_SOUND_CONFIG;
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

/* Value-bar fill offset for item i (FUN_1000_0140; the white bar is the
 * INCLUSIVE fill X 334..334+offset — see BAR_X comment for derivation). */
static int value_bar_offset(int i)
{
    switch (i) {
    case 0: return g_config.starting_cash / 16;
    case 1: return (int)(g_config.treasures * 2.2);
    case 2: return g_config.total_rounds * 3;
    case 3: {
        int total = (int)(uint16_t)g_config.time_limit_lo |
                    ((int)g_config.time_limit_hi << 16);
        return total / 150;
    }
    case 4: return (g_config.num_players - 1) * 0x37;
    case 5: return (33 - g_config.frame_delay) * 5;
    case 6: return (int)(g_config.bomb_damage_pct * 1.65);
    default: return 0;
    }
}

void options_draw(void)
{
    DrawTexture(bg_tex, 0, 0, WHITE);

    if (state == OPT_FADE_IN && palette_is_fading()) return;

    /* Draw cursor */
    int cursor_y = CURSOR_BASE_Y + current_item * CURSOR_STEP;
    DrawTexture(cursor_tex, CURSOR_X, cursor_y, WHITE);

    /* Bar = color 1 (white in OPTIONS5.SPY's palette — set_draw_color(1)
     * in FUN_1000_09fe), value text = color 8 (139,139,139 gray). */
    Color bar_color = palette_get_color(1);
    Color text_color = palette_get_color(8);
    char buf[32];

    /* Items 0-6: white value bar, then gray value text on top */
    for (int i = 0; i <= 6; i++) {
        int row_y = CURSOR_BASE_Y + i * CURSOR_STEP - 1;   /* 101 + 24i */
        int w = value_bar_offset(i);
        if (w < 0) w = 0;
        if (w > BAR_MAX_W - 1) w = BAR_MAX_W - 1;
        DrawRectangle(BAR_X, row_y, w + 1, 13, bar_color);

        int text_y = CURSOR_BASE_Y + i * CURSOR_STEP + 1;  /* cursor_Y + 1 */
        int text_x = (i <= 3) ? TEXT_X_0_3 : TEXT_X_4_6;

        switch (i) {
        case 0:
            snprintf(buf, sizeof(buf), "%d", g_config.starting_cash);
            break;
        case 1:
            snprintf(buf, sizeof(buf), "%d", g_config.treasures);
            break;
        case 2:
            snprintf(buf, sizeof(buf), "%d", g_config.total_rounds);
            break;
        case 3: {
            /* REAL time "M:SS Min": the value counts PIT ticks; the
             * original converts to seconds (×65536/1193182 ≈ /18.2065)
             * before the div/mod-60 split. DOSBox-verified: factory
             * default 0x1DEE = 7662 ticks displays "7:00 Min". */
            int total = (int)(uint16_t)g_config.time_limit_lo |
                        ((int)g_config.time_limit_hi << 16);
            int seconds = (int)((int64_t)total * 65536 / 1193182);
            if (seconds < 0) seconds = 0;
            snprintf(buf, sizeof(buf), "%d:%02d min",
                     seconds / 60, seconds % 60);
            break;
        }
        case 4:
            snprintf(buf, sizeof(buf), "%d", g_config.num_players);
            break;
        case 5:
            snprintf(buf, sizeof(buf), "%d%%", (33 - g_config.frame_delay) * 3 + 1);
            break;
        case 6:
            snprintf(buf, sizeof(buf), "%d%%", g_config.bomb_damage_pct);
            break;
        }

        DrawTextFON(&font, buf, text_x, text_y, text_color);
    }

    /* Items 7-10: toggle coins (FUN_1000_0140 items 7-10): the dim coin at
     * BOTH positions, the bright coin over the active one. Items 7-9:
     * nonzero → left ("on"). Item 10 (WINNER) is inverted in the original:
     * zero ("by money") → left. */
    for (int i = 0; i < 4; i++) {
        int y = CURSOR_BASE_Y + (7 + i) * CURSOR_STEP - 1;  /* 269 + 24i */
        DrawTexture(tog_dim_tex, TOG_X_LEFT,  y, WHITE);
        DrawTexture(tog_dim_tex, TOG_X_RIGHT, y, WHITE);
        bool lit_left = (i == 3) ? (g_config.option_toggle[i] == 0)
                                 : (g_config.option_toggle[i] != 0);
        DrawTexture(tog_lit_tex, lit_left ? TOG_X_LEFT : TOG_X_RIGHT, y, WHITE);
    }
}

void options_cleanup(void)
{
    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;

    UnloadTexture(cursor_tex);
    UnloadImage(cursor_img);

    UnloadTexture(tog_dim_tex);
    UnloadImage(tog_dim_img);
    UnloadTexture(tog_lit_tex);
    UnloadImage(tog_lit_img);

    UnloadFON(&font);
}
