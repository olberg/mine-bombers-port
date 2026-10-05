/* Map picker screen — lets the player select maps for each multiplayer round.
 *
 * Decompiled ref: FUN_1010_e231 (seg_1010:8533-8611)
 * Navigation: FUN_1010_dfee (seg_1010:8447-8529)
 * Grid drawing: FUN_1010_de6a (seg_1010:8372-8406)
 * Cell highlight: FUN_1010_dd1c (seg_1010:8319-8367)
 * Map count: FUN_1010_dc87 (seg_1010:8278-8287)
 *
 * Layout: 8 columns × N rows of map names.
 * Grid cell position: Y = row * 10 + 74, X = col * 80.
 * Colors: selected maps = 7 (yellow), unselected = 1 (blue),
 *         cursor on unselected = 4 (red), cursor on selected = 5.
 *
 * The original stores per-round map selections in g_high_score_table
 * (misnamed by decompiler). Grid cell 0 is the "Random" pseudo-map (name
 * table entry 0 = fixed string "Random", maps at 1..N sorted). On picker
 * entry all slots are zeroed; Enter/Space appends the cursor's grid index
 * to the next round slot; the fill key (code -0x61 = F1) overwrites
 * ALL round slots with unique random grid cells (range includes "Random");
 * ESC or code -0x58 (F10) exits, converting still-0 slots to 32000.
 * Key codes come from check_keypress (seg_1008:3420): ReadKey, and for an
 * extended key the scan code + 100 (F1 = 59 + 100 = 0x9F = -0x61,
 * F10 = 68 + 100 = 0xA8 = -0x58; arrows are 0xAC/0xB4/0xAF/0xB1 the same
 * way). The on-screen help reads "F1 = Random select" / "ESC = Done".
 * Round-loop gate: slot < 30000 → load that map, else random
 * (seg_1000:7082). There is no undo key in the original.
 */

#include "game/map_picker.h"
#include "game/map_list.h"
#include "game/map.h"
#include "game/map_thumbnail.h"
#include "game/config.h"
#include "loaders/spy_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util/prng.h"

#define FADE_STEPS  7

/* Grid layout constants from decompiled code */
#define GRID_COLS    8
#define GRID_X_START 0     /* col * 80 (0x50) */
#define GRID_X_STEP  80    /* 0x50 */
#define GRID_Y_START 74    /* 0x4A */
#define GRID_Y_STEP  10    /* 10px per row */

/* Cursor bar behind the highlighted name */
#define GRID_BAR_W   71
#define GRID_BAR_H   9

/* Round counter display position */
#define COUNTER_X    15    /* 0x0F */
#define COUNTER_Y    15    /* 0x0F */

typedef enum {
    MPICK_FADE_IN,
    MPICK_SELECTING,
    MPICK_FADE_OUT
} MPickState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static BitmapFont mpick_font;

static MPickState pick_state;

/* Cursor position in the grid (col, row) */
static int cursor_col;
static int cursor_row;

/* How many rounds have been assigned so far */
static int assigned_count;

/* Per-round map selection: index into map_list, or MAP_PICK_RANDOM */
static int round_selections[MAP_PICKER_MAX_ROUNDS];

/* Total number of maps available */
static int total_maps;

/* Total number of grid rows (maps / GRID_COLS, rounded up) */
static int total_rows;

/* Check if a given grid index is already assigned to any round in
 * [0, assigned_count). Matches FUN_1010_dcc2 (grid index 0 = "Random"
 * participates in the check like any map). */
static bool is_grid_assigned(int grid_index)
{
    for (int i = 0; i < assigned_count; i++) {
        if (round_selections[i] >= 0 && round_selections[i] == grid_index) {
            return true;
        }
    }
    return false;
}

/* Convert grid (row, col) to a flat grid index (0 = "Random" cell). */
static int grid_to_index(int row, int col)
{
    return row * GRID_COLS + col;
}

void map_picker_reset(void)
{
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        round_selections[i] = MAP_PICK_RANDOM;
    }
    assigned_count = 0;
}

void map_picker_session_begin(void)
{
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        round_selections[i] = 0;
    }
    assigned_count = 0;
}

void map_picker_assign_grid(int grid_index, int total_rounds)
{
    if (assigned_count < total_rounds &&
        assigned_count < MAP_PICKER_MAX_ROUNDS) {
        round_selections[assigned_count] = grid_index;
        assigned_count++;
    }
}

void map_picker_fill_random(int total_rounds, int map_count)
{
    /* The original fills slots 0..min(total_rounds, 0x1F0)-1 — writing past
     * its 56-entry table when total_rounds > 56 (an original out-of-bounds
     * bug, unreachable in practice because game_state_update clamps
     * total_rounds to 55 at round start). The port clamps to the table. */
    int fill = total_rounds;
    if (fill > MAP_PICKER_MAX_ROUNDS) fill = MAP_PICKER_MAX_ROUNDS;

    for (int r = 0; r < fill; r++) {
        for (int attempt = 0; attempt <= 100; attempt++) {
            /* Random(count + 1): 0..count — includes the Random cell */
            round_selections[r] = mb_random(map_count + 1);
            bool unique = true;
            for (int prev = 0; prev < r; prev++) {
                if (round_selections[prev] >= 0 &&
                    round_selections[prev] == round_selections[r]) {
                    unique = false;
                    break;
                }
            }
            if (unique) break;
        }
    }
    assigned_count = fill;
}

void map_picker_finalize(void)
{
    for (int i = 0; i < MAP_PICKER_MAX_ROUNDS; i++) {
        if (round_selections[i] == 0) {
            round_selections[i] = MAP_PICK_RANDOM;
        }
    }
}

int map_picker_assigned_count(void)
{
    return assigned_count;
}

/* Map under the cursor for the preview box (FUN_1010_db96,
 * seg_1010:8203-8271), reloaded when the cursor moves to another cell. */
#define PREVIEW_UNSET (-2)
static TileMap preview_map;
static int  preview_index = PREVIEW_UNSET;  /* map_list index, -1 = "Random" */
static bool preview_ok;

static void update_preview(void)
{
    int cur_idx = grid_to_index(cursor_row, cursor_col);
    int want = (cur_idx >= 1 && cur_idx < total_maps + 1) ? cur_idx - 1 : -1;
    if (want == preview_index) return;

    preview_index = want;
    preview_ok = false;
    if (want >= 0) {
        char path[64];
        preview_ok = map_list_build_path(path, sizeof(path), "assets", want) &&
                     map_load(&preview_map, path);
    }
}

void map_picker_init(void)
{
    total_maps = map_list_count();

    if (total_maps < 1) {
        /* No maps available — will show error and exit immediately */
        pick_state = MPICK_FADE_OUT;
        return;
    }

    /* +1: grid cell 0 is the "Random" pseudo-map (seg_1010:2630-2631
     * writes the fixed string "Random" — MB.EXE bytes at 98327 — into
     * name-table entry 0; real maps occupy entries 1..N). */
    total_rows = (total_maps + 1 + GRID_COLS - 1) / GRID_COLS;

    /* Load LEVSELEC.SPY background */
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/LEVSELEC.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    mpick_font = LoadFON("assets/FONTTI.FON", true);

    /* Picker entry zeroes the table — reopening DISCARDS previous
     * selections (seg_1010:8563-8566). */
    map_picker_session_begin();

    cursor_col = 0;
    cursor_row = 0;
    preview_index = PREVIEW_UNSET;
    update_preview();

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    pick_state = MPICK_FADE_IN;
}

MapPickerResult map_picker_update(void)
{
    switch (pick_state) {
    case MPICK_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            pick_state = MPICK_SELECTING;
        }
        break;

    case MPICK_SELECTING: {
        /* Grid cell count includes the "Random" pseudo-map at index 0. */
        int grid_count = total_maps + 1;

        /* RIGHT ('6') — move cursor right within row (seg_1010:8459-8467) */
        if (input_pressed(INPUT_RIGHT)) {
            int cur_idx = grid_to_index(cursor_row, cursor_col);
            if (cur_idx < grid_count - 1 && cursor_col < GRID_COLS - 1) {
                cursor_col++;
            }
        }

        /* LEFT ('4') — move cursor left */
        if (input_pressed(INPUT_LEFT)) {
            if (cursor_col > 0) {
                cursor_col--;
            }
        }

        /* DOWN ('2') — move cursor down one row (row cap 0x29 = 41,
         * seg_1010:8468-8478) */
        if (input_pressed(INPUT_DOWN)) {
            int next_idx = grid_to_index(cursor_row + 1, cursor_col);
            if (cursor_row < 0x29 && next_idx <= grid_count - 1) {
                cursor_row++;
            }
        }

        /* UP ('8') — move cursor up one row */
        if (input_pressed(INPUT_UP)) {
            if (cursor_row > 0) {
                cursor_row--;
            }
        }

        /* ENTER / SPACE — assign current grid cell (incl. "Random") to the
         * next round (seg_1010:8493-8498) */
        if (input_pressed(INPUT_CONFIRM) || IsKeyPressed(KEY_SPACE)) {
            map_picker_assign_grid(grid_to_index(cursor_row, cursor_col),
                                   g_config.total_rounds);
        }

        /* Random-fill key F1 (code -0x61) — overwrite ALL rounds with random
         * unique grid cells (seg_1010:8501-8523). */
        if (IsKeyPressed(KEY_F1)) {
            map_picker_fill_random(g_config.total_rounds, total_maps);
        }

        /* NOTE: the original has NO undo key — a previous port version
         * had Backspace-undo here; removed for fidelity. */

        /* ESC or F10 (code -0x58) — exit map picker */
        if (input_pressed(INPUT_CANCEL) || input_pressed(INPUT_QUIT)) {
            map_picker_finalize();
            palette_start_fade_out(FADE_STEPS);
            pick_state = MPICK_FADE_OUT;
        }
        break;
    }

    case MPICK_FADE_OUT:
        palette_update();
        if (bg_indexed) {
            palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                    SPY_WIDTH * SPY_HEIGHT);
            UpdateTexture(bg_tex, bg_img.data);
        }
        if (!palette_is_fading()) {
            return MAP_PICKER_DONE;
        }
        break;
    }

    update_preview();
    return MAP_PICKER_NONE;
}

void map_picker_draw(void)
{
    if (bg_indexed) {
        DrawTexture(bg_tex, 0, 0, WHITE);
    }

    if (pick_state == MPICK_FADE_IN && palette_is_fading()) return;
    if (total_maps < 1) return;

    Color col_normal = palette_get_color(1);       /* unselected */
    Color col_selected = palette_get_color(7);     /* already assigned */
    Color col_cursor = palette_get_color(4);       /* "Random" cell, unassigned */
    Color col_cursor_sel = palette_get_color(5);   /* "Random" cell, assigned */
    Color col_bar_text = palette_get_color(0);     /* cursor cell text, unassigned */
    Color col_bar_text_sel = palette_get_color(6); /* cursor cell text, assigned */
    Color col_bar = palette_get_color(1);          /* cursor bar */

    /* Draw the grid: cell 0 = "Random", cells 1..N = map names.
     * The "Random" cell is ALWAYS drawn in colors 4/5, cursor or not —
     * FUN_1010_dd1c/de6a special-case row+col == 0. The cell under the cursor
     * is drawn on a 71x9 bar (FUN_1010_dd1c: fill_rect(row*10+0x52,
     * col*0x50+0x46, row*10+0x4a, col*0x50)) in colors 0 (unassigned) or 6
     * (assigned). */
    for (int r = 0; r < total_rows; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            int grid_idx = grid_to_index(r, c);
            if (grid_idx >= total_maps + 1) break;

            const char *name = (grid_idx == 0) ? "Random"
                                               : map_list_name(grid_idx - 1);
            if (!name) continue;

            int x = GRID_X_START + c * GRID_X_STEP;
            int y = GRID_Y_START + r * GRID_Y_STEP;

            bool is_cursor = (r == cursor_row && c == cursor_col);
            bool assigned = is_grid_assigned(grid_idx);

            Color col;
            if (is_cursor) {
                DrawRectangle(x, y, GRID_BAR_W, GRID_BAR_H, col_bar);
            }
            if (grid_idx == 0) {
                col = assigned ? col_cursor_sel : col_cursor;
            } else if (is_cursor) {
                col = assigned ? col_bar_text_sel : col_bar_text;
            } else {
                col = assigned ? col_selected : col_normal;
            }

            DrawTextFON(&mpick_font, name, x, y, col);
        }
    }

    /* Selected counter: just the number of rounds assigned so far
     * (seg_1010:8574-8582, number printed at (15, 15) in color 1). */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", assigned_count);
        DrawTextFON(&mpick_font, buf, COUNTER_X, COUNTER_Y, col_normal);
    }

    /* Preview of the map under the cursor, one pixel per tile. For
     * "Random", or a file that cannot be read, the original fills the box
     * with colour 0 instead: fill_rect(0x33, 0x189, 7, 0x14a), corners
     * inclusive (seg_1010:8268-8271). */
    if (preview_ok) {
        map_thumbnail_draw(&preview_map, MAP_PICKER_PREVIEW_X,
                           MAP_PICKER_PREVIEW_Y);
    } else {
        DrawRectangle(MAP_PICKER_PREVIEW_X, MAP_PICKER_PREVIEW_Y,
                      MAP_ROWS, MAP_COLS, palette_get_color(0));
    }
}

void map_picker_cleanup(void)
{
    if (bg_indexed) {
        UnloadTexture(bg_tex);
        UnloadImage(bg_img);
        free(bg_indexed);
        bg_indexed = NULL;
    }
    UnloadFON(&mpick_font);
}

const int *map_picker_get_selections(void)
{
    return round_selections;
}

bool map_picker_has_selection(int round_index)
{
    if (round_index < 0 || round_index >= MAP_PICKER_MAX_ROUNDS) return false;
    /* Original round-loop gate: table[round] < 30000 → picked map
     * (seg_1000:7082). Slot 0 ("Random"/unassigned) cannot survive
     * finalize, but mirror the original by treating it as random too. */
    int sel = round_selections[round_index];
    return sel >= 1 && sel < 30000;
}

int map_picker_get_map_index(int round_index)
{
    if (!map_picker_has_selection(round_index)) return -1;
    /* Slot values are 1-based grid indices ("Random" occupies 0). */
    return round_selections[round_index] - 1;
}
