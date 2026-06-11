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
 * to the next round slot; the fill key (code -0x61, port: F11) overwrites
 * ALL round slots with unique random grid cells (range includes "Random");
 * ESC (or code -0x58, port: F12) exits, converting still-0 slots to 32000.
 * Round-loop gate: slot < 30000 → load that map, else random
 * (seg_1000:7082). There is no undo key in the original.
 */

#include "game/map_picker.h"
#include "game/map_list.h"
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

/* Round counter display position */
#define COUNTER_X    15    /* 0x0F */
#define COUNTER_Y    12    /* 0x0C */

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
        if (input_pressed(INPUT_CONFIRM)) {
            map_picker_assign_grid(grid_to_index(cursor_row, cursor_col),
                                   g_config.total_rounds);
        }

        /* Random-fill key — overwrite ALL rounds with random unique grid
         * cells (seg_1010:8501-8523). The original triggers on its key
         * code -0x61; F11 is the port's mapping (identity unverified). */
        if (IsKeyPressed(KEY_F11)) {
            map_picker_fill_random(g_config.total_rounds, total_maps);
        }

        /* NOTE: the original has NO undo key — a previous port version
         * had Backspace-undo here; removed for fidelity. */

        /* ESC — exit map picker (the original also exits on key code
         * -0x58, likely F12) */
        if (input_pressed(INPUT_CANCEL) || IsKeyPressed(KEY_F12)) {
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

    return MAP_PICKER_NONE;
}

void map_picker_draw(void)
{
    if (bg_indexed) {
        DrawTexture(bg_tex, 0, 0, WHITE);
    }

    if (pick_state == MPICK_FADE_IN && palette_is_fading()) return;
    if (total_maps < 1) return;

    Color col_normal = palette_get_color(1);      /* blue — unselected */
    Color col_selected = palette_get_color(7);     /* yellow — already assigned */
    Color col_cursor = palette_get_color(4);       /* red — cursor on unselected */
    Color col_cursor_sel = palette_get_color(5);   /* magenta — cursor on selected */

    /* Draw the grid: cell 0 = "Random", cells 1..N = map names.
     * The "Random" cell is ALWAYS drawn in the cursor colors (4/5),
     * cursor or not — FUN_1010_dd1c/de6a special-case row+col == 0 —
     * which makes the cursor invisible while it sits on that cell,
     * exactly as in the original. */
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
            if (grid_idx == 0 || is_cursor) {
                col = assigned ? col_cursor_sel : col_cursor;
            } else {
                col = assigned ? col_selected : col_normal;
            }

            DrawTextFON(&mpick_font, name, x, y, col);
        }
    }

    /* Draw round counter (how many rounds assigned / total) */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", assigned_count, g_config.total_rounds);
        DrawTextFON(&mpick_font, buf, COUNTER_X, COUNTER_Y, col_normal);
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
