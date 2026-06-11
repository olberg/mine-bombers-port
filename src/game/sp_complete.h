#ifndef SP_COMPLETE_H
#define SP_COMPLETE_H

/* Singleplayer campaign completion congratulations screen.
 * Shows CONGRATU.SPY, plays SFX, waits 3s then keypress.
 * Matches singleplayer_game_complete() at seg_1000:6957-6979. */

typedef enum {
    SPC_ACTIVE,
    SPC_DONE
} SpCompleteResult;

/* Initialize congratulations screen. */
void sp_complete_init(void);

/* Update (handles timing and input). */
SpCompleteResult sp_complete_update(void);

/* Draw the congratulations screen. */
void sp_complete_draw(void);

/* Clean up resources. */
void sp_complete_cleanup(void);

#endif
