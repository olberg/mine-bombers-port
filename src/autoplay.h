#ifndef AUTOPLAY_H
#define AUTOPLAY_H

#include <stdbool.h>
#include "game/player.h"
#include "game/round.h"

/*
 * Unattended match harness.
 *
 * Enabled with env MB_AUTOPLAY=1: skips title/menu/shop via scripted state
 * transitions, fills all 4 slots with scripted bots (input injection), runs
 * a multiplayer match on random maps at uncapped speed, and writes one JSON
 * line per round to a trace file. Combined with MB_SEED (read by mb_prng),
 * two runs with the same seed must produce identical traces.
 *
 * Env vars:
 *   MB_AUTOPLAY=1          enable
 *   MB_SEED=<n>            PRNG seed (read by mb_prng_init)
 *   MB_AUTOPLAY_ROUNDS=<n> rounds to play (default 5)
 *   MB_AUTOPLAY_TIME=<n>   per-round time limit in seconds, converted to
 *                          PIT ticks for the config (default 45;
 *                          guarantees rounds end even if all bots survive)
 *   MB_TRACE=<path>        trace file (default autoplay_trace.jsonl)
 *   MB_AUTOPLAY_SHOTS=0    disable per-state-transition screenshots
 *   MB_AUTOPLAY_PLAYERS=<n> player count 2-4 (default 4)
 *   MB_AUTOPLAY_VISCAP=1   visual-capture fidelity: keep the database
 *                          record names (no BOTn rename) and skip the
 *                          harness bomb loadout, so the player-select and
 *                          shop screens render exactly as a fresh match
 *                          would for side-by-side DOSBox comparison
 *   MB_AUTOPLAY_PSELDWELL=N keep the player-select screen open N drawn
 *                          frames before the scripted setup (mirrors
 *                          MB_AUTOPLAY_SHOPDWELL) so the harness can
 *                          screenshot its render
 *   MB_AUTOPLAY_RESDWELL=N keep the match-results screen open N drawn
 *                          frames before returning to the menu
 */

/* Read env config. Call once at startup before the window opens. */
void autoplay_init(void);

bool autoplay_active(void);

/* Override config for the scripted match (4 players, N rounds, time limit)
 * and switch input to injection mode. Call after config_load(). */
void autoplay_configure_match(void);

/* Menu decision: MENU_START on the first visit, quit afterwards.
 * Returns 1 = start match, 0 = quit. */
int autoplay_menu_wants_start(void);

/* Shop dwell (MB_AUTOPLAY_SHOPDWELL=N, default 0): with N > 0 the shop
 * state stays open N drawn frames before auto-leaving, so the harness
 * can screenshot the shop render (the state-transition shot fired on
 * leaving captures the shop's last frame). Returns true when the shop
 * should close this frame. */
bool autoplay_shop_should_leave(void);

/* Player-select dwell (MB_AUTOPLAY_PSELDWELL=N, default 0): with N > 0
 * the player-select state stays up N drawn frames before the scripted
 * setup runs. Returns true when the screen should be left this frame. */
bool autoplay_pselect_should_leave(void);

/* Results dwell (MB_AUTOPLAY_RESDWELL=N, default 0). */
bool autoplay_results_should_leave(void);

/* Set up g_players with bot profiles from the player database.
 * Call in place of player_select_update(). */
void autoplay_setup_players(void);

/* Inject this frame's bot inputs. Call once per frame during gameplay. */
void autoplay_drive_players(const Player players[], int num_players);

/* Round watchdog: returns false (and flags failure) if the round has run
 * past any plausible end condition — a determinism/flow bug. */
bool autoplay_check_watchdog(const Round *r);

/* Append one round record to the trace. */
void autoplay_trace_round(const Round *r, const Player players[],
                          int num_players, int round_number, int music_order);

/* Screenshot + trace hook for state transitions. */
void autoplay_notify_state(const char *from, const char *to);

/* Mark the match as completed (reached final results & back to menu). */
void autoplay_match_completed(void);

/* Flag an assertion failure (forces nonzero exit code). */
void autoplay_fail(const char *reason);

/* Close trace file; returns process exit code (0 = success). */
int autoplay_finish(void);

#endif
