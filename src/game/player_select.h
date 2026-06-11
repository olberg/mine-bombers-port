#ifndef PLAYER_SELECT_H
#define PLAYER_SELECT_H

#include "game/player.h"
#include "game/player_db.h"

/*
 * Player select screen ("new game setup"): choose player profiles from the
 * 32-record database before a match. Background: IDENTIFW.SPY.
 *
 * The screen is a VERTICAL stack of 4 player slot rows plus a PLAY row
 * (53px stride); the shovel cursor starts on PLAY. Selecting a slot opens
 * the record list on the right (arrow cursor); records can be created by
 * typing a name and deleted with Del. Pre-selected records come from
 * IDENTIFY.DAT. Inactive slot rows are blacked out.
 * Decompiled refs: FUN_1000_3276 + helpers (see player_select.c header).
 */

typedef enum {
    PSELECT_NONE,       /* still selecting */
    PSELECT_DONE,       /* all players ready */
    PSELECT_CANCEL,     /* user cancelled */
    PSELECT_RANDOM      /* autoplay harness only — scripted start. No user
                           key maps here; the original's -0x58 key at
                           player select is F10 = abort. */
} PlayerSelectResult;

/* Load PLAYERS.SPY background, load player database. */
void player_select_init(void);

/* Handle input. Returns result when selection is complete or cancelled. */
PlayerSelectResult player_select_update(void);

/* Draw the player select screen. */
void player_select_draw(void);

/* Unload resources. */
void player_select_cleanup(void);

/*
 * Pure logic helpers (testable without GPU):
 */

/* Get the number of active slots (equals g_config.num_players). */
int player_select_slot_count(void);

/* Check if all active slots have their ready flag set. */
bool player_select_all_ready(void);

/* Wrap a record index into 0..PLAYER_DB_SLOTS-1 range. */
int player_select_wrap_record(int index);

/* Get a pointer to the player database (valid until next player_select_init).
 * Used by main.c for stat persistence after multiplayer rounds. */
PlayerDatabase *player_select_get_db(void);

#endif
