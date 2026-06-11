#ifndef SHOP_H
#define SHOP_H

#include "game/player.h"
#include "game/map.h"

/*
 * Pre-round shop screen where players purchase weapons.
 * Background: SHOPPIC.SPY. All players use their own key bindings
 * simultaneously on the same keyboard.
 *
 * Buy: deduct price from cash, increment weapon inventory.
 * Sell: refund half price, decrement weapon inventory.
 */

typedef enum {
    SHOP_ACTIVE,
    SHOP_DONE,     /* All players left (or ESC closed the last page) —
                      continue to gameplay */
    SHOP_ABORTED   /* F10 pressed — match ends; the caller still runs the
                      post-round/post-match blocks */
} ShopResult;

/* Load SHOPPIC.SPY background, init shop state. */
void shop_init(void);

/* Provide the already-loaded next round's map and the rounds-left count
 * for the shop's NEXT LEVEL panel. The original loads the map
 * BEFORE the shop and FUN_1010_b293 draws a 64x45 one-pixel-per-tile
 * thumbnail at (288, 51) — darkness off only — plus the rounds-remaining
 * number at (306, 120). Pass map = NULL to disable the preview. The map
 * pointer must stay valid for the lifetime of the shop screen. */
void shop_set_next_round(const TileMap *map, int rounds_left);

/* Handle per-player input.
 * Returns SHOP_DONE when all players pressed LEAVE,
 * SHOP_ABORTED when ESC was pressed. */
ShopResult shop_update(void);

/* Draw the shop screen. */
void shop_draw(void);

/* Unload resources. */
void shop_cleanup(void);

/*
 * Pure logic helpers (testable without GPU):
 */

/* Attempt to buy a weapon for the player. Returns true if purchase succeeded. */
bool shop_buy_weapon(Player *p, uint8_t weapon_tile_id);

/* Attempt to sell a weapon for the player. Returns true if sale succeeded. */
bool shop_sell_weapon(Player *p, uint8_t weapon_tile_id);

/* Get the active (possibly randomized) price for a shop item (0-based index).
 * When free market (option_toggle[1]) is ON, prices vary per shop visit. */
int16_t shop_get_active_price(int shop_idx);

/* Recompute active prices based on current config (free market toggle).
 * Called internally by shop_init() and on every page advance — the
 * original rerolls per shop call, i.e. per page (FUN_1010_a2de at
 * seg_1010:7004). Exposed for testing. */
void shop_compute_prices(void);

/* Page structure — mirrors the original's per-pair shop calls
 * (seg_1000:7103-7119): 1P → one single-panel call, 2P → one dual call,
 * 3P → dual (P1/P2) + single (P3), 4P → dual + dual. */

/* Number of shop pages for the player count: 1-2P → 1, 3-4P → 2. */
int shop_page_count(int num_players);

/* Players shown on a page: [*first, *last) player indices. */
void shop_page_range(int num_players, int page, int *first, int *last);

/* Whether the page shows two panels (the original's param_1 == 1). */
bool shop_page_is_dual(int num_players, int page);

/* Sell gating: selling requires option_toggle[2], but
 * single-player forces it ON (seg_1010:7172). */
bool shop_selling_enabled_for(int num_players, uint8_t selling_toggle);

#endif
