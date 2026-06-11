#include "game/player_select.h"
#include "game/config.h"
#include "loaders/spy_loader.h"
#include "loaders/sprite_sheet.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FADE_STEPS  7

/*
 * Player select screen ("new game setup"). Decompiled refs:
 *   Main loop:    FUN_1000_3276 (seg_1000:2096-2189)
 *   Full redraw:  player_select_screen (seg_1000:1594-1631)
 *   Slot names:   FUN_1000_17e7 (seg_1000:959-980)
 *   Record list:  FUN_1000_1892 (seg_1000:984-1017)
 *   Stats panel:  FUN_1000_19e8 (seg_1000:1053-1260)
 *   Slot keys:    FUN_1000_295e (seg_1000:1635-1784)
 *   Browse keys:  FUN_1000_2350 (seg_1000:1378-1509)
 *   Name entry:   FUN_1000_200e (seg_1000:1264-1344)
 *   Shovel blit:  FUN_1000_2841; arrow blit: FUN_1000_2303
 *
 * Layout (all verified against a DOSBox capture, 2026-06-11):
 *   - 4 player slot rows + a PLAY row, stacked vertically with a 53px
 *     (0x35) stride. The shovel cursor blits at X=0x2c=44,
 *     Y=0x23+slot*0x35; cursor position 4 is the PLAY row.
 *   - Selected record name per slot at (120, 41+53*slot), color 1.
 *   - Inactive slot rows erased to black: X 39..331, Y 18+53s..70+53s
 *     (fill_rect coords inclusive).
 *   - Record list at X=378, Y=23, 8px rows; existing records in color 1,
 *     empty slots print "-" in color 3.
 *   - Browse arrow sprite (27x11, SIKA.SPY 205,99) at X=339,
 *     Y=22+(rec-1)*8.
 *   - Stats grid: column A X=65, column B X=211, rows Y=330+24k.
 *     Column A: stats[0], stats[1], win% bar+text, stats[2], stats[3],
 *     win% bar+text. Column B: stats[4..9]. Win% rows draw a white bar
 *     (X 64..64+pct, 10px tall at Y 376/448) with red "NN%" on top.
 *   - Player's history: weapon-count polyline in the box X 367..564,
 *     Y bottom 457 (count 0 = flat red line at the bottom).
 */

/* Shovel cursor sprite from SIKA.SPY (same as main menu cursor).
 * capture_screen_region(DAT_1038_0678, 0xa0, 0xd7, 0x8c, 0x96)
 * -> source X=150, Y=140, W=65, H=20 (port keeps the menu's 65x20). */
#define CURSOR_W        65
#define CURSOR_H        20
#define CURSOR_X        44   /* 0x2c */
#define CURSOR_Y_BASE   35   /* 0x23 */
#define CURSOR_Y_STRIDE 53   /* 0x35 */
#define CURSOR_PLAY      4   /* 5th cursor position = PLAY row */

/* Browse arrow sprite from SIKA.SPY.
 * capture_screen_region(DAT_1038_067c, 0x6d, 0xe7, 0x63, 0xcd)
 * -> source X=205, Y=99, W=27, H=11 (coords inclusive). */
#define ARROW_W   27
#define ARROW_H   11
#define ARROW_X  339   /* list X base 0x178 - 0x25 */

/* Slot name rows (FUN_1000_17e7): print_string_at(name, seg, Y, X). */
#define NAME_X   120   /* 0x78 */
#define NAME_Y_BASE 41 /* 0x29; stride 53 */

/* Inactive slot erase (player_select_screen):
 * fill_rect(s*0x35+0x46, 0x14b, s*0x35+0x12, 0x27) — Y/X inclusive. */
#define INACTIVE_X      39   /* 0x27 */
#define INACTIVE_W     293   /* 0x14b - 0x27 + 1 */
#define INACTIVE_Y_BASE 18   /* 0x12 */
#define INACTIVE_H      53   /* 0x46 - 0x12 + 1 */

/* Record list (FUN_1000_1892): text cursor starts at (Y=0x16+1, X=0x178+2). */
#define RECLIST_X      378
#define RECLIST_Y       23
#define RECLIST_ROW_H    8

/* Stats grid (FUN_1000_19e8). */
#define STATS_COL_A_X   65   /* 0x41 */
#define STATS_COL_B_X  211   /* 0xd3 */
#define STATS_Y_BASE   330   /* 0x14a */
#define STATS_Y_STRIDE  24   /* 0x18 */
#define RATIO_BAR_X     64   /* 0x40 */
#define RATIO1_BAR_Y   376   /* 0x178 (text at 0x17a) */
#define RATIO2_BAR_Y   448   /* 0x1c0 (text at 0x1c2) */
#define RATIO_BAR_H     10   /* fill_rect Y inclusive: 0x178..0x181 */

/* Player's history graph (FUN_1000_19e8 tail). */
#define GRAPH_X_START  367   /* 0x16f */
#define GRAPH_Y_BASE   457   /* 0x1c9 */
#define GRAPH_X_STEP     6

#define NAME_MAX_LEN    24   /* FUN_1000_200e stops at len > 0x17 */

typedef enum {
    PSEL_FADE_IN,
    PSEL_SLOTS,        /* shovel cursor on a slot row / PLAY row */
    PSEL_BROWSE,       /* arrow cursor in the record list */
    PSEL_NAME_ENTRY,   /* typing a new record name in the list */
    PSEL_FADE_OUT
} PSelState;

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static BitmapFont psel_font;

static Image cursor_img, arrow_img;
static Texture2D cursor_tex, arrow_tex;
static bool sprites_loaded;

static PlayerDatabase player_db;
static int record_sel[MAX_PLAYERS];  /* 1-based chosen record, 0 = none
                                        (original g_playerN_ready) */
static char slot_name[MAX_PLAYERS][26];
static int cursor;                   /* 0..3 = slots, 4 = PLAY */

static int browse_rec;               /* 1-based record under the arrow */
static int entry_len;                /* name-entry character count */

static PSelState state;
static bool f10_abort;

/* A record EXISTS when its first byte is 0 (verified against the shipped
 * PLAYERS.DAT: "Plr 1"/"Plr 2" have byte0=0, the 30 empty slots 0x01;
 * FUN_1000_1892 prints the name when byte0=='\0'). The port previously
 * had this inverted. */
static bool record_exists(const PlayerRecord *rec)
{
    return rec->exists == 0;
}

int player_select_slot_count(void)
{
    return g_config.num_players;
}

bool player_select_all_ready(void)
{
    for (int i = 0; i < g_config.num_players && i < MAX_PLAYERS; i++) {
        if (record_sel[i] == 0) return false;
    }
    return true;
}

int player_select_wrap_record(int index)
{
    index %= PLAYER_DB_SLOTS;
    if (index < 0) index += PLAYER_DB_SLOTS;
    return index;
}

/* Last-used selections file: 4 bytes, one 1-based record index per player
 * (0 = none). Original reads it in FUN_1000_2e4e and writes it back in
 * FUN_1000_2f9f; it ships as IDENTIFY.DAT = 01 02 00 00, which is why the
 * original boots the screen with both players pre-selected. */
static void load_identify(const char *path)
{
    for (int i = 0; i < MAX_PLAYERS; i++) record_sel[i] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint8_t b[MAX_PLAYERS] = {0};
    fread(b, 1, MAX_PLAYERS, f);
    fclose(f);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (b[i] >= 1 && b[i] <= PLAYER_DB_SLOTS) record_sel[i] = b[i];
    }
}

static void save_identify(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint8_t b[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) b[i] = (uint8_t)record_sel[i];
    fwrite(b, 1, MAX_PLAYERS, f);
    fclose(f);
}

static void copy_slot_name(int slot)
{
    if (record_sel[slot] >= 1) {
        player_record_name(slot_name[slot], sizeof(slot_name[slot]),
                           &player_db.records[record_sel[slot] - 1]);
    } else {
        slot_name[slot][0] = '\0';
    }
}

/* First empty record, 1-based (FUN_1000_2710; 0x20 if none). */
static int first_empty_record(void)
{
    for (int i = 0; i < PLAYER_DB_SLOTS; i++) {
        if (!record_exists(&player_db.records[i])) return i + 1;
    }
    return PLAYER_DB_SLOTS;
}

void player_select_init(void)
{
    /* Background is IDENTIFW.SPY — the Pascal string at seg_1000:0x2878
     * (MB.EXE file offset 14456) referenced by load_and_display_image in
     * player_select_screen is 'identifw.spy'. PLAYERS.SPY (which the port
     * loaded here before) is the in-game HUD art sheet, not this screen. */
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/IDENTIFW.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    psel_font = LoadFON("assets/FONTTI.FON", true);

    /* Shovel cursor + browse arrow from SIKA.SPY. */
    {
        uint8_t sheet_palette[768];
        uint8_t *sheet_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
        Image sheet_img = LoadSPY("assets/SIKA.SPY", sheet_palette, sheet_indexed);
        cursor_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                                   150, 140, CURSOR_W, CURSOR_H, sheet_palette);
        cursor_tex = LoadTextureFromImage(cursor_img);
        arrow_img = ExtractSprite(sheet_indexed, SPY_WIDTH, SPY_HEIGHT,
                                  205, 99, ARROW_W, ARROW_H, sheet_palette);
        arrow_tex = LoadTextureFromImage(arrow_img);
        sprites_loaded = true;
        free(sheet_indexed);
        UnloadImage(sheet_img);
    }

    if (!player_db_load(&player_db, "assets/players.dat")) {
        player_db_init_defaults(&player_db);
    }

    load_identify("assets/IDENTIFY.DAT");
    for (int i = 0; i < MAX_PLAYERS; i++) {
        copy_slot_name(i);
    }

    cursor = CURSOR_PLAY;  /* original starts on the PLAY row (local_e = 4) */
    browse_rec = 1;
    entry_len = 0;
    f10_abort = false;

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    state = PSEL_FADE_IN;
}

/* Finalize selections: init g_players, persist PLAYERS.DAT + IDENTIFY.DAT.
 * The original writes both files on every exit path (FUN_1000_3276 runs to
 * the end even on F10), so call this for abort too. */
static void persist_and_finalize(bool start_match)
{
    player_db_save(&player_db, "assets/players.dat");
    save_identify("assets/IDENTIFY.DAT");

    if (!start_match) return;

    for (int i = 0; i < g_config.num_players && i < MAX_PLAYERS; i++) {
        int rec = record_sel[i] - 1;
        player_init_from_record_slot(&g_players[i], &player_db.records[rec],
                                     i, rec);
        /* The original appends a "N " player-number prefix to the runtime
         * name on select exit (FUN_1000_3276 tail; the four constants at
         * seg_1000:0x326a are "1 ".."4 "). The shop masks the digit cell
         * (FUN_1010_a933); other screens print the prefixed name as-is. */
        char base[25];
        snprintf(base, sizeof(base), "%s", g_players[i].name);
        snprintf(g_players[i].name, sizeof(g_players[i].name), "%d %s",
                 i + 1, base);
    }
    g_num_active_players = g_config.num_players;
}

/* Slot-cursor movement (FUN_1000_295e '2'/'8' handlers): skip inactive
 * player rows, wrap through the PLAY row (position 4). */
static void cursor_down(void)
{
    cursor++;
    if (cursor == CURSOR_PLAY + 1) cursor = 0;
    while (cursor > g_config.num_players - 1 && cursor < CURSOR_PLAY) {
        cursor++;
    }
}

static void cursor_up(void)
{
    cursor--;
    if (cursor == -1) {
        cursor = CURSOR_PLAY;
        return;
    }
    while (cursor > g_config.num_players - 1 && cursor != 0) {
        cursor--;
    }
}

static void begin_fade_out(void)
{
    palette_start_fade_out(FADE_STEPS);
    state = PSEL_FADE_OUT;
}

/* Enter name entry on an empty record: zero it and mark it existing
 * (FUN_1000_2350 else-branch), then type into its Pascal name. */
static void begin_name_entry(int first_char)
{
    PlayerRecord *rec = &player_db.records[browse_rec - 1];
    memset(rec, 0, sizeof(*rec));   /* byte0 = 0 -> exists */
    entry_len = 0;
    if (first_char >= 33 && first_char <= 126 && entry_len < NAME_MAX_LEN) {
        rec->name[++entry_len] = (char)first_char;
        rec->name[0] = (char)entry_len;
    }
    state = PSEL_NAME_ENTRY;
}

/* Choose the browsed record for the browsing slot, or start creating a
 * player if the record is empty (FUN_1000_2350 default branch). */
static void browse_choose(int typed_char)
{
    PlayerRecord *rec = &player_db.records[browse_rec - 1];
    if (record_exists(rec)) {
        record_sel[cursor] = browse_rec;
        copy_slot_name(cursor);
        state = PSEL_SLOTS;
    } else {
        begin_name_entry(typed_char);
    }
}

/* Delete the browsed record (FUN_1000_2350, key 0xB7): mark it empty and
 * clear any slot that had it selected. */
static void browse_delete(void)
{
    PlayerRecord *rec = &player_db.records[browse_rec - 1];
    if (!record_exists(rec)) return;
    rec->exists = 1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (record_sel[i] == browse_rec) {
            record_sel[i] = 0;
            slot_name[i][0] = '\0';
        }
    }
}

PlayerSelectResult player_select_update(void)
{
    switch (state) {
    case PSEL_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = PSEL_SLOTS;
        }
        break;

    case PSEL_SLOTS: {
        /* F10: abort the match (g_mode_flag path, seg_1000:1712-1716). */
        if (input_pressed(INPUT_QUIT)) {
            f10_abort = true;
            begin_fade_out();
            break;
        }
        if (input_pressed(INPUT_DOWN)) cursor_down();
        if (input_pressed(INPUT_UP))   cursor_up();

        /* ESC: start the match if every active player has a record
         * (seg_1000:1705-1710). With players missing it does nothing —
         * the original has no ESC-to-menu here, only F10. */
        if (input_pressed(INPUT_CANCEL)) {
            if (player_select_all_ready()) {
                persist_and_finalize(true);
                begin_fade_out();
            }
            break;
        }

        int ch = GetCharPressed();
        if (input_pressed(INPUT_CONFIRM) || ch >= 33) {
            if (cursor == CURSOR_PLAY) {
                /* Any key on the PLAY row starts the match when everyone
                 * is ready (seg_1000:1718-1725). */
                if (player_select_all_ready()) {
                    persist_and_finalize(true);
                    begin_fade_out();
                }
            } else if (ch >= 33) {
                /* Typing a letter on a slot jumps to the first empty
                 * record and starts creating a player with that letter
                 * (FUN_1000_2772 + FUN_1000_2710). */
                browse_rec = first_empty_record();
                browse_choose(ch);
            } else {
                /* Enter opens the record list on the slot's record. */
                browse_rec = record_sel[cursor] >= 1 ? record_sel[cursor] : 1;
                state = PSEL_BROWSE;
            }
        }
        break;
    }

    case PSEL_BROWSE: {
        if (input_pressed(INPUT_QUIT)) {
            f10_abort = true;
            begin_fade_out();
            break;
        }
        /* ESC leaves the list without changing the slot (FUN_1000_2350). */
        if (input_pressed(INPUT_CANCEL)) {
            state = PSEL_SLOTS;
            break;
        }
        if (input_pressed(INPUT_DOWN)) {
            browse_rec++;
            if (browse_rec > PLAYER_DB_SLOTS) browse_rec = 1;
        }
        if (input_pressed(INPUT_UP)) {
            browse_rec--;
            if (browse_rec < 1) browse_rec = PLAYER_DB_SLOTS;
        }
        if (IsKeyPressed(KEY_DELETE)) {
            browse_delete();
            break;
        }
        int ch = GetCharPressed();
        if (input_pressed(INPUT_CONFIRM) || ch >= 33) {
            browse_choose(ch);
        }
        break;
    }

    case PSEL_NAME_ENTRY: {
        PlayerRecord *rec = &player_db.records[browse_rec - 1];
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (ch >= 32 && ch <= 126 && entry_len < NAME_MAX_LEN) {
                rec->name[++entry_len] = (char)ch;
                rec->name[0] = (char)entry_len;
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && entry_len > 0) {
            rec->name[0] = (char)--entry_len;
        }
        /* Enter or ESC commits the name; the new record is NOT selected
         * for the slot — the user chooses it with another Enter
         * (FUN_1000_2350: entry returns to the browse loop). */
        if (input_pressed(INPUT_CONFIRM) || input_pressed(INPUT_CANCEL)) {
            state = PSEL_BROWSE;
        }
        break;
    }

    case PSEL_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            if (f10_abort) {
                persist_and_finalize(false);
                return PSELECT_CANCEL;
            }
            return PSELECT_DONE;
        }
        break;
    }

    return PSELECT_NONE;
}

/* Win% row: white bar of pct pixels with red "NN%" on top
 * (FUN_1000_19e8: fill_rect(bar_y_end, pct+0x40, bar_y, 0x40) in color 1,
 * then the text at (0x41, bar_y+2) in color 3). */
static void draw_ratio_row(uint32_t numer, uint32_t denom, int bar_y)
{
    if (denom == 0) return;
    uint32_t pct = numer * 100u / denom;
    if (pct > 100) pct = 100;
    DrawRectangle(RATIO_BAR_X, bar_y, (int)pct + 1, RATIO_BAR_H,
                  palette_get_color(1));
    char buf[16];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
    DrawTextFON(&psel_font, buf, STATS_COL_A_X, bar_y + 2,
                palette_get_color(3));
}

/* Weapon-count polyline color class (FUN_1000_196a): the class thresholds
 * are Borland Real comparisons we have not decoded exactly; 25/50/75/100
 * matches the legend gradient bands (red/orange/gold/yellow/green) baked
 * into PLAYERS.SPY. Zero counts draw red at the box bottom — verified in
 * the DOSBox capture. */
static Color graph_color(uint8_t count)
{
    int ci;
    if (count < 25)       ci = 3;   /* red */
    else if (count < 50)  ci = 7;   /* orange */
    else if (count < 75)  ci = 6;   /* gold */
    else if (count < 100) ci = 5;   /* yellow */
    else                  ci = 4;   /* green */
    return palette_get_color(ci);
}

/* FUN_1000_19e8 head: black-fill the twelve stat value boxes
 * (Y 328..337 + 24k, X 64..158 / 210..304 inclusive) and the history
 * graph box (X 367..564, Y 328..457) BEFORE the record-exists check —
 * the fills run whenever the cursor is on a player slot, erasing the
 * boxes' baked-in inner shadow row (verified: DOSBox f_060 y=328). */
static void draw_stat_clears(void)
{
    for (int j = 0; j < 6; j++) {
        int y = 328 + j * STATS_Y_STRIDE;
        DrawRectangle(64, y, 95, 10, BLACK);
        DrawRectangle(210, y, 95, 10, BLACK);
    }
    DrawRectangle(GRAPH_X_START, 328,
                  564 - GRAPH_X_START + 1, GRAPH_Y_BASE - 328 + 1, BLACK);
}

/* Stats panel + history graph for one record (FUN_1000_19e8). */
static void draw_stats(const PlayerRecord *rec)
{
    if (!record_exists(rec)) return;

    Color white = palette_get_color(1);
    char buf[16];

    /* Column A: matches, match wins, win%, rounds, round wins, win%. */
    static const int col_a_stat[4] = { 0, 1, 2, 3 };
    static const int col_a_row[4]  = { 0, 1, 3, 4 };
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)rec->stats[col_a_stat[i]]);
        DrawTextFON(&psel_font, buf, STATS_COL_A_X,
                    STATS_Y_BASE + col_a_row[i] * STATS_Y_STRIDE, white);
    }
    draw_ratio_row(rec->stats[1], rec->stats[0], RATIO1_BAR_Y);
    draw_ratio_row(rec->stats[3], rec->stats[2], RATIO2_BAR_Y);

    /* Column B: stats[4..9], six rows. */
    for (int i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)rec->stats[4 + i]);
        DrawTextFON(&psel_font, buf, STATS_COL_B_X,
                    STATS_Y_BASE + i * STATS_Y_STRIDE, white);
    }

    /* Player's history polyline. The original iterates
     * (local_7-1) mod 0x22 for local_7 = 0x25..0x45: segments run
     * weapons[2], [3], ..., [33], then [0], starting from a previous
     * point at weapons[33] — replicated verbatim, quirk included. */
    int x = GRAPH_X_START;
    int prev_y = GRAPH_Y_BASE - rec->weapons[33];
    for (int k = 0; k < 33; k++) {
        int idx = (k + 36) % 34;   /* 2,3,...,33,0 */
        uint8_t count = rec->weapons[idx];
        int y = GRAPH_Y_BASE - count;
        DrawLine(x, prev_y, x + GRAPH_X_STEP - 1, y, graph_color(count));
        prev_y = y;
        x += GRAPH_X_STEP;
    }
}

void player_select_draw(void)
{
    DrawTexture(bg_tex, 0, 0, WHITE);

    if ((state == PSEL_FADE_IN || state == PSEL_FADE_OUT) &&
        palette_is_fading()) {
        return;
    }

    Color white = palette_get_color(1);
    Color red   = palette_get_color(3);

    /* Black out inactive slot rows (banners are baked into PLAYERS.SPY). */
    for (int s = g_config.num_players; s < MAX_PLAYERS; s++) {
        DrawRectangle(INACTIVE_X, INACTIVE_Y_BASE + s * CURSOR_Y_STRIDE,
                      INACTIVE_W, INACTIVE_H, BLACK);
    }

    /* Selected record name under each active slot's banner. */
    for (int i = 0; i < g_config.num_players && i < MAX_PLAYERS; i++) {
        if (slot_name[i][0] != '\0') {
            DrawTextFON(&psel_font, slot_name[i], NAME_X,
                        NAME_Y_BASE + i * CURSOR_Y_STRIDE, white);
        }
    }

    /* Record list: names in white, "-" markers on empty slots. */
    for (int i = 0; i < PLAYER_DB_SLOTS; i++) {
        const PlayerRecord *rec = &player_db.records[i];
        int list_y = RECLIST_Y + i * RECLIST_ROW_H;
        if (record_exists(rec)) {
            char pname[26];
            player_record_name(pname, sizeof(pname), rec);
            DrawTextFON(&psel_font, pname, RECLIST_X, list_y, white);
        } else {
            DrawTextFON(&psel_font, "-", RECLIST_X, list_y, red);
        }
    }

    /* Shovel cursor on the current row (PLAY row included). */
    if (sprites_loaded) {
        DrawTexture(cursor_tex, CURSOR_X,
                    CURSOR_Y_BASE + cursor * CURSOR_Y_STRIDE, WHITE);
    }

    /* Browse arrow + name-entry underline. */
    if (state == PSEL_BROWSE || state == PSEL_NAME_ENTRY) {
        if (sprites_loaded) {
            DrawTexture(arrow_tex, ARROW_X,
                        RECLIST_Y - 1 + (browse_rec - 1) * RECLIST_ROW_H,
                        WHITE);
        }
        if (state == PSEL_NAME_ENTRY) {
            /* FUN_1000_200e: 9x2 underline after the typed text. */
            int row_y = RECLIST_Y + (browse_rec - 1) * RECLIST_ROW_H;
            DrawRectangle(RECLIST_X + 1 + entry_len * 8, row_y + 6, 9, 2,
                          white);
        }
    }

    /* Stats panel: browsed record while in the list, otherwise the
     * record selected for the slot under the cursor. Nothing on PLAY —
     * the original never calls FUN_1000_19e8 from cursor position 4, so
     * the boxes keep their background pixels there; on a slot the boxes
     * are black-filled even when no record is selected (19e8(0)). */
    bool on_slot = (state == PSEL_BROWSE || state == PSEL_NAME_ENTRY ||
                    cursor < g_config.num_players);
    if (on_slot) {
        draw_stat_clears();
        int stats_rec;
        if (state == PSEL_BROWSE || state == PSEL_NAME_ENTRY) {
            stats_rec = browse_rec;
        } else {
            stats_rec = record_sel[cursor];
        }
        if (stats_rec >= 1) {
            draw_stats(&player_db.records[stats_rec - 1]);
        }
    }
}

void player_select_cleanup(void)
{
    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;

    UnloadFON(&psel_font);

    if (sprites_loaded) {
        UnloadTexture(cursor_tex);
        UnloadImage(cursor_img);
        UnloadTexture(arrow_tex);
        UnloadImage(arrow_img);
        sprites_loaded = false;
    }
    /* Note: player_db is kept alive (static) for use by main.c
     * stat persistence after multiplayer rounds. */
}

PlayerDatabase *player_select_get_db(void)
{
    return &player_db;
}
