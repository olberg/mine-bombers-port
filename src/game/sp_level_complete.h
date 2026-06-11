#ifndef SP_LEVEL_COMPLETE_H
#define SP_LEVEL_COMPLETE_H

/* Singleplayer level complete / game over screen.
 * Shows GAMEOVER.SPY, waits 1s then keypress.
 * Matches singleplayer_level_complete() at seg_1000:6434-6455. */

typedef enum {
    SPLC_ACTIVE,
    SPLC_DONE
} SpLevelCompleteResult;

/* Initialize level complete screen. */
void sp_level_complete_init(void);

/* Update (handles timing and input). */
SpLevelCompleteResult sp_level_complete_update(void);

/* Draw the level complete screen. */
void sp_level_complete_draw(void);

/* Clean up resources. */
void sp_level_complete_cleanup(void);

#endif
