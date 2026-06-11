#include "game/shop.h"
#include "game/player.h"
#include "game/weapons.h"
#include "game/sprites.h"
#include "game/config.h"
#include "game/hud.h"
#include "loaders/spy_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util/prng.h"

#define FADE_STEPS     7

/*
 * Shop screen — decompiled ref:
 *   Main loop:     seg_1010_graphics.c:6987-7057 (mislabeled "apply_bot_ai")
 *   Setup:         seg_1010_graphics.c:6514-6543 (FUN_1010_b293, loads SHOPPIC.SPY)
 *   Per-player:    seg_1010_graphics.c:6548-6983 (FUN_1010_b316, cursor/buy/sell)
 *   Price lookup:  seg_1010_graphics.c:6320-6414 (FUN_1010_af5c)
 *   Item grid:     seg_1010_graphics.c:6077-6261 (FUN_1010_a9c9, renders one item cell)
 *   Price text:    seg_1010_graphics.c:6418-6454 (FUN_1010_b0fa)
 *   Item loop:     seg_1010_graphics.c:6265-6287 (FUN_1010_ae88, all 28 items)
 *
 * Layout: 4 columns × 7 rows grid (28 items: 27 purchasable + LEAVE).
 *   Panel 1 ("side 1"):  X_base = 0x160 (352) — right side, player 1 / single-player
 *   Panel 2 ("side 2"):  X_base = 0x20  (32)  — left side, player 2
 *
 *   Grid cell coordinates (blit_sprite uses Y, X order):
 *     col = (item-1) % 4,  row = (item-1) / 4
 *     Border sprite (64×48): Y = row*48 + 96,   X = panel_base + col*64
 *     Item sprite (30×30):   Y = row*48 + 99,   X = panel_base + col*64 + 17
 *     Price text:            Y = row*48 + 132,   X = panel_base + col*64 + 12
 *     Inventory bar (5px):   Y = row*48 + 99+56, X = panel_base + col*64 + 56 area
 *
 *   Info displays (print_string_at uses Y, X order):
 *     Player name:  Y=16, X=439 (P1) / X=19 (P2)
 *     Cash:         Y=44, X=455 (P1) / X=35 (P2)
 *     Digging:      Y=30, X=455 (P1) / X=35 (P2)
 *     Item count:   Y=58, X=455 (P1) / X=35 (P2)
 *
 * Cursor navigation (1-based, 1..0x1C=28):
 *   LEFT  (+0xF3): cursor -= 1 (min 1)
 *   RIGHT (+0xF4): cursor += 1 (max 0x1C)
 *   UP    (+0xF5): cursor -= 4 (if < 5, clamp to 1)
 *   DOWN  (+0xF6): cursor += 4 (if >= 0x18, clamp to 0x1C)
 *
 * Buy (+0xF8): deduct price, increment weapons[cursor*2+0xAE]
 * Sell (+0xFA): requires weapons[cursor*2+0xAE] > 0, add price/2 to cash
 * Item 0x1C = LEAVE: marks player done (no buy/sell)
 * Items 0x12-0x14: stat upgrades (call game_state_update after buy/sell)
 * Item 0x19: special (calls FUN_1010_c15c)
 */

/*
 * Shop item table: 27 purchasable items in shop display order (indices 1-27).
 * Order matches sequential inventory offsets: 0xB0, 0xB2, ..., 0xE4.
 * Prices from MB.EXE FUN_1010_a2de (6-byte Borland Pascal Real constants).
 * Sell price = buy_price / 2 (integer division, via FUN_1030_1545/1531/1549).
 */
typedef struct {
    const char *name;
    int16_t     price;       /* buy price */
    uint8_t     tile_id;     /* tile for sprite display (0 = no sprite) */
    int         weapon_idx;  /* index into p->weapons[] via weapon_inv_index, -1 if N/A */
    uint8_t     flags;       /* SHOP_FLAG_* */
} ShopItem;

#define SHOP_FLAG_NONE      0
#define SHOP_FLAG_UPGRADE   1  /* stat upgrade: buying calls game_state_update */
#define SHOP_FLAG_SPECIAL   2  /* special item (index 0x19) */

#define SHOP_ITEM_COUNT     27  /* purchasable items */
#define SHOP_LEAVE_INDEX    28  /* cursor position for LEAVE (1-based) */
#define SHOP_CURSOR_MIN      1
#define SHOP_CURSOR_MAX     SHOP_LEAVE_INDEX

/*
 * Mapping from shop display index (1-27) to inventory offset (0xB0-0xE4):
 *   inv_offset = shop_index * 2 + 0xAE
 *
 * Items at offsets 0xB8, 0xBA, 0xD2, 0xD4, 0xD6, 0xE0 have no weapon tile
 * — they are rockpicks, powerdrills, and stat upgrades.
 */
/*
 * Map shop item to weapons[] index.
 * Most items map via their tile_id through weapon_inv_index().
 * Items 5 and 6 (0-based 4 and 5) are bomb signature weapon slots
 * at inventory offsets 0xB8 and 0xBA — these map to the per-player
 * WEAPON_SIG0_SLOT and WEAPON_SIG1_SLOT in the weapons[] array.
 */
static int shop_item_weapon_idx(int shop_idx, uint8_t tile_id)
{
    if (shop_idx == 4) return WEAPON_SIG0_SLOT;  /* offset 0xB8 */
    if (shop_idx == 5) return WEAPON_SIG1_SLOT;  /* offset 0xBA */
    if (tile_id == 0) return -1;
    return weapon_inv_index(tile_id);
}

static const ShopItem g_shop_items[SHOP_ITEM_COUNT] = {
    /*  1: 0xB0 */ { "Small bomb",      1, 0x57, -1, SHOP_FLAG_NONE },
    /*  2: 0xB2 */ { "Medium bomb",     3, 0x58, -1, SHOP_FLAG_NONE },
    /*  3: 0xB4 */ { "Large bomb",     10, 0x59, -1, SHOP_FLAG_NONE },
    /*  4: 0xB6 */ { "Mine",          650, 0x9D, -1, SHOP_FLAG_NONE },
    /*  5: 0xB8 */ { "Bomb sig A",     15, 0x00, -1, SHOP_FLAG_NONE },
    /*  6: 0xBA */ { "Bomb sig B",     65, 0x00, -1, SHOP_FLAG_NONE },
    /*  7: 0xBC */ { "Arrow",         300, 0xA5, -1, SHOP_FLAG_NONE },
    /*  8: 0xBE */ { "Explosive",      25, 0x65, -1, SHOP_FLAG_NONE },
    /*  9: 0xC0 */ { "Rocket",        500, 0x5A, -1, SHOP_FLAG_NONE },
    /* 10: 0xC2 */ { "Mega bomb",      80, 0x7F, -1, SHOP_FLAG_NONE },
    /* 11: 0xC4 */ { "Power bomb",     90, 0xA4, -1, SHOP_FLAG_NONE },
    /* 12: 0xC6 */ { "Napalm",         35, 0x8A, -1, SHOP_FLAG_NONE },
    /* 13: 0xC8 */ { "Cluster bomb",  145, 0x80, -1, SHOP_FLAG_NONE },
    /* 14: 0xCA */ { "Super bomb",     15, 0x81, -1, SHOP_FLAG_NONE },
    /* 15: 0xCC */ { "Timer bomb",     80, 0xA1, -1, SHOP_FLAG_NONE },
    /* 16: 0xCE */ { "Remote bomb",   120, 0xA2, -1, SHOP_FLAG_NONE },
    /* 17: 0xD0 */ { "Tele bomb",      50, 0xA9, -1, SHOP_FLAG_NONE },
    /* 18: 0xD2 */ { "Rockpick",      400, 0x00, -1, SHOP_FLAG_UPGRADE },
    /* 19: 0xD4 */ { "Lg rockpick",  1100, 0x00, -1, SHOP_FLAG_UPGRADE },
    /* 20: 0xD6 */ { "Powerdrill",   1600, 0x00, -1, SHOP_FLAG_UPGRADE },
    /* 21: 0xD8 */ { "Warp bomb",      70, 0x9C, -1, SHOP_FLAG_NONE },
    /* 22: 0xDA */ { "Spawner",       400, 0x6E, -1, SHOP_FLAG_NONE },
    /* 23: 0xDC */ { "Prox mine",      50, 0x6F, -1, SHOP_FLAG_NONE },
    /* 24: 0xDE */ { "Rocket Lnchr",   80, 0x72, -1, SHOP_FLAG_NONE },
    /* 25: 0xE0 */ { "Steel plate",   800, 0x00, -1, SHOP_FLAG_SPECIAL },
    /* 26: 0xE2 */ { "Random bomb",    95, 0xAB, -1, SHOP_FLAG_NONE },
    /* 27: 0xE4 */ { "Money bomb",    575, 0xB0, -1, SHOP_FLAG_NONE },
};

/* Resolve weapon_idx fields (called once at init) */
static int resolved_weapon_idx[SHOP_ITEM_COUNT];

static void resolve_shop_weapon_indices(void)
{
    for (int i = 0; i < SHOP_ITEM_COUNT; i++) {
        resolved_weapon_idx[i] = shop_item_weapon_idx(i, g_shop_items[i].tile_id);
    }
}

/*
 * Get the weapon inventory index for a shop item.
 * Returns -1 for non-weapon items (rockpicks, upgrades, steel plates).
 */
static int shop_get_weapon_idx(int shop_idx_0based)
{
    if (shop_idx_0based < 0 || shop_idx_0based >= SHOP_ITEM_COUNT) return -1;
    return resolved_weapon_idx[shop_idx_0based];
}

/* ------------------------------------------------------------------ */
/* Shop screen layout (from FUN_1010_a9c9 / FUN_1010_b0fa)            */
/* ------------------------------------------------------------------ */

/*
 * Panel X base: right side for player 1 (side 1), left side for player 2 (side 2).
 * In single-player, only the right panel is used.
 */
#define PANEL_P1_XBASE  0x160  /* 352 — right side ("side 1") */
#define PANEL_P2_XBASE  0x020  /*  32 — left side ("side 2")  */

/* Grid dimensions */
#define GRID_COL_STEP   0x40  /* 64px per column (matches border sprite width) */
#define GRID_ROW_STEP   0x30  /* 48px per row (matches border sprite height) */

/* Offsets within each grid cell */
#define CELL_BORDER_Y   0x60  /* 96 — Y offset for border sprite (row*48 + 96) */
#define CELL_SPRITE_DX  0x11  /* 17 — X inset for item sprite within cell */
#define CELL_SPRITE_DY  0x03  /*  3 — Y inset for item sprite (99 - 96 = 3) */
#define CELL_PRICE_DX   0x0C  /* 12 — X inset for price text */
#define CELL_PRICE_DY   0x24  /* 36 — Y inset for price text (132 - 96 = 36) */

/* Info display positions (from FUN_1010_a933, FUN_1010_a7c0, FUN_1010_a87d, FUN_1010_a70c) */
#define INFO_NAME_Y     0x10  /* 16 */
#define INFO_DIG_Y      0x1E  /* 30 — digging power (color 3) */
#define INFO_CASH_Y     0x2C  /* 44 — cash display (color 5) */
#define INFO_COUNT_Y    0x3A  /* 58 — selected item count (color 1) */
#define INFO_P1_X       0x1C7 /* 455 — panel 1 info column */
#define INFO_P2_X       0x023 /* 35  — panel 2 info column */
#define INFO_NAME_P1_X  0x1B7 /* 439 */
#define INFO_NAME_P2_X  0x013 /* 19  */
#define INFO_NAME_X_OFF 0x10  /* 16  — Y offset for names (draw_color=1) */

/* Border sprite source rects in SIKA.SPY (64×48 each).
 * From capture_screen_region at seg_1010:4789-4795 */
#define BORDER_SEL_X   128   /* X_start=0x80, selected (DAT_1038_0664) */
#define BORDER_SEL_Y    92   /* Y_start=0x5C */
#define BORDER_UNSEL_X  64   /* X_start=0x40, unselected (DAT_1038_0668) */
#define BORDER_UNSEL_Y  92   /* Y_start=0x5C */
#define BORDER_W        64
#define BORDER_H        48

/*
 * Shop item sprites in SIKA.SPY (30×30 each).
 * Extracted from seg_1010:4696-4798, decode: capture_screen_region(buf, Y_end, X_end, Y_start, X_start).
 * Index matches shop item param_4 (1-based) in FUN_1010_a9c9.
 */
typedef struct {
    int x, y;  /* source position in SIKA.SPY */
} ShopSpritePos;

static const ShopSpritePos shop_sprites[SHOP_LEAVE_INDEX + 1] = {
    [0]    = {  0,   0 },  /* unused (0-indexed placeholder) */
    [1]    = {  0, 170 },  /* DAT_05f0: small bomb */
    [2]    = { 30, 170 },  /* DAT_05f4: medium bomb */
    [3]    = { 60, 170 },  /* DAT_05f8: large bomb */
    [4]    = {216, 140 },  /* DAT_061c: mine */
    [5]    = {240, 170 },  /* DAT_0618: small rockpick */
    [6]    = {210, 170 },  /* DAT_0614: large rockpick */
    [7]    = {246, 140 },  /* DAT_062c: arrow */
    [8]    = {270, 170 },  /* DAT_060c: explosive */
    [9]    = { 90, 170 },  /* DAT_05fc: rocket */
    [10]   = {120, 170 },  /* DAT_0600: mega bomb */
    [11]   = {246, 110 },  /* DAT_0628: power bomb */
    [12]   = { 90, 140 },  /* DAT_0608: napalm */
    [13]   = {150, 170 },  /* DAT_0604: cluster bomb */
    [14]   = {180, 170 },  /* DAT_0610: super bomb */
    [15]   = {276, 140 },  /* DAT_0620: timer bomb */
    [16]   = {276, 110 },  /* DAT_0624: remote bomb */
    [17]   = {216, 110 },  /* DAT_0630: tele bomb */
    [18]   = {  0, 140 },  /* DAT_0634: rockpick upgrade */
    [19]   = { 30, 140 },  /* DAT_0638: lg rockpick upgrade */
    [20]   = { 60, 140 },  /* DAT_063c: powerdrill upgrade */
    [21]   = { 30,  40 },  /* DAT_0640: warp bomb */
    [22]   = {232,  80 },  /* DAT_0644: spawner */
    [23]   = {262,  80 },  /* DAT_0648: prox mine */
    [24]   = {  0,  40 },  /* DAT_064c: rocket launcher */
    [25]   = {105,  40 },  /* DAT_0650: steel plate */
    [26]   = { 60,  40 },  /* DAT_0654: random bomb */
    [27]   = {  0,  90 },  /* DAT_0658: money bomb */
    [28]   = {120, 140 },  /* DAT_0674: LEAVE icon */
};

#define SHOP_SPRITE_SIZE 30

typedef enum { SHOP_FADE_IN, SHOP_SHOPPING, SHOP_FADE_OUT } ShopState;

/* Set when F10 is pressed — the match ends (original g_key_esc handler in
 * the shop loop, seg_1010:7047-7052: g_round_over = 1, rounds_remaining = 0).
 * Shop exit then returns SHOP_ABORTED instead of SHOP_DONE. */
static bool shop_aborted;

/* NEXT LEVEL panel data: the upcoming round's already-loaded map
 * and the rounds-left count, set by main.c before shop_init. The number
 * prints at (306, 120); the thumbnail draws at the minimap position. */
static const TileMap *next_map;
static int next_rounds_left;
#define NEXT_ROUNDS_X 306   /* 0x132 (FUN_1010_b293, print_string_at Y,X) */
#define NEXT_ROUNDS_Y 120   /* 0x78 */

void shop_set_next_round(const TileMap *map, int rounds_left)
{
    next_map = map;
    next_rounds_left = rounds_left;
}

/* Page structure: the original calls the shop screen once per
 * player pair (seg_1000:7103-7119) — 1P: one single-panel call; 2P: one
 * dual call; 3P: dual (P1/P2) then single (P3); 4P: dual then dual. */
int shop_page_count(int num_players)
{
    return num_players > 2 ? 2 : 1;
}

void shop_page_range(int num_players, int page, int *first, int *last)
{
    if (num_players <= 2) {
        *first = 0;
        *last  = num_players;
        return;
    }
    *first = page * 2;
    *last  = *first + 2;
    if (*last > num_players)
        *last = num_players;
}

bool shop_page_is_dual(int num_players, int page)
{
    /* The second panel exists when the page's second slot is populated —
     * the original's param_1: 0 for 1P and for 3P's second call, 1
     * otherwise. */
    return page * 2 + 1 < num_players;
}

bool shop_selling_enabled_for(int num_players, uint8_t selling_toggle)
{
    /* Single-player forces selling ON (seg_1010:7172 sets the
     * toggle global); multiplayer follows option_toggle[2]. */
    return (num_players <= 1) || (selling_toggle != 0);
}

static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

static BitmapFont shop_font;
static ShopState state;

/* Per-player shop state */
static int cursor_pos[MAX_PLAYERS];     /* 1-based cursor (1..28) */
static bool player_done[MAX_PLAYERS];   /* player selected LEAVE */

/* Active prices: base prices * free market multiplier.
 * Recomputed each shop_init() call.
 * Decompiled ref: seg_1010:5808-5907 (FUN_1010_a2de).
 * When option_toggle[1] (free market) is ON, prices are randomized.
 * When OFF, prices equal the base prices in g_shop_items[].price. */
static int16_t active_prices[SHOP_ITEM_COUNT];

/* For 3-4 players: which page is shown (0 = P1+P2, 1 = P3+P4) */
static int shop_page;

/* Selling enabled: requires option_toggle[2] or single-player.
 * Decompiled ref: seg_1010:7172 forces g_player3_color=1 for SP,
 *                 seg_1010:6719-6720 checks g_player3_color for sell. */
static bool selling_enabled;

/* Auto-repeat state per player — SHARED between buy (BOMB) and sell (CYCLE)
 * keys, matching original FUN_1010_b316 which stores one pair of counters
 * per player and both buy and sell blocks read/write the same slots
 * (decompiled seg_1010:6651-6774, offsets param_1+-0x12/-0x10/-0x16/-0x14).
 *
 * Ramp: while either key held, speed_counter increments each frame (caps at
 * 0x30). delay_counter counts down; when it reaches 0 a buy or sell fires
 * and delay resets to max(1, 0x14 - speed/3). Cursor movement resets both
 * to (speed=0, delay=1) so the first press after moving fires next frame
 * (matching decompiled 6586-6589 and siblings). */
static int repeat_speed[MAX_PLAYERS];   /* 0..0x30, ramps while held */
static int repeat_delay[MAX_PLAYERS];   /* countdown to next auto-fire */

#define REPEAT_SPEED_MAX    48   /* 0x30 in original */
#define REPEAT_DELAY_MAX    20   /* 0x14 — max interval between auto-fires */
#define REPEAT_DELAY_AFTER_MOVE 1  /* post-cursor-move delay: next-frame fire */

/* Panel X base: slot 0 = player 2 (left, "side 2"), slot 1 = player 1 (right, "side 1").
 * In the original, side 1 is rendered at 0x160, side 2 at 0x20. */
static const int panel_xbase[2] = {
    PANEL_P2_XBASE, PANEL_P1_XBASE
};

/* ------------------------------------------------------------------ */
/* Buy / sell logic                                                   */
/* ------------------------------------------------------------------ */

bool shop_buy_weapon(Player *p, uint8_t weapon_tile_id)
{
    const WeaponDef *wd = weapon_get_def(weapon_tile_id);
    if (!wd) return false;

    if (p->cash < wd->price) return false;

    int idx = weapon_inv_index(weapon_tile_id);
    if (idx < 0) return false;

    p->cash -= wd->price;
    p->weapons[idx]++;
    return true;
}

bool shop_sell_weapon(Player *p, uint8_t weapon_tile_id)
{
    const WeaponDef *wd = weapon_get_def(weapon_tile_id);
    if (!wd) return false;

    int idx = weapon_inv_index(weapon_tile_id);
    if (idx < 0) return false;

    if (p->weapons[idx] <= 0) return false;

    p->weapons[idx]--;
    p->cash += wd->price / 2;  /* sell refund = price / 2 (integer division) */
    return true;
}

/*
 * Buy/sell using shop item index (0-based, 0..26).
 * Used internally by the shop update loop.
 */
static bool shop_buy_item(Player *p, int shop_idx)
{
    if (shop_idx < 0 || shop_idx >= SHOP_ITEM_COUNT) return false;
    const ShopItem *item = &g_shop_items[shop_idx];
    int16_t price = active_prices[shop_idx];

    if (p->cash < price) return false;

    int widx = shop_get_weapon_idx(shop_idx);
    if (widx >= 0) {
        /* Normal weapon: deduct cash, increment inventory */
        p->cash -= price;
        p->weapons[widx]++;
        return true;
    }

    /* Non-weapon items (rockpicks, upgrades, steel plates): */
    if (item->flags & SHOP_FLAG_UPGRADE) {
        /* Stat upgrades: original increments the inventory count at offsets
         * 0xD2/0xD4/0xD6, then game_state_update() (seg_1010:7065) computes:
         *   bonus_stat = lg_rockpick * 3 + rockpick + powerdrill * 5
         * This is added to digging_power in the dig damage formula.
         * Recalculation happens at round start (player_reset_for_round). */
        p->cash -= price;
        if (shop_idx == 17) {        /* item 18 (1-based): Rockpick, offset 0xD2 */
            p->rockpick++;
        } else if (shop_idx == 18) { /* item 19: Lg rockpick, offset 0xD4 */
            p->lg_rockpick++;
        } else if (shop_idx == 19) { /* item 20: Powerdrill, offset 0xD6 */
            p->powerdrill++;
        }
        p->bonus_stat = player_dig_upgrade_bonus(p);
        return true;
    }

    if (item->flags & SHOP_FLAG_SPECIAL) {
        /* Steel plate (item 25, offset 0xE0): increments health upgrade
         * counter AND recalculates health immediately. Original calls
         * FUN_1010_c15c() right after the buy (seg_1010:6712-6714), which
         * sets health = steel_plates * 100 + 100 for all four players.
         * In practice only the buyer's count changed, so only their
         * max_health/health need updating here. */
        p->cash -= price;
        p->steel_plates++;
        int16_t new_health = (int16_t)(p->steel_plates * 100 + 100);
        p->max_health = new_health;
        p->health = new_health;
        return true;
    }

    /* Other non-weapon items: just deduct cash */
    p->cash -= price;
    return true;
}

static bool shop_sell_item(Player *p, int shop_idx)
{
    if (shop_idx < 0 || shop_idx >= SHOP_ITEM_COUNT) return false;
    const ShopItem *item = &g_shop_items[shop_idx];
    int16_t price = active_prices[shop_idx];

    int widx = shop_get_weapon_idx(shop_idx);
    if (widx >= 0) {
        if (p->weapons[widx] <= 0) return false;
        p->weapons[widx]--;
        p->cash += price / 2;
        return true;
    }

    /* Stat upgrades can be sold (decrement count, refund half price) */
    if (item->flags & SHOP_FLAG_UPGRADE) {
        int16_t *count = NULL;
        if (shop_idx == 17)      count = &p->rockpick;
        else if (shop_idx == 18) count = &p->lg_rockpick;
        else if (shop_idx == 19) count = &p->powerdrill;
        if (!count || *count <= 0) return false;
        (*count)--;
        p->cash += price / 2;
        p->bonus_stat = player_dig_upgrade_bonus(p);
        return true;
    }

    /* Steel plates can be sold. Original sell block also calls
     * FUN_1010_c15c() on cursor 0x19 (decompiled 6768-6770), so health is
     * recalculated after the decrement. Cap current health to the new
     * (lower) max so HUD/gameplay see a consistent value. */
    if (item->flags & SHOP_FLAG_SPECIAL) {
        if (p->steel_plates <= 0) return false;
        p->steel_plates--;
        p->cash += price / 2;
        int16_t new_max = (int16_t)(p->steel_plates * 100 + 100);
        p->max_health = new_max;
        if (p->health > new_max) p->health = new_max;
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Shop init / update / draw / cleanup                                */
/* ------------------------------------------------------------------ */

/* Compute active prices for this shop visit.
 * Decompiled ref: seg_1010:5812-5907 (FUN_1010_a2de).
 * When free market (option_toggle[1]) is OFF: multiplier = 1.0 (prices unchanged).
 * When ON: random(60) generates a multiplier that scales all prices.
 *
 * Original uses Borland Pascal 6-byte Real arithmetic with inline constants:
 *   r := Random(60);  // 0-59
 *   multiplier := f(r);  // some function of r
 *   for each item: price := Trunc(base * multiplier) + 1;
 *
 * The sign computation (seg_1010:5821-5822) splits at r=30:
 *   r < 30: prices below base (discount)
 *   r >= 30: prices above base (markup)
 *
 * Approximation: multiplier = (r + 30) / 60.0, giving range [0.5, 1.48].
 * This produces meaningful price variation while keeping the game balanced. */
void shop_compute_prices(void)
{
    bool free_market = (g_config.option_toggle[1] != 0);

    if (!free_market) {
        /* Fixed prices: multiplier = 1.0 */
        for (int i = 0; i < SHOP_ITEM_COUNT; i++) {
            active_prices[i] = g_shop_items[i].price;
        }
        return;
    }

    /* Free market: random multiplier per shop visit.
     * random(60) → 0-59, map to multiplier range ~0.5 to ~1.48. */
    int r = mb_random(60);
    for (int i = 0; i < SHOP_ITEM_COUNT; i++) {
        int base = g_shop_items[i].price;
        int price = (base * (r + 30)) / 60 + 1;
        if (price < 1) price = 1;
        active_prices[i] = (int16_t)price;
    }
}

int16_t shop_get_active_price(int shop_idx)
{
    if (shop_idx < 0 || shop_idx >= SHOP_ITEM_COUNT) return 0;
    return active_prices[shop_idx];
}

void shop_init(void)
{
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/SHOPPIC.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    shop_font = LoadFON("assets/FONTTI.FON", true);

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    resolve_shop_weapon_indices();
    shop_compute_prices();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        cursor_pos[i] = 1;  /* 1-based, start at first item */
        player_done[i] = false;
        repeat_speed[i] = 0;
        repeat_delay[i] = REPEAT_DELAY_AFTER_MOVE;
    }

    shop_page = 0;
    shop_aborted = false;
    state = SHOP_FADE_IN;

    /* Selling enabled: option_toggle[2] or single-player.
     * Single-player forces selling ON (seg_1010:7172: g_player3_color=1). */
    selling_enabled = shop_selling_enabled_for(g_config.num_players,
                                               g_config.option_toggle[2]);
}

ShopResult shop_update(void)
{
    switch (state) {
    case SHOP_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = SHOP_SHOPPING;
        }
        break;

    case SHOP_SHOPPING: {
        /* Page mapping:
         *   num_players <= 2: all players visible on page 0.
         *   num_players >  2: page 0 shows P1/P2, page 1 shows P3/P4. */
        int input_first, input_last;
        shop_page_range(g_config.num_players, shop_page,
                        &input_first, &input_last);

        /* In-shop exit keys (shop loop, seg_1010:7043-7052):
         *   F10 ("g_key_esc" byte): ends the MATCH — g_round_over = 1 and
         *     g_rounds_remaining = 0. The original then falls through the
         *     skipped round straight into the post-round and post-match
         *     blocks (interest, scoring, results/HoF) — handled by main.c
         *     on SHOP_ABORTED.
         *   ESC ("g_key_mp_sync" byte): closes the CURRENT page only — the
         *     match continues; for 3-4 players page 1 still runs (the
         *     seg_1000:7111/7118 calls are gated on g_round_over, which ESC
         *     leaves clear). The port previously treated ESC as a match
         *     abort to the menu — both the key and the destination were
         *     wrong. */
        if (IsKeyPressed(KEY_F10)) {
            shop_aborted = true;
            palette_start_fade_out(FADE_STEPS);
            state = SHOP_FADE_OUT;
            break;
        }
        if (input_pressed(INPUT_CANCEL)) {
            for (int i = input_first; i < input_last && i < MAX_PLAYERS; i++)
                player_done[i] = true;
            palette_start_fade_out(FADE_STEPS);
            state = SHOP_FADE_OUT;
            break;
        }

        /* Per-player input — restricted to the currently displayed page.
         * Decompiled ref: seg_1000:7103-7120. Original calls the shop function
         * once per page, so the per-player input handler (FUN_1010_b316) only
         * sees the pair for that page. Without this gate, players on a hidden
         * page could shop while their panel isn't drawn. */
        for (int i = input_first; i < input_last && i < MAX_PLAYERS; i++) {
            if (player_done[i]) continue;

            int cur = cursor_pos[i];
            bool moved = false;

            /*
             * Cursor movement matching decompiled FUN_1010_b316:
             *   LEFT  (0xF3): cur -= 1, min 1
             *   RIGHT (0xF4): cur += 1, max 0x1C (28)
             *   UP    (0xF5): cur -= 4, if < 5 clamp to 1
             *   DOWN  (0xF6): cur += 4, if >= 0x18 (24) clamp to 0x1C (28)
             */
            if (player_input_pressed(i, PLAYER_INPUT_LEFT)) {
                if (cur > 1) cur--;
                moved = true;
            }
            if (player_input_pressed(i, PLAYER_INPUT_RIGHT)) {
                if (cur < SHOP_CURSOR_MAX) cur++;
                moved = true;
            }
            if (player_input_pressed(i, PLAYER_INPUT_UP)) {
                if (cur < 5) {
                    cur = 1;
                } else {
                    cur -= 4;
                }
                moved = true;
            }
            if (player_input_pressed(i, PLAYER_INPUT_DOWN)) {
                if (cur < 0x18) {
                    cur += 4;
                } else {
                    cur = SHOP_CURSOR_MAX;
                }
                moved = true;
            }

            cursor_pos[i] = cur;

            /* Cursor move resets auto-repeat state (matches decompiled
             * 6586-6589: speed=0, delay=1 after any nav key). With delay=1,
             * the first BUY/SELL press on the new cell fires next frame. */
            if (moved) {
                repeat_speed[i] = 0;
                repeat_delay[i] = REPEAT_DELAY_AFTER_MOVE;
            }

            /* A fresh press edge fires immediately. The original's delay
             * counter persists after each fire (up to 0x14 iterations) and
             * only decrements while the key is held — but its DOS shop loop
             * ran unthrottled, so even a brief tap spanned enough iterations
             * to drain it and the next buy registered. At the port's fixed
             * 60 fps a tap drains only 1-2 frames' worth, so repeat taps on
             * the same cell were swallowed. Collapsing the delay on the
             * press edge (player_input_edge, no typematic) makes every
             * discrete press register while leaving the held-key ramp
             * (interval 0x14 - speed/3) untouched. */
            if (player_input_edge(i, PLAYER_INPUT_BOMB) ||
                (selling_enabled && player_input_edge(i, PLAYER_INPUT_CYCLE))) {
                repeat_delay[i] = 1;
            }

            /*
             * Buy and sell both use the SAME speed/delay counters in the
             * original (decompiled 6651-6774). Each block independently
             * ramps `repeat_speed` and ticks `repeat_delay` while its key
             * is held, firing when delay reaches zero.
             *
             * On fire, the new interval is `0x14 - speed/3` (capped ≥1),
             * so holding the key produces an accelerating auto-repeat.
             * No state reset on key release — state persists until cursor
             * moves.
             */

            /* Buy auto-repeat (BOMB key, original +0xF8, decompiled 6651-6717) */
            if (player_input_down(i, PLAYER_INPUT_BOMB)) {
                repeat_speed[i]++;
                if (repeat_speed[i] > REPEAT_SPEED_MAX)
                    repeat_speed[i] = REPEAT_SPEED_MAX;
                repeat_delay[i]--;
                if (repeat_delay[i] <= 0) {
                    if (cur == SHOP_LEAVE_INDEX) {
                        /* LEAVE: mark player done, no purchase */
                        player_done[i] = true;
                    } else {
                        shop_buy_item(&g_players[i], cur - 1);
                    }
                    int interval = REPEAT_DELAY_MAX - repeat_speed[i] / 3;
                    repeat_delay[i] = interval > 1 ? interval : 1;
                }
            }

            /* Sell auto-repeat (CYCLE key held, original +0xFA, decompiled
             * 6718-6774). Gated by `selling_enabled` (option_toggle[2] or SP).
             * Holding CYCLE on the LEAVE cell also marks the player done
             * (matches decompiled 6742-6744). */
            if (player_input_down(i, PLAYER_INPUT_CYCLE) && selling_enabled) {
                repeat_speed[i]++;
                if (repeat_speed[i] > REPEAT_SPEED_MAX)
                    repeat_speed[i] = REPEAT_SPEED_MAX;
                repeat_delay[i]--;
                if (repeat_delay[i] <= 0) {
                    if (cur == SHOP_LEAVE_INDEX) {
                        player_done[i] = true;
                    } else {
                        shop_sell_item(&g_players[i], cur - 1);
                    }
                    int interval = REPEAT_DELAY_MAX - repeat_speed[i] / 3;
                    repeat_delay[i] = interval > 1 ? interval : 1;
                }
            }
        }

        /* Exit / page transition.
         *
         * Original apply_bot_ai (seg_1010:6987-7057) is invoked once per
         * visible pair of players. For 3-4 players the round loop calls
         * it twice (seg_1000:7109-7119): first P1/P2, then P3/P4. Each
         * call runs its own palette_fade_in → shop loop → palette_fade_out
         * and re-rolls free-market prices via FUN_1010_a2de (seg_1010:7004).
         *
         * We simulate this by fading out when page 0 completes, rerolling
         * prices, then fading back in for page 1 (handled in SHOP_FADE_OUT
         * below). Single/two-player games fade out directly to SHOP_DONE.
         */
        {
            bool all_done = true;
            for (int i = 0; i < g_config.num_players && i < MAX_PLAYERS; i++) {
                if (!player_done[i]) { all_done = false; break; }
            }

            bool page0_complete = false;
            if (g_config.num_players > 2 && shop_page == 0) {
                page0_complete = true;
                for (int j = 0; j < 2 && j < g_config.num_players; j++) {
                    if (!player_done[j]) { page0_complete = false; break; }
                }
            }

            if (all_done || page0_complete) {
                palette_start_fade_out(FADE_STEPS);
                state = SHOP_FADE_OUT;
            }
        }
        break;
    }

    case SHOP_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data,
                                SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            if (shop_aborted) {
                return SHOP_ABORTED;
            }

            /* 3-4 players: page 0 just finished, there may be a page 1
             * still to run. Check for any page-1 players who haven't
             * pressed LEAVE yet. If so, advance the page, reroll prices
             * (matches original per-page FUN_1010_a2de call), and fade in
             * on the new panel. Otherwise exit the shop. */
            if (g_config.num_players > 2 && shop_page == 0) {
                bool page1_pending = false;
                for (int j = 2; j < g_config.num_players && j < MAX_PLAYERS; j++) {
                    if (!player_done[j]) { page1_pending = true; break; }
                }
                if (page1_pending) {
                    shop_page = 1;
                    shop_compute_prices();
                    palette_start_fade_in(FADE_STEPS);
                    state = SHOP_FADE_IN;
                    break;
                }
            }
            return SHOP_DONE;
        }
        break;
    }

    return SHOP_ACTIVE;
}

/*
 * Get grid cell top-left for a shop item (1-based index).
 *   col = (idx-1) % 4,  row = (idx-1) / 4
 *   cell_x = panel_xbase + col * 64
 *   cell_y = row * 48 + 96
 */
static void shop_cell_pos(int idx_1based, int px_base,
                           int *out_x, int *out_y)
{
    int col = (idx_1based - 1) % 4;
    int row = (idx_1based - 1) / 4;
    *out_x = px_base + col * GRID_COL_STEP;
    *out_y = row * GRID_ROW_STEP + CELL_BORDER_Y;
}

static int shop_get_qty(const Player *p, int shop_idx)
{
    int widx = shop_get_weapon_idx(shop_idx);
    if (widx >= 0) return p->weapons[widx];
    if (shop_idx == 17) return p->rockpick;
    if (shop_idx == 18) return p->lg_rockpick;
    if (shop_idx == 19) return p->powerdrill;
    if (g_shop_items[shop_idx].flags & SHOP_FLAG_SPECIAL) return p->steel_plates;
    return 0;
}

/*
 * Draw inventory quantity bar matching FUN_1010_a9c9 (seg_1010:6121-6140).
 * VERTICAL bar: 5px wide, up to 40px tall, at right side of cell (X offset 56).
 * Grows upward from bottom as quantity increases.
 * 5 gradient columns: 0xe, 0xd, 0xc, 0xb, 7 (left to right).
 *
 * Decompiled: iVar2 = panel_base + 0x38 + col*64 (bar X = cell_x + 56)
 *   Bar Y area: row*48+99 to row*48+139 (40px, = cell_y+3 to cell_y+43)
 *   fill_height = qty * 2, capped at 40 (0x28)
 *   Bar fills from bottom (cell_y+43) upward by fill_height pixels.
 */
static void draw_qty_bar(int cell_x, int cell_y, int qty)
{
    if (qty <= 0) return;
    int bar_x = cell_x + 56;    /* X offset 0x38 within cell */
    int bar_bottom = cell_y + 43; /* row*48 + 139 = cell_y + 43 */
    int fill_h = qty * 2;
    if (fill_h > 40) fill_h = 40; /* max 0x28 */

    static const uint8_t bar_colors[5] = { 0x0e, 0x0d, 0x0c, 0x0b, 0x07 };
    for (int col = 0; col < 5; col++) {
        Color c = palette_get_color(bar_colors[col]);
        DrawRectangle(bar_x + col, bar_bottom - fill_h, 1, fill_h, c);
    }
}

/*
 * Draw one player's shop panel.
 *   px_base: panel X base (352 for P1/right, 32 for P2/left)
 *   info_x:  X position for info text column
 *   name_x:  X position for player name
 */
static void draw_player_shop(const Player *p, int px_base, int cursor,
                              bool done, int info_x, int name_x)
{
    char buf[40];
    Texture2D atlas = sprites_get_atlas();
    Color col_white  = palette_get_color(1);
    Color col_red    = palette_get_color(3);
    Color col_yellow = palette_get_color(5);

    /* Player name (color 1, at fixed Y=16). FUN_1010_a933 copies the name
     * into a 24-char space-padded buffer and forces the FIRST character to
     * a space, hiding the "N " player-number prefix digit — so the visible
     * name starts two cells right of name_x (DOSBox-verified, 2026-06-11). */
    {
        char masked[26];
        snprintf(masked, sizeof(masked), "%s", p->name);
        if (masked[0] != '\0') masked[0] = ' ';
        DrawTextFON(&shop_font, masked, name_x, INFO_NAME_X_OFF, col_white);
    }

    /* Digging power (color 3, at Y=30) */
    snprintf(buf, sizeof(buf), "%d", (int)(p->digging_power + p->bonus_stat));
    DrawTextFON(&shop_font, buf, info_x, INFO_DIG_Y, col_red);

    /* Cash (color 5, at Y=44) */
    snprintf(buf, sizeof(buf), "%d", (int)p->cash);
    DrawTextFON(&shop_font, buf, info_x, INFO_CASH_Y, col_yellow);

    /* Selected item count (color 1, at Y=58) */
    if (cursor >= 1 && cursor <= SHOP_ITEM_COUNT) {
        int qty = shop_get_qty(p, cursor - 1);
        snprintf(buf, sizeof(buf), "%d", qty);
        DrawTextFON(&shop_font, buf, info_x, INFO_COUNT_Y, col_white);
    }

    /* Draw 28 item cells (27 items + LEAVE) */
    for (int idx1 = 1; idx1 <= SHOP_LEAVE_INDEX; idx1++) {
        int cx, cy;
        shop_cell_pos(idx1, px_base, &cx, &cy);

        bool selected = (idx1 == cursor);

        /* Border sprite (64×48) — selected or unselected frame */
        Rectangle border_src = {
            .x = selected ? BORDER_SEL_X : BORDER_UNSEL_X,
            .y = selected ? BORDER_SEL_Y : BORDER_UNSEL_Y,
            .width = BORDER_W,
            .height = BORDER_H
        };
        Rectangle border_dst = { (float)cx, (float)cy,
                                  (float)BORDER_W, (float)BORDER_H };
        DrawTexturePro(atlas, border_src, border_dst,
                       (Vector2){0, 0}, 0.0f, WHITE);

        /* Item sprite (30×30) inside the border */
        const ShopSpritePos *sp = &shop_sprites[idx1];
        Rectangle item_src = {
            .x = (float)sp->x, .y = (float)sp->y,
            .width = SHOP_SPRITE_SIZE, .height = SHOP_SPRITE_SIZE
        };
        Rectangle item_dst = {
            (float)(cx + CELL_SPRITE_DX), (float)(cy + CELL_SPRITE_DY),
            (float)SHOP_SPRITE_SIZE, (float)SHOP_SPRITE_SIZE
        };
        DrawTexturePro(atlas, item_src, item_dst,
                       (Vector2){0, 0}, 0.0f, WHITE);

        /* Price text below the border (color 5). The original appends
         * "$" to the number (FUN_1010_b0fa concatenates the string at
         * 1010:0xb0f2, bytes 01 24 = "$" in MB.EXE). */
        if (idx1 <= SHOP_ITEM_COUNT) {
            snprintf(buf, sizeof(buf), "%d$", active_prices[idx1 - 1]);
            DrawTextFON(&shop_font, buf,
                        cx + CELL_PRICE_DX, cy + CELL_PRICE_DY, col_yellow);

            /* Inventory quantity bar */
            draw_qty_bar(cx, cy, shop_get_qty(p, idx1 - 1));
        } else {
            /* LEAVE text */
            DrawTextFON(&shop_font, "LEAVE",
                        cx + CELL_PRICE_DX, cy + CELL_PRICE_DY, col_yellow);
        }
    }
}

void shop_draw(void)
{
    DrawTexture(bg_tex, 0, 0, WHITE);

    if (state == SHOP_FADE_IN && palette_is_fading()) return;

    /* NEXT LEVEL panel (FUN_1010_b293, seg_1010:6526-6542): the
     * rounds-remaining number prints at (X=306, Y=120) under the caption
     * baked into SHOPPIC.SPY, and the upcoming map draws as a 64x45
     * one-pixel-per-tile thumbnail at (288, 51) — the same renderer as
     * the darkness minimap — ONLY when darkness is off (the original
     * gates on g_minimap_enabled == 0: no peeking at a hidden map). */
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", next_rounds_left);
        DrawTextFON(&shop_font, buf, NEXT_ROUNDS_X, NEXT_ROUNDS_Y,
                    palette_get_color(1));
        bool darkness = (g_config.num_players == 1) ||
                        (g_config.option_toggle[0] != 0);
        if (next_map && !darkness) {
            hud_draw_minimap(next_map, NULL, 0);
        }
    }

    if (g_config.num_players <= 2) {
        /* 1-2 players: P1 on right panel (slot 1), P2 on left (slot 0) */
        /* Single player: only right panel */
        if (g_config.num_players >= 1) {
            draw_player_shop(&g_players[0], panel_xbase[1],
                             cursor_pos[0], player_done[0],
                             INFO_P1_X, INFO_NAME_P1_X);
        }
        if (g_config.num_players >= 2) {
            draw_player_shop(&g_players[1], panel_xbase[0],
                             cursor_pos[1], player_done[1],
                             INFO_P2_X, INFO_NAME_P2_X);
        }
    } else {
        /* 3-4 players: show current page (2 players at a time) */
        int start = shop_page * 2;
        /* First player in pair → right panel (side 1) */
        if (start < g_config.num_players) {
            draw_player_shop(&g_players[start], panel_xbase[1],
                             cursor_pos[start], player_done[start],
                             INFO_P1_X, INFO_NAME_P1_X);
        }
        /* Second player in pair → left panel (side 2) */
        if (start + 1 < g_config.num_players) {
            draw_player_shop(&g_players[start + 1], panel_xbase[0],
                             cursor_pos[start + 1], player_done[start + 1],
                             INFO_P2_X, INFO_NAME_P2_X);
        }
    }
}

void shop_cleanup(void)
{
    UnloadTexture(bg_tex);
    UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;
    UnloadFON(&shop_font);
    next_map = NULL;  /* points into the caller's Round — don't keep it */
}
