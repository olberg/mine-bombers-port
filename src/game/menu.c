#include "menu.h"
#include "loaders/spy_loader.h"
#include "loaders/sprite_sheet.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FADE_STEPS    7
#define MENU_ITEMS    4
#define CURSOR_X      222
#define CURSOR_W      65
#define CURSOR_H      20

/* Menu item Y positions.
 * Original menu items (seg_1000:824 main_menu): Play, Options, Info, Quit.
 * Cursor Y for each item is drawn by FUN_1000_12bc at seg_1000:713-724:
 *   item 1 → 0x88 = 136, item 2 → 0xb8 = 184,
 *   item 3 → 0xe8 = 232, item 4 → 0x118 = 280
 * — uniform 48-px spacing. Map Select is reached via Options
 * (seg_1000:544-549 item 0xC invokes FUN_1010_e231), not from top level. */
static const int item_y[MENU_ITEMS] = { 136, 184, 232, 280 };

/* Title text from original: stored at DAT_1038_03e0 */
static const char *title_text = "Mine Bombers 3.11";

typedef enum { MENU_FADE_IN, MENU_ACTIVE, MENU_FADE_OUT } MenuState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static Image cursor_img;
static Texture2D cursor_tex;

static BitmapFont font;

static int current_item;
static int prev_item;
static MenuState state;
static MenuSelection pending_selection;

void menu_init(void)
{
    /* Load menu background */
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/MAIN3.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    /* Load sprite sheet and extract cursor */
    uint8_t sheet_palette[768];
    uint8_t *sheet_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    Image sheet_img = LoadSPY("assets/SIKA.SPY", sheet_palette, sheet_indexed);
    cursor_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                               150, 140, CURSOR_W, CURSOR_H, sheet_palette);
    cursor_tex = LoadTextureFromImage(cursor_img);
    free(sheet_indexed);
    UnloadImage(sheet_img);

    /* Load font */
    font = LoadFON("assets/FONTTI.FON", true);

    /* Init palette and start fade in */
    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    current_item = 0;
    prev_item = 0;
    state = MENU_FADE_IN;
    pending_selection = MENU_NONE;
}

MenuSelection menu_update(void)
{
    switch (state) {
    case MENU_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = MENU_ACTIVE;
        }
        break;

    case MENU_ACTIVE:
        /* Navigation */
        if (input_pressed(INPUT_DOWN)) {
            prev_item = current_item;
            current_item++;
            if (current_item >= MENU_ITEMS) current_item = 0;
        }
        if (input_pressed(INPUT_UP)) {
            prev_item = current_item;
            current_item--;
            if (current_item < 0) current_item = MENU_ITEMS - 1;
        }

        /* Selection */
        if (input_pressed(INPUT_CONFIRM)) {
            switch (current_item) {
            case 0: pending_selection = MENU_START; break;
            case 1: pending_selection = MENU_OPTIONS; break;
            case 2: pending_selection = MENU_INFO; break;
            case 3: pending_selection = MENU_QUIT; break;
            }
            palette_start_fade_out(FADE_STEPS);
            state = MENU_FADE_OUT;
        }

        /* ESC / Cancel / F10 = quit */
        if (input_pressed(INPUT_CANCEL) || input_pressed(INPUT_QUIT)) {
            pending_selection = MENU_QUIT;
            palette_start_fade_out(FADE_STEPS);
            state = MENU_FADE_OUT;
        }
        break;

    case MENU_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            return pending_selection;
        }
        break;
    }

    return MENU_NONE;
}

void menu_draw(void)
{
    /* Background */
    DrawTexture(bg_tex, 0, 0, WHITE);

    /* Title text with 3-pass shadow (from FUN_1000_1340):
     * Color 10 (green) at Y-1, color 8 (dark grey) at Y+1, color 0 (black) at Y+0.
     * X centered: (26 - len) * 4 + 253
     * Y = 437 */
    if (state != MENU_FADE_IN || !palette_is_fading()) {
        int len = (int)strlen(title_text);
        int text_x = (26 - len) * 4 + 253;
        int text_y = 437;

        Color green = palette_get_color(10);
        Color grey  = palette_get_color(8);
        Color black = palette_get_color(0);

        DrawTextFON(&font, title_text, text_x, text_y - 1, green);
        DrawTextFON(&font, title_text, text_x, text_y + 1, grey);
        DrawTextFON(&font, title_text, text_x, text_y,     black);
    }

    /* Cursor sprite */
    DrawTexture(cursor_tex, CURSOR_X, item_y[current_item], WHITE);
}

void menu_cleanup(void)
{
    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;

    UnloadTexture(cursor_tex);
    UnloadImage(cursor_img);

    UnloadFON(&font);
}
