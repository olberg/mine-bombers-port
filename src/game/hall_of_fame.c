/* Hall of Fame (high score) screen.
 * Decompiled ref:
 *   FUN_1000_a619 (load)  seg_1000:6713-6744
 *   FUN_1000_a6d7 (save)  seg_1000:6748-6765
 *   FUN_1000_a738 (sort)  seg_1000:6769-6838
 *   FUN_1000_a93f (draw)  seg_1000:6842-6912
 *   FUN_1000_aad7 (main)  seg_1000:6916-6953
 *
 * Data format: 10 entries × 26 bytes = 260 bytes in halloffa.dat
 * Each entry: name_len(1) + name(20) + level(1) + score_lo(2) + score_hi(2)
 *
 * Sort: two-pass bubble sort.
 *   Pass 1: group by name[0] (first byte of name, descending)
 *   Pass 2: within same name[0], sort by score descending (hi word, then lo word)
 *
 * Display: one concatenated string per line at X=0x7F (127),
 *   Y = rank*10 + 0xA9 (169) for rank 1..10.
 *   Format: "rank  name                  Level N  Money NNNNN"
 */

#include "game/hall_of_fame.h"
#include "loaders/spy_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FADE_STEPS   7

/* Display layout from decompiled FUN_1000_a93f (seg_1000:6842-6912):
 * All text drawn at X=0x7F (127). Y = rank*10 + 0xA9.
 * The original concatenates substrings into one long string per line. */
#define HOF_TEXT_X  127

typedef enum {
    HOF_FADE_IN,
    HOF_WAIT_KEY,
    HOF_FADE_OUT,
    HOF_FINISHED
} HofState;

/* Background */
static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

/* Font */
static BitmapFont font;

/* Data: 11 slots (10 existing + 1 candidate). After sort, only top 10 are kept. */
#define HOF_SORT_SLOTS  (HOF_ENTRIES + 1)
static HofEntry entries[HOF_SORT_SLOTS];
static int new_entry_index;  /* -1 if player didn't qualify */
static HofState state;

/* --- File I/O --- */

static void hof_load_dat(const char *path)
{
    memset(entries, 0, sizeof(entries));

    FILE *f = fopen(path, "rb");
    if (!f) return;

    for (int i = 0; i < HOF_ENTRIES; i++) {
        if (fread(&entries[i], HOF_ENTRY_SIZE, 1, f) != 1) break;
    }
    fclose(f);
}

static void hof_save_dat(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;

    for (int i = 0; i < HOF_ENTRIES; i++) {
        fwrite(&entries[i], HOF_ENTRY_SIZE, 1, f);
    }
    fclose(f);
}

/* --- Sort matching decompiled FUN_1000_a738 (seg_1000:6769-6838) ---
 *
 * Two-pass bubble sort on 11 entries (indices 1..10 in original, 0..10 here):
 *   Pass 1: Sort by name[0] (first char) descending — higher byte values first.
 *           This groups entries by player name.
 *   Pass 2: Within entries sharing the same name[0], sort by score descending
 *           (high word first, then low word).
 */
static void hof_sort(int count)
{
    bool swapped;

    /* Pass 1: bubble sort by name[0] descending */
    do {
        swapped = false;
        for (int i = 1; i < count; i++) {
            /* Original: if prev name[0] < curr name[0], swap */
            uint8_t a = (uint8_t)entries[i - 1].name[0];
            uint8_t b = (uint8_t)entries[i].name[0];
            /* Treat empty entries (name_len==0) as having name[0]=0 */
            if (entries[i - 1].name_len == 0) a = 0;
            if (entries[i].name_len == 0) b = 0;
            if (a < b) {
                HofEntry tmp = entries[i - 1];
                entries[i - 1] = entries[i];
                entries[i] = tmp;
                swapped = true;
            }
        }
    } while (swapped);

    /* Pass 2: within same name[0], sort by score descending */
    do {
        swapped = false;
        for (int i = 1; i < count; i++) {
            uint8_t na = entries[i - 1].name_len > 0 ? (uint8_t)entries[i - 1].name[0] : 0;
            uint8_t nb = entries[i].name_len > 0 ? (uint8_t)entries[i].name[0] : 0;
            if (na != nb) continue;  /* different name groups */

            int16_t hi_a = entries[i - 1].score_hi;
            int16_t hi_b = entries[i].score_hi;
            int16_t lo_a = entries[i - 1].score_lo;
            int16_t lo_b = entries[i].score_lo;

            /* Swap if prev score < curr score (to put higher scores first) */
            bool should_swap = false;
            if (hi_a < hi_b) {
                should_swap = true;
            } else if (hi_a == hi_b && (uint16_t)lo_a < (uint16_t)lo_b) {
                should_swap = true;
            }

            if (should_swap) {
                HofEntry tmp = entries[i - 1];
                entries[i - 1] = entries[i];
                entries[i] = tmp;
                swapped = true;
            }
        }
    } while (swapped);
}

static int32_t hof_score(const HofEntry *e)
{
    return ((int32_t)(uint16_t)e->score_hi << 16) | (uint16_t)e->score_lo;
}

/* --- Public API --- */

void hof_init(const char *player_name, int level_reached, int32_t score)
{
    /* Load background: HALLOFFA.SPY */
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/HALLOFFA.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    /* Load font */
    font = LoadFON("assets/FONTTI.FON", true);

    /* Load existing hall of fame data into slots 0..9 */
    hof_load_dat("assets/HALLOFFA.DAT");

    new_entry_index = -1;

    if (player_name && player_name[0]) {
        /* Create candidate entry in slot 10 (the 11th slot).
         * Matches FUN_1000_aad7: copies player name, level, score into a
         * local record, then calls sort which inserts it. */
        HofEntry *cand = &entries[HOF_ENTRIES];
        memset(cand, 0, sizeof(*cand));
        int name_len = (int)strlen(player_name);
        if (name_len > HOF_NAME_LEN) name_len = HOF_NAME_LEN;
        cand->name_len = (uint8_t)name_len;
        memcpy(cand->name, player_name, name_len);
        /* Pad with spaces (matching rtl_strcopy(0x14,...) behavior) */
        for (int i = name_len; i < HOF_NAME_LEN; i++)
            cand->name[i] = ' ';
        cand->level = (uint8_t)level_reached;
        cand->score_lo = (int16_t)(score & 0xFFFF);
        cand->score_hi = (int16_t)((score >> 16) & 0xFFFF);

        /* Sort all 11 entries, then discard the last one (keep top 10) */
        hof_sort(HOF_SORT_SLOTS);

        /* Find where candidate ended up (in the top 10) */
        for (int i = 0; i < HOF_ENTRIES; i++) {
            if (entries[i].name_len == cand->name_len &&
                entries[i].level == cand->level &&
                entries[i].score_lo == cand->score_lo &&
                entries[i].score_hi == cand->score_hi &&
                memcmp(entries[i].name, cand->name, HOF_NAME_LEN) == 0) {
                new_entry_index = i;
                break;
            }
        }

        /* Clear slot 10 so it doesn't leak into display */
        memset(&entries[HOF_ENTRIES], 0, sizeof(HofEntry));

        /* Save the updated top 10 */
        hof_save_dat("assets/HALLOFFA.DAT");
    }

    /* Start fade in */
    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);
    state = HOF_FADE_IN;
}

HofResult hof_update(void)
{
    switch (state) {
    case HOF_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = HOF_WAIT_KEY;
        }
        break;

    case HOF_WAIT_KEY:
        if (input_pressed(INPUT_CONFIRM) || input_pressed(INPUT_CANCEL) ||
            input_pressed(INPUT_ANY_KEY)) {
            palette_start_fade_out(FADE_STEPS);
            state = HOF_FADE_OUT;
        }
        break;

    case HOF_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = HOF_FINISHED;
            return HOF_DONE;
        }
        break;

    case HOF_FINISHED:
        return HOF_DONE;
    }

    return HOF_ACTIVE;
}

void hof_draw(void)
{
    /* Background */
    if (bg_tex.id) {
        DrawTexture(bg_tex, 0, 0, WHITE);
    }

    /* Don't draw text during fade-in (original draws after fade completes) */
    if (state == HOF_FADE_IN && palette_is_fading()) return;

    /* Original uses set_draw_color(1) = palette index 1 */
    Color text_color = palette_get_color(1);

    char line[128];

    for (int i = 0; i < HOF_ENTRIES; i++) {
        /* Y = (rank * 10) + 0xA9.  rank is 1-based (i+1). */
        int y = (i + 1) * 10 + 0xa9;

        /* Build concatenated line matching original FUN_1000_a93f format:
         *   rank(4 chars) + name(21 chars padded) + " Level "(9 chars)
         *   + level_num + " Money "(13 chars) + score
         *
         * Original allocates:
         *   local_38 = rank string (4 chars: "NN. ")
         *   local_1a = name string (21 chars padded with spaces)
         *   local_24 = " Level NN" (9 chars)
         *   local_32 = " Money NNNN" (13 chars)
         * Then concatenates: rank + name + level + money */

        char rank_str[8];
        snprintf(rank_str, sizeof(rank_str), "%2d. ", i + 1);

        char name_str[HOF_NAME_LEN + 2];
        memset(name_str, ' ', HOF_NAME_LEN + 1);
        name_str[HOF_NAME_LEN + 1] = '\0';
        if (entries[i].name_len > 0) {
            int len = entries[i].name_len;
            if (len > HOF_NAME_LEN) len = HOF_NAME_LEN;
            memcpy(name_str, entries[i].name, len);
        }

        char level_str[16];
        if (entries[i].level > 0) {
            snprintf(level_str, sizeof(level_str), " Level %d", entries[i].level);
        } else {
            level_str[0] = '\0';
        }

        char money_str[24];
        int32_t sc = hof_score(&entries[i]);
        if (sc > 0 || entries[i].name_len > 0) {
            snprintf(money_str, sizeof(money_str), " Money %ld", (long)sc);
        } else {
            money_str[0] = '\0';
        }

        snprintf(line, sizeof(line), "%s%s%s%s", rank_str, name_str,
                 level_str, money_str);

        /* Highlight the new entry */
        Color c = text_color;
        if (i == new_entry_index) {
            c = palette_get_color(14);  /* bright color for highlighting */
        }

        DrawTextFON(&font, line, HOF_TEXT_X, y, c);
    }
}

void hof_cleanup(void)
{
    if (bg_tex.id) UnloadTexture(bg_tex);
    if (bg_img.data) UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;
    bg_tex = (Texture2D){0};
    bg_img = (Image){0};

    UnloadFON(&font);
}
