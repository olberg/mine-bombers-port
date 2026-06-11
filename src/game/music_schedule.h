#ifndef MUSIC_SCHEDULE_H
#define MUSIC_SCHEDULE_H

/*
 * Original song-position scheduling for the round loop.
 *
 * The original plays one module per context:
 *   - Menu: HUIPPE.S3M (init_music_playback, seg_1010:3255; filename verified
 *     in MB.EXE bytes at file offset 102244).
 *   - Match: OEKU.S3M loaded once at PLAY (FUN_1010_5c83, seg_1010:3144),
 *     then never reloaded — only *order jumps* per round:
 *       - Before the shop:  jump to order 0x54 (seg_1000:7100). OEKU's orders
 *         84..90 (1-based) are a distinct tail section = the shop music.
 *       - After the shop:   jump to a random entry of a 14-byte order table
 *         at 0x1038:0x0010 (seg_1000:7122-7126) = gameplay music start.
 *
 * Order numbers are 1-BASED: the player indexes its order list with
 * (order - 1) (seg_1018:610-614), and FUN_1018_0855 rejects order 0.
 */

#define MUSIC_ORDER_SHOP        0x54  /* seg_1000:7100 */
#define MUSIC_GAME_ORDER_COUNT  14

/* The 1-based order the shop screen plays from (= MUSIC_ORDER_SHOP). */
int music_schedule_shop_order(void);

/* Pick this round's gameplay start order: game_orders[Random(14)].
 * Consumes one draw from the global PRNG (as the original does at
 * seg_1000:7124). */
int music_schedule_gameplay_order(void);

/* The 14-entry table of 1-based OEKU order numbers (0x1038:0x0010). */
const unsigned char *music_schedule_game_orders(void);

#endif
