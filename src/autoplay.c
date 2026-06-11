#include "autoplay.h"
#include "game/config.h"
#include "game/entity.h"
#include "game/player_select.h"
#include "game/player_db.h"
#include "input/input.h"
#include "util/prng.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool active;
static bool failed;
static bool completed;
static int  menu_visits;
static int  shot_seq;
static bool shots_enabled = true;
static int  rounds_setting = 5;
static int  time_setting = 45;          /* seconds per round */
static FILE *trace;
static char trace_path[260] = "autoplay_trace.jsonl";

/* Private LCG for bot decisions so the harness never disturbs the game's
 * PRNG draw sequence (mb_random). Seeded from MB_SEED for determinism. */
static uint32_t bot_seed;

static uint32_t bot_rand(uint32_t n)
{
    bot_seed = bot_seed * 0x08088405u + 1;
    return (uint32_t)(((uint64_t)bot_seed * n) >> 32);
}

/* Per-bot movement state */
static int bot_dir_frames[MAX_PLAYERS];   /* frames until direction change */
static int bot_dir[MAX_PLAYERS];

/* MB_AUTOPLAY_MIDSHOT=N: take one screenshot N driven frames into the
 * match (mid-round visual checks). 0 = off. */
static int midshot_frame;
static int drive_frame_count;

/* MB_AUTOPLAY_SHOPDWELL=N: frames to keep each shop open. */
static int shop_dwell;
static int shop_dwell_left;

/* MB_AUTOPLAY_PSELDWELL=N: frames to keep the player-select screen up. */
static int pselect_dwell;
static int pselect_dwell_left;

/* MB_AUTOPLAY_RESDWELL=N: frames to keep the results screen up. */
static int results_dwell;
static int results_dwell_left;

/* MB_AUTOPLAY_PLAYERS / MB_AUTOPLAY_VISCAP (visual-capture fidelity). */
static int  players_setting = 4;
static bool viscap;

void autoplay_init(void)
{
    const char *v = getenv("MB_AUTOPLAY");
    active = (v && v[0] == '1');
    if (!active) return;

    v = getenv("MB_AUTOPLAY_ROUNDS");
    if (v && atoi(v) > 0) rounds_setting = atoi(v);
    v = getenv("MB_AUTOPLAY_TIME");
    if (v && atoi(v) > 0) time_setting = atoi(v);
    v = getenv("MB_TRACE");
    if (v && v[0]) snprintf(trace_path, sizeof(trace_path), "%s", v);
    v = getenv("MB_AUTOPLAY_SHOTS");
    if (v && v[0] == '0') shots_enabled = false;
    v = getenv("MB_AUTOPLAY_MIDSHOT");
    if (v && atoi(v) > 0) midshot_frame = atoi(v);
    v = getenv("MB_AUTOPLAY_SHOPDWELL");
    if (v && atoi(v) > 0) shop_dwell = atoi(v);
    shop_dwell_left = shop_dwell;
    v = getenv("MB_AUTOPLAY_PSELDWELL");
    if (v && atoi(v) > 0) pselect_dwell = atoi(v);
    pselect_dwell_left = pselect_dwell;
    v = getenv("MB_AUTOPLAY_RESDWELL");
    if (v && atoi(v) > 0) results_dwell = atoi(v);
    results_dwell_left = results_dwell;
    v = getenv("MB_AUTOPLAY_PLAYERS");
    if (v && atoi(v) >= 2 && atoi(v) <= 4) players_setting = atoi(v);
    v = getenv("MB_AUTOPLAY_VISCAP");
    viscap = (v && v[0] == '1');

    bot_seed = mb_prng_get_seed();

    trace = fopen(trace_path, "w");
    if (!trace) {
        fprintf(stderr, "AUTOPLAY: cannot open trace file %s\n", trace_path);
        failed = true;
    } else {
        fprintf(trace,
                "{\"type\":\"match\",\"seed\":%u,\"players\":4,\"rounds\":%d,"
                "\"time_limit_s\":%d}\n",
                mb_prng_get_seed(), rounds_setting, time_setting);
        fflush(trace);
    }
    TraceLog(LOG_INFO, "AUTOPLAY: enabled, seed=%u rounds=%d trace=%s",
             mb_prng_get_seed(), rounds_setting, trace_path);
}

bool autoplay_active(void)
{
    return active;
}

void autoplay_configure_match(void)
{
    g_config.num_players = players_setting;
    g_config.total_rounds = rounds_setting;
    /* MB_AUTOPLAY_TIME is in SECONDS (harness knob); the config value
     * counts PIT ticks at 18.2065 Hz, so convert. */
    int32_t ticks = (int32_t)((int64_t)time_setting * 1193182 / 65536);
    g_config.time_limit_lo = (int16_t)(ticks & 0xFFFF);
    g_config.time_limit_hi = (int16_t)(ticks >> 16);
    player_input_inject_mode(true);
    /* Visual-capture runs keep the 60 FPS cap so dwell frame counts map to
     * wall-clock time (the capture script drives the screen with real key
     * events); soak/determinism runs stay uncapped. */
    if (!viscap) SetTargetFPS(0);
}

int autoplay_menu_wants_start(void)
{
    menu_visits++;
    return menu_visits == 1;
}

bool autoplay_shop_should_leave(void)
{
    if (shop_dwell <= 0) return true;
    if (--shop_dwell_left > 0) return false;
    shop_dwell_left = shop_dwell;  /* re-arm for the next round's shop */
    return true;
}

bool autoplay_pselect_should_leave(void)
{
    if (pselect_dwell <= 0) return true;
    if (--pselect_dwell_left > 0) return false;
    pselect_dwell_left = pselect_dwell;
    return true;
}

bool autoplay_results_should_leave(void)
{
    if (results_dwell <= 0) return true;
    if (--results_dwell_left > 0) return false;
    results_dwell_left = results_dwell;
    return true;
}

void autoplay_setup_players(void)
{
    PlayerDatabase *db = player_select_get_db();
    g_num_active_players = players_setting;
    for (int i = 0; i < players_setting; i++) {
        player_init_from_record_slot(&g_players[i], &db->records[i], i, i);
        if (viscap) {
            /* Match the real select-exit path: "N " prefix on the runtime
             * name (the shop masks the digit, FUN_1010_a933). */
            char base[25];
            snprintf(base, sizeof(base), "%s", g_players[i].name);
            snprintf(g_players[i].name, sizeof(g_players[i].name), "%d %s",
                     i + 1, base);
        } else {
            snprintf(g_players[i].name, sizeof(g_players[i].name),
                     "BOT%d", i + 1);
            /* Harness-only loadout: bots skip the shop, so give them bombs
             * up front or the kill/scoring paths never execute. */
            g_players[i].weapons[player_weapon_index(WEAPON_SMALL_BOMB)] = 50;
            g_players[i].weapons[player_weapon_index(WEAPON_MEDIUM_BOMB)] = 20;
            g_players[i].selected_weapon = WEAPON_SMALL_BOMB;
        }
        bot_dir[i] = 1 + (int)bot_rand(4);
        bot_dir_frames[i] = 0;
    }
}

void autoplay_drive_players(const Player players[], int num_players)
{
    if (midshot_frame > 0 && ++drive_frame_count == midshot_frame) {
        TakeScreenshot("autoplay_midround.png");
    }
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        player_input_inject_clear(i);
        if (players[i].dead) continue;

        if (--bot_dir_frames[i] <= 0) {
            bot_dir[i] = 1 + (int)bot_rand(4);
            bot_dir_frames[i] = 10 + (int)bot_rand(40);
        }
        PlayerInputAction dir_action;
        switch (bot_dir[i]) {
        case 1:  dir_action = PLAYER_INPUT_DOWN;  break;
        case 2:  dir_action = PLAYER_INPUT_UP;    break;
        case 3:  dir_action = PLAYER_INPUT_LEFT;  break;
        default: dir_action = PLAYER_INPUT_RIGHT; break;
        }
        player_input_inject(i, dir_action, true, false);

        if (bot_rand(64) == 0)
            player_input_inject(i, PLAYER_INPUT_BOMB, true, true);
        if (bot_rand(128) == 0)
            player_input_inject(i, PLAYER_INPUT_CYCLE, true, true);
        if (bot_rand(256) == 0)
            player_input_inject(i, PLAYER_INPUT_REMOTE, true, true);
    }
}

bool autoplay_check_watchdog(const Round *r)
{
    /* Time limit guarantees an end within time_setting*60 frames; allow
     * generous slack for fades and the ending ramp. */
    int budget = time_setting * 60 + 3600;
    if (r->frame_counter > budget) {
        autoplay_fail("round exceeded frame budget");
        return false;
    }
    return true;
}

static const char *end_reason_name(RoundEndReason reason)
{
    switch (reason) {
    case ROUND_END_NONE:       return "none";
    case ROUND_END_INACTIVITY: return "inactivity";
    case ROUND_END_TIME:       return "time";
    case ROUND_END_ESC:        return "f10_abort";
    case ROUND_END_SYNC:       return "esc_round_end";
    case ROUND_END_EXIT:       return "sp_exit";
    default:                   return "?";
    }
}

void autoplay_trace_round(const Round *r, const Player players[],
                          int num_players, int round_number, int music_order)
{
    if (!trace) return;
    fprintf(trace,
            "{\"type\":\"round\",\"round\":%d,\"frames\":%d,"
            "\"end_reason\":\"%s\",\"music_order\":%d,\"players\":[",
            round_number, r->frame_counter, end_reason_name(r->end_reason),
            music_order);
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        const Player *p = &players[i];
        fprintf(trace,
                "%s{\"name\":\"%s\",\"cash\":%ld,\"earned\":%ld,"
                "\"kills\":%d,\"deaths\":%d,\"wins\":%d,\"dead\":%d}",
                i ? "," : "", p->name, (long)p->cash, (long)p->earned,
                p->kills, p->deaths, p->round_wins, p->dead);
    }
    fprintf(trace, "]}\n");
    fflush(trace);
}

void autoplay_notify_state(const char *from, const char *to)
{
    if (!active) return;
    if (trace) {
        fprintf(trace, "{\"type\":\"state\",\"from\":\"%s\",\"to\":\"%s\"}\n",
                from, to);
        fflush(trace);
    }
    if (shots_enabled) {
        char name[64];
        snprintf(name, sizeof(name), "autoplay_%02d_%s.png", shot_seq++, to);
        TakeScreenshot(name);
    }
}

void autoplay_match_completed(void)
{
    completed = true;
}

void autoplay_fail(const char *reason)
{
    failed = true;
    TraceLog(LOG_ERROR, "AUTOPLAY: FAIL — %s", reason);
    if (trace) {
        fprintf(trace, "{\"type\":\"fail\",\"reason\":\"%s\"}\n", reason);
        fflush(trace);
    }
}

int autoplay_finish(void)
{
    if (!active) return 0;
    if (!completed && !failed) autoplay_fail("exited before match completed");
    /* Leak rung: every round's entity list must have been freed.
     * Counter-based because MinGW ships no libasan. */
    if (entities_allocated_count() != 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "entity allocations leaked: %d",
                 entities_allocated_count());
        autoplay_fail(msg);
    }
    if (trace) {
        fprintf(trace, "{\"type\":\"match_end\",\"completed\":%s}\n",
                completed && !failed ? "true" : "false");
        fclose(trace);
        trace = NULL;
    }
    return (completed && !failed) ? 0 : 1;
}
