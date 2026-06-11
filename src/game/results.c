#include "game/results.h"
#include "game/config.h"
#include "game/player_db.h"
#include "audio/sfx.h"
#include "loaders/spy_loader.h"
#include "loaders/pcx_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FADE_STEPS       7
#define WAIT_FRAMES    180  /* 3 seconds at 60fps */

/* Color prefixes for each player slot (Finnish color names) */
static const char *COLOR_PREFIX[MAX_PLAYERS] = { "sin", "pun", "vih", "kel" };

/* Result suffixes */
static const char *RESULT_SUFFIX[] = { "voit", "draw", "draw", "lose" };

/* Column layout (seg_1000:6087-6373, DOSBox-verified against f_108,
 * 2026-06-11). The screen is four COLUMNS on the FINAL.SPY stone tablet:
 * the 132x219 PCX portrait draws at Y=95 (FUN_1010_7785(1, 0x5f, X, file)
 * — Y before X), the masked name prints at Y=330, the wallet at Y=346 and
 * the round wins at Y=362 next to the $ / trophy icons baked into the
 * background. All text is draw color 1; there are no "Cash:" labels. */
static const int PORTRAIT_X[MAX_PLAYERS] = { 32, 182, 334, 484 };  /* 0x20 0xb6 0x14e 0x1e4 */
#define PORTRAIT_Y    95   /* 0x5f */
static const int RES_NAME_X[MAX_PLAYERS] = { 17, 167, 321, 471 }; /* 0x11 0xa7 0x141 0x1d7 */
#define RES_NAME_Y   330   /* 0x14a */
static const int RES_VAL_X[MAX_PLAYERS] = { 36, 186, 338, 488 }; /* 0x24 0xba 0x152 0x1e8 */
#define RES_CASH_Y   346   /* 0x15a */
#define RES_WINS_Y   362   /* 0x16a */

typedef enum {
    RES_FADE_IN,
    RES_WAIT_TIMER,
    RES_WAIT_KEY,
    RES_FADE_OUT,
    RES_FINISHED
} ResultsState;

/* Rank: 0=win, 1/2=draw, 3=lose */
typedef struct {
    int    rank;
    char   name[27];
    int32_t score;      /* wallet (DAT_1038_1cd4, seg_1000:6151) */
    int16_t round_wins;
    bool   active;
} PlayerResult;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static Image portrait_imgs[MAX_PLAYERS];
static Texture2D portrait_texs[MAX_PLAYERS];

static BitmapFont res_font;

static PlayerResult results[MAX_PLAYERS];
static int num_results;
static ResultsState state;
static int timer;

/* Determine player rank = number of players with a strictly higher metric.
 * The results screen switches on the winner-by option (g_player4_color =
 * option_toggle[3], seg_1000:6089/6111): 0 → rank by 32-bit WALLET
 * (FUN_1000_96c6), nonzero → rank by ROUND WINS (FUN_1000_9640). Both
 * compare all player slots with strict greater-than (self never counts),
 * and rank+1 == num_players → 3 (sole loser). 0=winner, 1/2=draw, 3=loser.
 * */
int results_compute_rank(int player_idx, const Player players[],
                         int num_players)
{
    bool by_wins = g_config.option_toggle[3] != 0;
    int higher = 0;

    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (i == player_idx) continue;
        if (by_wins) {
            if (players[player_idx].round_wins < players[i].round_wins) {
                higher++;
            }
        } else {
            if (players[player_idx].cash < players[i].cash) {
                higher++;
            }
        }
    }

    if ((higher + 1) == num_players) {
        return 3;
    }
    return higher;
}

void results_accumulate_match_stats(Player players[], int num_players)
{
    /* The original's results screen (multiplayer_round_end_scoring,
     * seg_1000:6070-6400) increments match-stats dword 0 (matches played)
     * for every player (seg_1000:6157) and dword 1 (matches won) for the
     * rank-0 player only (seg_1000:6093/6115). Runs once per match. */
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        players[i].match_stats[STAT_MATCHES] += 1;
        if (results_compute_rank(i, players, num_players) == 0) {
            players[i].match_stats[STAT_MATCH_WINS] += 1;
        }
        /* The original ASSIGNS the match's round-win count into
         * match-stats dword 3 here (seg_1000:6374-6393), so the record
         * accumulates career round wins at the post-match merge. */
        players[i].match_stats[STAT_ROUND_WINS] =
            (uint32_t)players[i].round_wins;
    }
}

void results_init(const Player players[], int num_players)
{
    num_results = num_players;
    if (num_results > MAX_PLAYERS) num_results = MAX_PLAYERS;

    /* Compute rankings */
    for (int i = 0; i < num_results; i++) {
        results[i].active = true;
        strncpy(results[i].name, players[i].name, sizeof(results[i].name) - 1);
        results[i].name[sizeof(results[i].name) - 1] = '\0';
        results[i].score = players[i].cash;
        results[i].round_wins = players[i].round_wins;
        results[i].rank = results_compute_rank(i, players, num_results);
    }
    for (int i = num_results; i < MAX_PLAYERS; i++) {
        results[i].active = false;
    }

    /* Load background */
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/FINAL.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    /* Load font */
    res_font = LoadFON("assets/FONTTI.FON", true);

    /* Load portraits */
    for (int i = 0; i < num_results; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/%s%s.PPM",
                 COLOR_PREFIX[i], RESULT_SUFFIX[results[i].rank]);

        /* Try uppercase first (assets from original/) */
        for (int c = 7; path[c]; c++) {
            if (path[c] >= 'a' && path[c] <= 'z')
                path[c] -= 32;
        }

        portrait_imgs[i] = LoadPCX(path);
        if (portrait_imgs[i].width > 0) {
            portrait_texs[i] = LoadTextureFromImage(portrait_imgs[i]);
        } else {
            portrait_texs[i] = (Texture2D){0};
        }
    }
    for (int i = num_results; i < MAX_PLAYERS; i++) {
        portrait_imgs[i] = (Image){0};
        portrait_texs[i] = (Texture2D){0};
    }

    /* Start fade in */
    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);
    state = RES_FADE_IN;
    timer = 0;
}

ResultsResult results_update(void)
{
    switch (state) {
    case RES_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            /* Applause plays right after the fade-in, before the 3 s
             * key-flush delay (seg_1000:6394-6396:
             * trigger_sound_effect(7, 1, 11000, 0)). */
            sfx_play(SFX_APPLAUSE);
            state = RES_WAIT_TIMER;
            timer = 0;
        }
        break;

    case RES_WAIT_TIMER:
        timer++;
        if (timer >= WAIT_FRAMES) {
            state = RES_WAIT_KEY;
        }
        /* Allow ESC to skip the timer */
        if (input_pressed(INPUT_CANCEL) || input_pressed(INPUT_ANY_KEY)) {
            state = RES_WAIT_KEY;
        }
        break;

    case RES_WAIT_KEY:
        if (input_pressed(INPUT_CANCEL) || input_pressed(INPUT_ANY_KEY) ||
            input_pressed(INPUT_CONFIRM)) {
            palette_start_fade_out(FADE_STEPS);
            state = RES_FADE_OUT;
        }
        break;

    case RES_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = RES_FINISHED;
        }
        break;

    case RES_FINISHED:
        return RESULTS_DONE;
    }

    return RESULTS_ACTIVE;
}

void results_draw(void)
{
    /* Draw background */
    if (bg_tex.id) {
        DrawTexture(bg_tex, 0, 0, WHITE);
    }

    /* Only draw overlays after fade-in completes */
    if (state == RES_FADE_IN) return;

    Color text_col = palette_get_color(1);   /* white */

    for (int i = 0; i < num_results; i++) {
        if (!results[i].active) continue;

        /* Portrait (132x219 PCX) in the column's stone panel. */
        if (portrait_texs[i].id) {
            DrawTexture(portrait_texs[i], PORTRAIT_X[i], PORTRAIT_Y, WHITE);
        }

        /* Name: like the shop, the first character (the "N " prefix
         * digit) is forced to a space and the string is capped at 18
         * chars (seg_1000:6135-6139). */
        char buf[32];
        snprintf(buf, sizeof(buf), "%.18s", results[i].name);
        if (buf[0] != '\0') buf[0] = ' ';
        DrawTextFON(&res_font, buf, RES_NAME_X[i], RES_NAME_Y, text_col);

        /* The original prints exactly two numbers per player — wallet
         * (Y=346) and round wins (Y=362), next to the $ / trophy icons
         * baked into FINAL.SPY. It never shows `earned`. */
        snprintf(buf, sizeof(buf), "%d", (int)results[i].score);
        DrawTextFON(&res_font, buf, RES_VAL_X[i], RES_CASH_Y, text_col);

        snprintf(buf, sizeof(buf), "%d", (int)results[i].round_wins);
        DrawTextFON(&res_font, buf, RES_VAL_X[i], RES_WINS_Y, text_col);
    }
}

void results_cleanup(void)
{
    if (bg_tex.id) UnloadTexture(bg_tex);
    if (bg_img.data) UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;
    bg_tex = (Texture2D){0};
    bg_img = (Image){0};

    UnloadFON(&res_font);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (portrait_texs[i].id) UnloadTexture(portrait_texs[i]);
        if (portrait_imgs[i].data) UnloadImage(portrait_imgs[i]);
        portrait_texs[i] = (Texture2D){0};
        portrait_imgs[i] = (Image){0};
    }
}
