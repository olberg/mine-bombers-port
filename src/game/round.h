#ifndef ROUND_H
#define ROUND_H

#include "game/map.h"
#include "game/entity.h"
#include "game/player.h"
#include <stdbool.h>

/*
 * Game round lifecycle: load map, run gameplay loop, determine winner.
 *
 * Frame timing (from decompiled game_state_update):
 *   Every frame:    player input, movement, bomb fuses
 *   Every 2 frames: bomb stages, weapon processing, death check
 *   Every 5 frames: entity collision, alive-player check
 *   Every 20 frames: timer bar, treasure check
 *   Every 26 frames: AI decisions
 *
 * End conditions:
 *   - <2 players alive: inactivity += 3 per 5 frames, ends at 100
 *   - All treasures collected: inactivity += 20 per 20 frames
 *   - Time runs out: immediate end
 *   - ESC: immediate end (rounds remaining set to 0)
 */

/* Inactivity thresholds */
#define INACTIVITY_MAX         100
#define INACTIVITY_FEW_ALIVE     3  /* increment per 5-frame check */
#define INACTIVITY_TREASURES    20  /* increment per 20-frame check */

/* Welfare floor (FUN_1000_a17c, MP only): if wallet < 100 at round end,
 * ADD 150 to it (MB.EXE bytes 45897-45934: add ax,0x96 / adc dx,0). */
#define CASH_FLOOR_THRESHOLD   100
#define CASH_FLOOR_BONUS       150

typedef enum {
    ROUND_FADE_IN,      /* palette fade from black */
    ROUND_RUNNING,
    ROUND_ENDING,       /* inactivity counter active */
    ROUND_FADE_OUT,     /* palette fade to black */
    ROUND_OVER
} RoundState;

/* Why the round ended (for traces and end-condition tests). The original
 * does not track this — every end path just fades and sets g_round_over —
 * so it is observability-only and must not drive gameplay decisions. */
typedef enum {
    ROUND_END_NONE = 0,
    ROUND_END_INACTIVITY,   /* inactivity counter > 100 (seg_1000:7272) */
    ROUND_END_TIME,         /* MP time limit expiry (seg_1000:7290) */
    ROUND_END_ESC,          /* match abort — key is F10, scancode 0x44
                               (seg_1000:7160) */
    ROUND_END_SYNC,         /* MP-only round end — key is ESC, scancode
                               0x01 (seg_1000:7141) */
    ROUND_END_EXIT          /* SP exit tile reached */
} RoundEndReason;

typedef struct {
    TileMap      map;
    Entity      *entity_head;    /* linked list of monsters */
    int          frame_counter;
    int          inactivity;     /* countdown to round end */
    /* The time limit counts PIT TICKS (18.2065 Hz): the original's
     * round init copies the config value into g_time_remaining
     * verbatim (seg_1010:7447) and its INT8 ISR decrements once per
     * tick (measured in DOSBox: config 45 -> 2.47 s round).
     * One tick = 65536/1193182 s = 54.9 ms; round_update converts
     * 60 fps frames to ticks exactly via tick_accum. */
    int          time_remaining; /* ticks until timeout (-1 = no limit) */
    int          time_total;     /* starting ticks (for timer bar) */
    int32_t      tick_accum;     /* frame->tick remainder accumulator */
    RoundState   state;
    int          round_number;
    bool         single_player;
    bool         escaped;        /* match aborted (F10) */
    bool         darkness_enabled; /* fog-of-war / darkness (seg_1010:7197-7201).
                                    * When active, minimap overlay is also drawn. */
    bool         paused;          /* pause state (seg_1000:7146, FUN_1000_7194) */
    int          fade_step;       /* current fade step (0..FADE_STEPS) */
    RoundEndReason end_reason;    /* why the round ended (observability only) */
} Round;

/* Initialize a round: load map, spawn entities, set starting positions. */
bool round_init(Round *r, const char *map_path, int round_number,
                bool single_player, int time_limit);

/* Initialize a round with a randomly generated map (FUN_1008_1263).
 * treasure_count = config_byte from options (default 45). */
bool round_init_random(Round *r, int round_number, int time_limit,
                       int treasure_count);

/* Update one frame of gameplay. Returns current state. */
RoundState round_update(Round *r, Player players[], int num_players);

/* Draw the current round state (map, sprites, HUD).
 * Also processes screen shake: applies vertical offset on odd shake values
 * and decrements the shake counter (seg_1010:7705-7725). */
void round_draw(Round *r, const Player players[], int num_players);

/* Clean up round resources. */
void round_cleanup(Round *r);

/* Apply end-of-round scoring (FUN_1000_a17c). Runs in BOTH modes:
 * SP banks earnings into cash; MP redistributes dead players' earnings.
 * map is needed for the sole-survivor remaining-treasure bonus (may be
 * NULL only when no treasure bonus is possible). */
void round_apply_scoring(Player players[], int num_players, const TileMap *map);

/* Apply the 7% savings interest to each active player's wallet
 * (FUN_1010_ceb3: wallet := Round(wallet * 1.07), called per active player
 * at seg_1000:7300-7309 — BOTH modes, unconditionally at round end,
 * immediately BEFORE round_apply_scoring, so interest compounds on banked
 * cash only, never on this round's pickups). */
void round_apply_interest(Player players[], int num_players);

/* Resolve a player's movement direction from held keys, exactly as the
 * original's process_weapons does (seg_1000:2608-2630): held-key priority
 * Up > Right > Down > Left > Stop, stop level-triggered and lowest priority,
 * direction unchanged when nothing is held. Pure over the input layer —
 * testable headlessly via player_input_inject. */
int round_resolve_direction(int player_idx, int current_direction);

/* Count alive (non-dead) players. */
int round_count_alive(const Player players[], int num_players);

/* Check if single-player reached the exit tile ('k'). */
bool round_check_exit_tile(const Player *p, const TileMap *map);

/* Count remaining treasure tiles on the map. */
int round_count_treasures(const TileMap *map);

/* Place players at starting corner positions with random pair swap.
 * Also clears paths near spawn corners (seg_1010:7456-7512). */
void round_place_players(Round *r, Player players[], int num_players);

#endif
