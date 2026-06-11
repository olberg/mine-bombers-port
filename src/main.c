#include "raylib.h"
#include "audio/music.h"
#include "audio/sfx.h"
#include "input/input.h"
#include "game/title.h"
#include "game/menu.h"
#include "game/config.h"
#include "game/options.h"
#include "game/info.h"
#include "game/player_select.h"
#include "game/shop.h"
#include "game/round.h"
#include "game/hud.h"
#include "game/sprites.h"
#include "gfx/palette.h"
#include "game/map_renderer.h"
#include "game/hall_of_fame.h"
#include "game/sp_complete.h"
#include "game/sp_level_complete.h"
#include "game/key_config.h"
#include "game/sound_config.h"
#include "game/map_list.h"
#include "game/map_picker.h"
#include "game/music_schedule.h"
#include "game/results.h"
#include "debug_overlay.h"
#include "autoplay.h"
#include "util/prng.h"
#include <stdio.h>
#include <stdarg.h>

#define RENDER_WIDTH  640
#define RENDER_HEIGHT 480
#define SCALE         2
#define WINDOW_WIDTH  (RENDER_WIDTH * SCALE)
#define WINDOW_HEIGHT (RENDER_HEIGHT * SCALE)

static FILE *log_file;

static void trace_log(int logLevel, const char *text, va_list args)
{
    static const char *level_str[] = {
        "ALL", "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "NONE"
    };
    const char *lvl = (logLevel >= 0 && logLevel <= 7) ? level_str[logLevel] : "???";

    /* A va_list is single-use: vprintf consumes it, so each output needs its
     * own va_copy (reusing the original segfaults on x86_64 Linux). */
    va_list console_args;
    va_copy(console_args, args);

    /* Console */
    printf("[%s] ", lvl);
    vprintf(text, console_args);
    printf("\n");
    va_end(console_args);

    /* File */
    if (log_file) {
        va_list file_args;
        va_copy(file_args, args);
        fprintf(log_file, "[%s] ", lvl);
        vfprintf(log_file, text, file_args);
        fprintf(log_file, "\n");
        fflush(log_file);
        va_end(file_args);
    }
}

typedef enum {
    STATE_INIT,
    STATE_TITLE,
    STATE_MENU,
    STATE_OPTIONS,
    STATE_INFO,
    STATE_PLAYER_SELECT,
    STATE_SHOP,
    STATE_GAMEPLAY,
    STATE_RESULTS,
    STATE_SP_LEVEL_COMPLETE, /* GAMEOVER.SPY — SP match end (game over) */
    STATE_SP_CONGRATS,       /* CONGRATU.SPY — SP campaign completion */
    STATE_HALL_OF_FAME,
    STATE_KEY_CONFIG,
    STATE_SOUND_CONFIG,
    STATE_MAP_PICKER,
    STATE_QUIT
} GameState;

static const char *state_name(GameState s) {
    static const char *names[] = {
        "INIT", "TITLE", "MENU", "OPTIONS", "INFO",
        "PLAYER_SELECT", "SHOP", "GAMEPLAY", "RESULTS",
        "SP_LEVEL_COMPLETE", "SP_CONGRATS",
        "HALL_OF_FAME", "KEY_CONFIG", "SOUND_CONFIG", "MAP_PICKER", "QUIT"
    };
    if (s >= 0 && s < (int)(sizeof(names)/sizeof(names[0]))) return names[s];
    return "???";
}

static void change_state(GameState *st, GameState new_st) {
    TraceLog(LOG_INFO, "STATE: %s -> %s", state_name(*st), state_name(new_st));
    if (autoplay_active()) {
        autoplay_notify_state(state_name(*st), state_name(new_st));
    }
    *st = new_st;
}

/* Gameplay state */
static Round current_round;
static int rounds_remaining;
static int current_map_index;
static bool single_player_mode;

/* Abort semantics: the original's g_mode_flag is set ONLY
 * by F10 at player select (seg_1000:1712-1716) and skips the shop/round
 * loop and the whole post-match block — the port equivalent is the
 * PSELECT_CANCEL path straight back to the menu, so no flag is needed.
 * F10 at the shop or in-round merely zeroes rounds_remaining; results /
 * stats / HoF still run (see the SHOP_ABORTED and `escaped` handlers).
 * The old "all_random_maps / F12 random-map mode" reading of g_mode_flag
 * was wrong; the autoplay harness gets all-random rounds via
 * map_picker_reset() instead. */

/* Map file path buffer */
static char map_path[64];

/* Last 1-based song order scheduled (shop or gameplay) — for trace/debug. */
static int music_jump;

static void build_map_path(int index, bool single_player)
{
    if (single_player) {
        /* Original (seg_1000:7073-7079) computes level index as:
         * g_total_rounds - g_rounds_remaining (starting from 0).
         * Files are LEVEL0.MNL through LEVEL14.MNL. */
        snprintf(map_path, sizeof(map_path), "assets/LEVEL%d.MNL", index);
    } else {
        /* Multiplayer: use per-round map selection from map picker.
         * Original (seg_1000:7082-7089): slot < 30000 → load the name at
         * table[slot * 9 + 0x9F8] + ".mne" (1-based; entry 0 = "Random").
         * map_picker_get_map_index converts to the 0-based map_list index. */
        int round_idx = g_config.total_rounds - rounds_remaining;
        int map_idx = map_picker_get_map_index(round_idx);
        if (map_idx >= 0) {
            map_list_build_path(map_path, sizeof(map_path), "assets", map_idx);
        } else {
            /* Fallback: shouldn't reach here if should_use_random_map()
             * is checked first, but cycle through map list as safety. */
            int count = map_list_count();
            if (count > 0) {
                map_list_build_path(map_path, sizeof(map_path), "assets",
                                    index % count);
            } else {
                snprintf(map_path, sizeof(map_path), "assets/KOMPLEX.MNE");
            }
        }
    }
}

/* Determine whether this round should use a random map.
 * Original gate (seg_1000:7082-7083):
 *   table[round] < 30000 && g_mode_flag == 0  → picked map, else random.
 *   - Single-player: always load .MNL (never reaches this gate)
 *   - per-round: slot < 30000 → picked (1-based map index);
 *     32000 = untouched/Random-cell slots → random.
 * (g_mode_flag in the gate is the abort marker — when set, rounds_remaining
 * is already 0 and no round runs, so it needs no port equivalent here.) */
static bool should_use_random_map(void)
{
    if (single_player_mode) return false;
    int round_idx = g_config.total_rounds - rounds_remaining;
    return !map_picker_has_selection(round_idx);
}

static void play_menu_music(void)
{
    music_stop();
    if (music_load("assets/HUIPPE.S3M")) {
        music_play();
    }
}

/* PLAY selected: the original mutes (seg_1000:7054) and loads OEKU.S3M plus
 * the game SFX (FUN_1010_5c83). Music stays MUTED through player select —
 * the first audible position is the pre-shop order jump. */
static void load_game_music(void)
{
    music_stop();
    music_load("assets/OEKU.S3M");
}

/* Pre-shop: jump to order 0x54 and unmute (seg_1000:7098-7102). OEKU's
 * orders 84-90 are a distinct tail section — the designated shop music. */
static void play_shop_music(void)
{
    music_jump = music_schedule_shop_order();
    music_play_from_order(music_jump);
}

/* Post-shop: gameplay music starts at a random entry of the 14-position
 * order table at 0x1038:0x0010 (seg_1000:7122-7126). The PRNG draw happens
 * unconditionally so a disabled music option cannot shift the seed
 * sequence. */
static void play_gameplay_music(void)
{
    music_jump = music_schedule_gameplay_order();
    music_jump_to_order(music_jump);
}

/* Match-level reset pass. Mirrors process_menu_selection (seg_1010:7158-7228),
 * which the original runs once between menu exit and the round loop to clean
 * per-player state for a new match. See structural audit finding A2.
 *
 * Step 7: wallets = Starting Cash option (MP, seg_1010:7174-7183) or 250
 *   (SP, seg_1010:7170) — handled by player_init_from_record, which the
 *   player select screen calls at confirm. If we reset here, we'd clobber
 *   the record-name and cheat steps later, so it lives there.
 * Step 11: SP lives = 3 — handled by player_init_defaults.
 * Step 14: DAT_1038_2556 = 0 — vestigial: nothing in v3.11 ever sets it
 *   nonzero, so the SP HoF gate (seg_1000:7326, `2556 == 0`) always
 *   passes. No port equivalent needed. */
static void game_reset_for_new_match(void)
{
    /* All steps are currently covered elsewhere (see above); the function
     * remains as the anchor for the original's match-reset point. */
}

static bool round_loaded;

/* Load the next round BEFORE the shop. Original per-round order
 * (seg_1000:7065-7102): pick/load or generate the map, spawn monsters
 * (FUN_1000_722b), init overlays/collision + per-round player resets +
 * placement (FUN_1010_c5f4), reset dig power (game_state_update) — all
 * before the shop call at 7103, whose screen shows a thumbnail of this
 * map. */
static void load_round(void)
{
    /* Time limit from config — the value is PIT TICKS (18.2065 Hz, one
     * INT8 tick = 54.9 ms), NOT seconds: the original initializes
     * g_time_remaining = g_time_limit_lo verbatim (seg_1010:7447) and
     * the ISR decrements once per tick (DOSBox measurement:
     * config 45 -> 2.47 s round, config 16 -> 0.88 s, config 120 ->
     * 6.6 s; factory 7662 = 7 min 1 s). round.c converts frames to
     * ticks internally. */
    int time_limit = -1;
    int32_t t = (int32_t)g_config.time_limit_hi << 16 | (uint16_t)g_config.time_limit_lo;
    if (t > 0) {
        time_limit = t;
    }

    bool ok;
    if (should_use_random_map()) {
        TraceLog(LOG_INFO, "ROUND: Loading round %d, RANDOM MAP, players=%d, treasures=%d",
                 g_config.total_rounds - rounds_remaining + 1,
                 g_num_active_players, g_config.config_byte);
        ok = round_init_random(&current_round,
                               g_config.total_rounds - rounds_remaining + 1,
                               time_limit, g_config.config_byte);
    } else {
        build_map_path(current_map_index, single_player_mode);
        TraceLog(LOG_INFO, "ROUND: Loading round %d, map=%s, single=%d, players=%d",
                 g_config.total_rounds - rounds_remaining + 1, map_path,
                 single_player_mode, g_num_active_players);
        ok = round_init(&current_round, map_path,
                        g_config.total_rounds - rounds_remaining + 1,
                        single_player_mode, time_limit);
    }

    if (ok) {
        /* Place players at starting positions (corners of the map) and
         * run the per-round resets — pre-shop, as in the original
         * (FUN_1010_c4f2 inside c5f4). The shop panels
         * therefore show post-reset values (dig power 1 + tool bonus). */
        round_place_players(&current_round, g_players, g_num_active_players);
        shop_set_next_round(&current_round.map, rounds_remaining);
    } else {
        shop_set_next_round(NULL, rounds_remaining);
    }
    round_loaded = ok;
}

/* Post-shop round start (seg_1000:7121-7129): re-apply shop purchases
 * (the original's second game_state_update + FUN_1010_c15c) and bring up
 * the renderer/HUD. Positions and the other per-round resets already ran
 * in load_round(). */
static void begin_round(void)
{
    if (!round_loaded) return;

    for (int i = 0; i < g_num_active_players && i < MAX_PLAYERS; i++) {
        player_apply_shop_purchases(&g_players[i]);
    }

    /* Per-round map renderer + HUD. The sprite atlas (SIKA.SPY) is a
     * process-level resource loaded in main() — it is also used by the
     * shop screen, which runs before any round starts. */
    map_renderer_init();
    map_renderer_set_map(&current_round.map);
    hud_init(g_num_active_players);

    /* Re-install the in-game palette: the round was loaded BEFORE the
     * shop, whose screen installed its own palette and faded to
     * black on exit. The original's pre-gameplay fade-in to the palette
     * at DS:0x688 (seg_1000:7135) happens at this point too. */
    palette_init(sprites_get_palette());
}

static void end_round(void)
{
    hud_cleanup();
    map_renderer_cleanup();
    round_cleanup(&current_round);
    round_loaded = false;
}

int main(int argc, char *argv[])
{
    debug_parse_args(argc, argv);
    mb_prng_init();
    autoplay_init();
    log_file = fopen("minebombers.log", "w");
    SetTraceLogCallback(trace_log);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Mine Bombers");
    SetTargetFPS(60);
    SetExitKey(0);  /* Disable ESC-to-close; game uses ESC for navigation */

    RenderTexture2D target = LoadRenderTexture(RENDER_WIDTH, RENDER_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    /* SIKA.SPY is an immutable atlas used by both the shop screen and the
     * in-round map renderer. Load once at startup so the shop (which runs
     * before the first round) has access to the cell-border and item icons. */
    sprites_init();

    GameState state = STATE_INIT;

    /* Load config */
    /* Original config filename verified against the original game: the game writes OPTIONS.CFG
     * (17 bytes) on options exit; no config file ships in the distribution.
     * "ASETUK.DAT" (an earlier wrong guess) is ignored by the game. */
    config_load("assets/OPTIONS.CFG");
    if (autoplay_active()) {
        autoplay_configure_match();
    }

    /* Load multiplayer map list (scan for .MNE files, sort alphabetically) */
    map_list_load("assets");

    /* Load input bindings (defaults first, then override from file if present) */
    player_input_init_defaults();
    player_input_load("assets/keybinds.dat");

    /* Init audio — menu music is HUIPPE.S3M
     * Original: init_music_playback() loads huippe.s3m at seg_1010:3255 */
    music_init();
    sfx_init();
    sound_config_load("assets/SOUNDCFG.DAT");
    if (music_load("assets/HUIPPE.S3M")) {
        music_play();
    }

    change_state(&state, STATE_TITLE);
    title_init();

    while (state != STATE_QUIT && !WindowShouldClose()) {
        if (state != STATE_KEY_CONFIG) input_update();
        music_update();

        /* Update */
        switch (state) {
        case STATE_TITLE:
            if (autoplay_active() || title_update() == TITLE_DONE) {
                title_cleanup();
                menu_init();
                change_state(&state, STATE_MENU);
            }
            break;

        case STATE_MENU: {
            MenuSelection sel;
            if (autoplay_active()) {
                sel = autoplay_menu_wants_start() ? MENU_START : MENU_QUIT;
            } else {
                sel = menu_update();
            }
            if (sel != MENU_NONE) {
                menu_cleanup();
                switch (sel) {
                case MENU_OPTIONS:
                    options_init();
                    change_state(&state, STATE_OPTIONS);
                    break;
                case MENU_INFO:
                    info_init();
                    change_state(&state, STATE_INFO);
                    break;
                case MENU_START:
                    /* Original (seg_1000:7052-7061) resets per-match state,
                     * mutes the menu music and loads OEKU.S3M before the
                     * player-select screen. Player select runs in silence;
                     * music resumes at the shop's order jump. */
                    game_reset_for_new_match();
                    load_game_music();
                    player_select_init();
                    change_state(&state, STATE_PLAYER_SELECT);
                    break;
                case MENU_QUIT:
                    change_state(&state, STATE_QUIT);
                    break;
                default:
                    menu_init();
                    break;
                }
            }
            break;
        }

        case STATE_OPTIONS: {
            OptionsResult opt_res = options_update();
            if (opt_res == OPTIONS_DONE) {
                options_cleanup();
                menu_init();
                change_state(&state, STATE_MENU);
            } else if (opt_res == OPTIONS_KEY_CONFIG) {
                options_cleanup();
                key_config_init();
                change_state(&state, STATE_KEY_CONFIG);
            } else if (opt_res == OPTIONS_SOUND_CONFIG) {
                options_cleanup();
                sound_config_init();
                change_state(&state, STATE_SOUND_CONFIG);
            } else if (opt_res == OPTIONS_MAP_SELECT) {
                options_cleanup();
                map_picker_init();
                change_state(&state, STATE_MAP_PICKER);
            }
            break;
        }

        case STATE_KEY_CONFIG:
            if (key_config_update() == KEY_CONFIG_DONE) {
                key_config_cleanup();
                options_init();
                change_state(&state, STATE_OPTIONS);
            }
            break;

        case STATE_SOUND_CONFIG:
            if (sound_config_update() == SOUND_CONFIG_DONE) {
                sound_config_cleanup();
                options_init();
                change_state(&state, STATE_OPTIONS);
            }
            break;

        case STATE_INFO:
            if (info_update() == INFO_DONE) {
                info_cleanup();
                menu_init();
                change_state(&state, STATE_MENU);
            }
            break;

        case STATE_MAP_PICKER:
            /* Map picker screen: select maps for each multiplayer round.
             * Decompiled ref: FUN_1010_e231 (seg_1010:8533-8611).
             * Invoked from the options submenu (process_menu_selection item 0xC,
             * seg_1000:544-549). Returns to options when done. */
            if (map_picker_update() == MAP_PICKER_DONE) {
                map_picker_cleanup();
                options_init();
                change_state(&state, STATE_OPTIONS);
            }
            break;

        case STATE_PLAYER_SELECT: {
            PlayerSelectResult res;
            if (autoplay_active()) {
                /* MB_AUTOPLAY_PSELDWELL keeps the real screen up N frames
                 * (the state-transition shot on leaving captures its last
                 * frame). The real update runs during the dwell so the
                 * fade-in completes; no keys are injected. Then: scripted
                 * bot setup; all rounds random via untouched picker slots
                 * (32000), exactly as a user who never opens the map
                 * picker. */
                if (!autoplay_pselect_should_leave()) {
                    player_select_update();
                    res = PSELECT_NONE;
                } else {
                    autoplay_setup_players();
                    map_picker_reset();
                    res = PSELECT_RANDOM;
                }
            } else {
                res = player_select_update();
            }
            if (res == PSELECT_DONE || res == PSELECT_RANDOM) {
                player_select_cleanup();

                /* Setup game session */
                single_player_mode = (g_config.num_players == 1);
                /* The original clamps g_total_rounds to 0x37 = 55 in
                 * game_state_update at every round start (seg_1010:7450),
                 * bounding it to the 56-entry map-selection table. Clamp
                 * the same global once at match start — equivalent, since
                 * nothing raises it mid-match. */
                if (g_config.total_rounds > 55) {
                    g_config.total_rounds = 55;
                }
                rounds_remaining = g_config.total_rounds;
                current_map_index = 0;

                /* Both SP and MP enter the shop before the first round.
                 * Structural audit B1: the original (seg_1000:7103) runs the
                 * shop for SP as well (param_1=0 for solo-mode layout). The
                 * port previously skipped the shop in SP entirely, breaking
                 * the campaign economy. The round loads BEFORE the shop
                 * so the shop can preview the upcoming map. */
                load_round();
                shop_init();
                play_shop_music();
                change_state(&state, STATE_SHOP);
            } else if (res == PSELECT_CANCEL) {
                player_select_cleanup();
                play_menu_music();
                menu_init();
                change_state(&state, STATE_MENU);
            }
            break;
        }

        case STATE_SHOP: {
            ShopResult sr;
            if (autoplay_active()) {
                /* Normally leaves instantly; MB_AUTOPLAY_SHOPDWELL keeps
                 * the shop open N frames for screenshots. The
                 * real shop_update still runs during the dwell so the
                 * page state (items, names, thumbnail) is drawn — no
                 * keys are injected, so nothing is bought. */
                if (autoplay_shop_should_leave()) {
                    sr = SHOP_DONE;
                } else {
                    shop_update();
                    sr = SHOP_ACTIVE;
                }
            } else {
                sr = shop_update();
            }
            if (sr == SHOP_DONE) {
                shop_cleanup();
                begin_round();
                play_gameplay_music();
                debug_set_round(&current_round);
                change_state(&state, STATE_GAMEPLAY);
            } else if (sr == SHOP_ABORTED) {
                shop_cleanup();
                /* A round was pre-loaded for this shop — discard
                 * it; the original simply abandons the loaded map. */
                if (round_loaded) {
                    round_cleanup(&current_round);
                    round_loaded = false;
                }
                /* F10 at the shop (seg_1010:7047-7052) sets g_round_over
                 * and zeroes g_rounds_remaining — but the original then
                 * falls straight through the skipped round into the
                 * post-round block (interest + scoring run with everyone
                 * alive and earned already zeroed by the pre-shop
                 * game_state_update) and the post-match block: results +
                 * PLAYERS.DAT update in MP, GAMEOVER + HoF entry in SP.
                 * Only the player-select F10 (g_mode_flag) skips those.
                 * */
                music_stop();
                rounds_remaining = 0;
                for (int i = 0; i < g_num_active_players && i < MAX_PLAYERS; i++)
                    g_players[i].earned = 0;
                round_apply_interest(g_players, g_num_active_players);
                /* NULL map is safe: with every player alive the sole-
                 * survivor treasure bonus (the only map read) can't run. */
                round_apply_scoring(g_players, g_num_active_players, NULL);
                if (single_player_mode) {
                    sp_level_complete_init();
                    change_state(&state, STATE_SP_LEVEL_COMPLETE);
                } else {
                    results_accumulate_match_stats(g_players,
                                                   g_num_active_players);
                    results_init(g_players, g_num_active_players);
                    change_state(&state, STATE_RESULTS);
                }
            }
            break;
        }

        case STATE_GAMEPLAY: {
            if (autoplay_active()) {
                autoplay_drive_players(g_players, g_num_active_players);
                if (!autoplay_check_watchdog(&current_round)) {
                    change_state(&state, STATE_QUIT);
                    break;
                }
            }
            RoundState rs = round_update(&current_round, g_players,
                                         g_num_active_players);
            if (rs == ROUND_OVER) {
                /* Round-end mute: the original disables music before the
                 * per-player finalizers and the results screen
                 * (swap_display_pages at seg_1000:7299). Music next resumes
                 * at the following shop's order jump (or the menu reload). */
                music_stop();

                if (autoplay_active()) {
                    autoplay_trace_round(&current_round, g_players,
                                         g_num_active_players,
                                         g_config.total_rounds - rounds_remaining + 1,
                                         music_jump);
                }

                /* 7% savings interest (FUN_1010_ceb3): the original calls
                 * it per active player in the post-round block
                 * (seg_1000:7300-7309), unconditionally in BOTH modes (even
                 * on ESC), immediately BEFORE the scoring call — interest
                 * compounds on banked cash only, never on this round's
                 * still-unbanked pickups. */
                round_apply_interest(g_players, g_num_active_players);

                /* Apply scoring (FUN_1000_a17c). The original runs this
                 * unconditionally in the post-round block (seg_1000:7310)
                 * in BOTH modes — SP banks the level's earnings into cash,
                 * MP redistributes dead players' earnings — even when the
                 * round ended via ESC. */
                round_apply_scoring(g_players, g_num_active_players,
                                    &current_round.map);

                bool escaped = current_round.escaped;
                bool player_died = single_player_mode && g_players[0].dead;
                end_round();
                debug_set_round(NULL);

                /* In the original (seg_1000:7065-7172), g_rounds_remaining
                 * is incremented on SP death to cancel the loop's decrement,
                 * effectively retrying the same level. Only decrement when
                 * the round wasn't a SP death-retry. */
                if (!player_died) {
                    rounds_remaining--;
                }

                if (escaped) {
                    /* F10 in-round (seg_1000:7160-7164):
                     *  rounds_remaining = 0 and the round ends —
                     * but neither g_quit_flag nor g_mode_flag is set, so
                     * the original still runs the full post-match block:
                     * results + PLAYERS.DAT update in MP, GAMEOVER + HoF
                     * entry in SP. Interest and scoring for the aborted
                     * round were already applied above, exactly as in the
                     * original's unconditional post-round block. */
                    rounds_remaining = 0;
                }

                if (single_player_mode) {
                    /* Structural audit B2/E2: SP only shows level-complete /
                     * congrats / Hall-of-Fame at *match end*, not after every
                     * round. Per-round SP progress goes back through the shop
                     * to the next level (or a retry with a life deducted).
                     * (seg_1000:7315-7328 — GAMEOVER/CONGRATU run OUTSIDE the
                     * round loop.) */
                    bool match_end = false;
                    bool campaign_done = false;

                    if (escaped) {
                        /* F10 = give up: campaign over, GAMEOVER screen and
                         * HoF entry with the wallet as-is (seg_1000:7315-
                         * 7328 — the gates only check quit/mode_flag). */
                        match_end = true;
                    } else if (player_died) {
                        g_players[0].lives--;
                        if (g_players[0].lives < 1) {
                            match_end = true;   /* game over */
                        }
                        /* else: retry same level, fall through to shop */
                    } else {
                        current_map_index++;
                        if (current_map_index >= 15) {
                            match_end = true;
                            campaign_done = true;
                        }
                    }

                    if (match_end) {
                        music_stop();
                        if (campaign_done) {
                            sp_complete_init();
                            change_state(&state, STATE_SP_CONGRATS);
                        } else {
                            sp_level_complete_init();
                            change_state(&state, STATE_SP_LEVEL_COMPLETE);
                        }
                    } else {
                        /* Back to shop for the next SP round (retry or
                         * advance) — load it first. */
                        load_round();
                        shop_init();
                        play_shop_music();
                        change_state(&state, STATE_SHOP);
                    }
                } else if (rounds_remaining > 0) {
                    /* Multiplayer, more rounds to play: the original's
                     * per-round flow is simply round → shop → round
                     * (seg_1000:7065 loop). The results screen and the
                     * PLAYERS.DAT update are in the POST-MATCH block
                     * (outside the round loop, seg_1000:7314-7339) — the
                     * port previously showed results and saved records
                     * after every round. */
                    current_map_index++;
                    load_round();
                    shop_init();
                    play_shop_music();
                    change_state(&state, STATE_SHOP);
                } else {
                    /* Multiplayer match complete: results screen, then the
                     * once-per-match stats merge (seg_1000:7323-7339).
                     * The results screen itself increments matches played /
                     * matches won in the match-stats blocks before the
                     * merge. */
                    results_accumulate_match_stats(g_players,
                                                   g_num_active_players);
                    results_init(g_players, g_num_active_players);
                    change_state(&state, STATE_RESULTS);
                }
            }
            break;
        }

        case STATE_SP_LEVEL_COMPLETE:
            /* GAMEOVER.SPY — shown once at SP match end (out of lives).
             * Decompiled ref: seg_1000:7315-7321 — outside the round loop. */
            if (sp_level_complete_update() == SPLC_DONE) {
                sp_level_complete_cleanup();
                hof_init(g_players[0].name,
                         current_map_index,
                         g_players[0].cash);
                change_state(&state, STATE_HALL_OF_FAME);
            }
            break;

        case STATE_SP_CONGRATS:
            /* CONGRATU.SPY — shown once when the 15-level SP campaign is
             * completed. Decompiled ref: seg_1000:7315-7321. */
            if (sp_complete_update() == SPC_DONE) {
                sp_complete_cleanup();
                hof_init(g_players[0].name,
                         current_map_index,
                         g_players[0].cash);
                change_state(&state, STATE_HALL_OF_FAME);
            }
            break;

        case STATE_HALL_OF_FAME:
            /* Hall of Fame entry — runs once at match end, then back to menu.
             * Decompiled ref: seg_1000:7326-7327 (FUN_1000_aad7 call is the
             * entry; the HoF view is part of it). */
            if (hof_update() == HOF_DONE) {
                hof_cleanup();
                play_menu_music();
                menu_init();
                change_state(&state, STATE_MENU);
            }
            break;

        case STATE_RESULTS: {
            /* Reached only at match end — including F10 aborts
             * from the shop or mid-round. After the screen, merge
             * each player's match-stats block into PLAYERS.DAT — the
             * original calls FUN_1000_15c7 per player in the post-match
             * block (seg_1000:7329-7339), multiplayer only, skipped only
             * on program quit or a player-select F10 (g_mode_flag). */
            bool results_done;
            if (autoplay_active()) {
                /* MB_AUTOPLAY_RESDWELL keeps the real screen up N frames
                 * for screenshots (fade-in runs via results_update; no
                 * keys are consumed). */
                if (autoplay_results_should_leave()) {
                    results_done = true;
                } else {
                    results_update();
                    results_done = false;
                }
            } else {
                results_done = (results_update() == RESULTS_DONE);
            }
            if (results_done) {
                results_cleanup();
                {
                    PlayerDatabase *db = player_select_get_db();
                    for (int i = 0; i < g_num_active_players && i < MAX_PLAYERS; i++) {
                        player_db_merge_match_stats(db, &g_players[i]);
                    }
                    player_db_save(db, "assets/players.dat");
                }
                if (autoplay_active()) autoplay_match_completed();
                play_menu_music();
                menu_init();
                change_state(&state, STATE_MENU);
            }
            break;
        }

        default:
            break;
        }

        /* Draw at native resolution */
        BeginTextureMode(target);
            ClearBackground(BLACK);
            switch (state) {
            case STATE_TITLE:         title_draw();          break;
            case STATE_MENU:          menu_draw();           break;
            case STATE_OPTIONS:       options_draw();        break;
            case STATE_INFO:          info_draw();           break;
            case STATE_PLAYER_SELECT: player_select_draw();  break;
            case STATE_SHOP:          shop_draw();           break;
            case STATE_GAMEPLAY:
                round_draw(&current_round, g_players, g_num_active_players);
                break;
            case STATE_RESULTS:            results_draw();            break;
            case STATE_SP_LEVEL_COMPLETE:  sp_level_complete_draw();  break;
            case STATE_SP_CONGRATS:        sp_complete_draw();        break;
            case STATE_HALL_OF_FAME:       hof_draw();                break;
            case STATE_KEY_CONFIG: key_config_draw(); break;
            case STATE_SOUND_CONFIG: sound_config_draw(); break;
            case STATE_MAP_PICKER: map_picker_draw(); break;
            default: break;
            }
        EndTextureMode();

        /* Scale up to window */
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(
                target.texture,
                (Rectangle){0, 0, RENDER_WIDTH, -RENDER_HEIGHT},
                (Rectangle){0, 0, WINDOW_WIDTH, WINDOW_HEIGHT},
                (Vector2){0, 0},
                0.0f,
                WHITE
            );
            /* Debug overlay drawn at window resolution (1280x960) */
            if (state == STATE_GAMEPLAY) {
                debug_draw(g_players, g_num_active_players);
            } else {
                debug_draw(NULL, 0);
            }
        EndDrawing();

        /* F12 = screenshot of render texture (native res) */
        if (IsKeyPressed(KEY_F12)) {
            Image shot = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&shot);
            ExportImage(shot, "screenshot.png");
            UnloadImage(shot);
            TraceLog(LOG_INFO, "Screenshot saved: screenshot.png");
        }
    }

    sfx_shutdown();
    music_shutdown();
    sprites_cleanup();
    UnloadRenderTexture(target);
    CloseWindow();
    if (log_file) fclose(log_file);
    return autoplay_finish();
}
