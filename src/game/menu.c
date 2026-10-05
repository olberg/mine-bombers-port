#include "menu.h"
#include "loaders/spy_loader.h"
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
/* Sprite sheet origin of the shovel cursor (DAT_1038_0678 sprite). */
#define CURSOR_SHEET_X 150
#define CURSOR_SHEET_Y 140
/* Erase rectangle FUN_1000_1240 fills: x 0xde..0x11f, y item_y..item_y+20. */
#define ERASE_W       66
#define ERASE_H       21

/* Menu item Y positions.
 * Original menu items (seg_1000:824 main_menu): Play, Options, Info, Quit.
 * Cursor Y for each item is drawn by FUN_1000_12bc at seg_1000:713-724:
 *   item 1 → 0x88 = 136, item 2 → 0xb8 = 184,
 *   item 3 → 0xe8 = 232, item 4 → 0x118 = 280
 * — uniform 48-px spacing. Map Select is reached via Options
 * (seg_1000:544-549 item 0xC invokes FUN_1010_e231), not from top level. */
static const int item_y[MENU_ITEMS] = { 136, 184, 232, 280 };

/* "Registered to" line. The original prints the registration name it reads
 * from REGISTER.DAT into DS:0x3e0 (FUN_1010_02fc, seg_1010:120; a Pascal
 * string of up to 26 characters — "Freeware" in the 3.11 release).
 * Deliberate deviation: the port prints its own text there so the screen
 * says it is a port. */
static const char *registered_to = "Mine Bombers 3.11 port";

typedef enum { MENU_FADE_IN, MENU_ACTIVE, MENU_FADE_OUT } MenuState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

/* The screen is kept as palette indices: the original draws the cursor
 * straight into video memory, so the shovel shares the menu palette and the
 * fades, and its opaque 65x20 blit and 66x21 black erase leave marks in the
 * rim of the holes (see cursor_blit / cursor_erase). */
static uint8_t *screen_indexed;
static uint8_t cursor_pixels[CURSOR_W * CURSOR_H];

static BitmapFont font;

static int current_item;
static int prev_item;
/* main_menu (seg_1000:824-863) keeps its cursor while the Options and Info
 * sub-screens run and redraws it on return (FUN_1000_13d0); every other
 * entry starts on New game. */
static int remembered_item;
static MenuState state;
static MenuSelection pending_selection;

/* FUN_1000_12bc (seg_1000:700-726): blit_sprite of the whole 65x20 cursor
 * rectangle at (222, item_y), index 0 included — it is not a masked blit, so
 * the sprite's black pixels overwrite the few rim pixels of the hole that
 * fall inside the rectangle. */
static void cursor_blit(int item)
{
    for (int row = 0; row < CURSOR_H; row++) {
        memcpy(&screen_indexed[(item_y[item] + row) * SPY_WIDTH + CURSOR_X],
               &cursor_pixels[row * CURSOR_W], CURSOR_W);
    }
}

/* FUN_1000_1240 (seg_1000:674-696): fill_rect (222, item_y) .. (287, item_y+20)
 * with color 0 — a rectangle one pixel wider and taller than the sprite. */
static void cursor_erase(int item)
{
    for (int row = 0; row < ERASE_H; row++) {
        memset(&screen_indexed[(item_y[item] + row) * SPY_WIDTH + CURSOR_X], 0, ERASE_W);
    }
}

static void refresh_texture(void)
{
    palette_apply_to_pixels(screen_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
    UpdateTexture(bg_tex, bg_img.data);
}

void menu_init(void)
{
    /* Load menu background */
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/MAIN3.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    /* Load sprite sheet and extract the cursor as palette indices */
    uint8_t sheet_palette[768];
    uint8_t *sheet_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    Image sheet_img = LoadSPY("assets/SIKA.SPY", sheet_palette, sheet_indexed);
    for (int row = 0; row < CURSOR_H; row++) {
        memcpy(&cursor_pixels[row * CURSOR_W],
               &sheet_indexed[(CURSOR_SHEET_Y + row) * SPY_WIDTH + CURSOR_SHEET_X],
               CURSOR_W);
    }
    free(sheet_indexed);
    UnloadImage(sheet_img);

    screen_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    memcpy(screen_indexed, bg_indexed, SPY_WIDTH * SPY_HEIGHT);

    /* Load font */
    font = LoadFON("assets/FONTTI.FON", true);

    /* Init palette and start fade in */
    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    current_item = remembered_item;
    prev_item = current_item;
    remembered_item = 0;
    state = MENU_FADE_IN;
    pending_selection = MENU_NONE;

    cursor_blit(current_item);
    refresh_texture();
}

MenuSelection menu_update(void)
{
    switch (state) {
    case MENU_FADE_IN:
        palette_update();
        refresh_texture();
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
        if (current_item != prev_item) {
            /* seg_1000:845-854: erase the previous position, blit the new one */
            cursor_erase(prev_item);
            cursor_blit(current_item);
            refresh_texture();
            prev_item = current_item;
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
        refresh_texture();
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

    /* Registration name with 3-pass shadow (from FUN_1000_1340):
     * Color 10 (green) at Y-1, color 8 (dark grey) at Y+1, color 0 (black) at Y+0.
     * X centered: (26 - len) * 4 + 253
     * Y = 437 */
    if (state != MENU_FADE_IN || !palette_is_fading()) {
        int len = (int)strlen(registered_to);
        int text_x = (26 - len) * 4 + 253;
        int text_y = 437;

        Color green = palette_get_color(10);
        Color grey  = palette_get_color(8);
        Color black = palette_get_color(0);

        DrawTextFON(&font, registered_to, text_x, text_y - 1, green);
        DrawTextFON(&font, registered_to, text_x, text_y + 1, grey);
        DrawTextFON(&font, registered_to, text_x, text_y,     black);
    }
}

void menu_cleanup(void)
{
    remembered_item = (pending_selection == MENU_OPTIONS ||
                       pending_selection == MENU_INFO) ? current_item : 0;

    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;

    free(screen_indexed);
    screen_indexed = NULL;

    UnloadFON(&font);
}
