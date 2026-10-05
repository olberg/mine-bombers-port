#include "unity.h"
#include "util/prng.h"
#include "game/shop.h"
#include "game/player.h"
#include "game/weapons.h"
#include "game/config.h"
#include "input/input.h"
#include "util/harness_env.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdint.h>

static bool shop_screen_open;

void setUp(void) {}

void tearDown(void)
{
    /* A failed assertion longjmps out of the test body, so the shop-screen
     * tests rely on this to release the window and the injection mode. */
    if (shop_screen_open) {
        shop_cleanup();
        CloseWindow();
        shop_screen_open = false;
    }
    player_input_inject_mode(false);
    g_config.option_toggle[1] = 0;
    g_config.option_toggle[2] = 0;
}

/* Buy weapon: verify cash deducted and inventory incremented */
void test_buy_deducts_cash(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.cash = 1000;

    /* Buy a small bomb (price=1) */
    int idx = weapon_inv_index(WEAPON_SMALL_BOMB);
    int old_qty = p.weapons[idx];
    int32_t old_cash = p.cash;

    bool ok = shop_buy_weapon(&p, WEAPON_SMALL_BOMB);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(old_qty + 1, p.weapons[idx]);

    const WeaponDef *wd = weapon_get_def(WEAPON_SMALL_BOMB);
    TEST_ASSERT_EQUAL_INT32(old_cash - wd->price, p.cash);
}

/* Buy with insufficient funds: verify purchase blocked */
void test_buy_insufficient_funds(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.cash = 0;

    int idx = weapon_inv_index(WEAPON_SMALL_BOMB);
    int old_qty = p.weapons[idx];

    bool ok = shop_buy_weapon(&p, WEAPON_SMALL_BOMB);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(old_qty, p.weapons[idx]);
    TEST_ASSERT_EQUAL_INT32(0, p.cash);
}

/* Sell weapon: verify refund = price / 2 (integer division) */
void test_sell_refunds_half_price(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.cash = 100;

    /* Give player some medium bombs (price=3, sell=1) */
    int idx = weapon_inv_index(WEAPON_MEDIUM_BOMB);
    p.weapons[idx] = 5;

    int32_t old_cash = p.cash;
    bool ok = shop_sell_weapon(&p, WEAPON_MEDIUM_BOMB);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(4, p.weapons[idx]);

    /* Refund = price / 2 (integer division: 3/2 = 1) */
    const WeaponDef *wd = weapon_get_def(WEAPON_MEDIUM_BOMB);
    int16_t refund = wd->price / 2;
    TEST_ASSERT_EQUAL_INT32(old_cash + refund, p.cash);
}

/* Sell empty slot: verify can't sell what you don't have */
void test_sell_empty_slot(void)
{
    Player p;
    player_init_defaults(&p, 0);
    p.cash = 100;

    /* Ensure no mega bombs */
    int idx = weapon_inv_index(WEAPON_MEGA_BOMB);
    p.weapons[idx] = 0;

    int32_t old_cash = p.cash;
    bool ok = shop_sell_weapon(&p, WEAPON_MEGA_BOMB);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(0, p.weapons[idx]);
    TEST_ASSERT_EQUAL_INT32(old_cash, p.cash);
}

/* Verify prices match MB.EXE originals */
void test_prices_match_original(void)
{
    const WeaponDef *wd;

    wd = weapon_get_def(0x57); /* Small bomb */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(1, wd->price);

    wd = weapon_get_def(0x58); /* Medium bomb */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(3, wd->price);

    wd = weapon_get_def(0x59); /* Large bomb */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(10, wd->price);

    wd = weapon_get_def(0x9D); /* Mine */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(650, wd->price);

    wd = weapon_get_def(0x5A); /* Rocket */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(500, wd->price);

    wd = weapon_get_def(0x7F); /* Mega bomb */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(80, wd->price);

    wd = weapon_get_def(0x80); /* Cluster bomb */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(145, wd->price);

    wd = weapon_get_def(0xB0); /* Money bomb */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(575, wd->price);

    wd = weapon_get_def(0xA5); /* Arrow */
    TEST_ASSERT_NOT_NULL(wd);
    TEST_ASSERT_EQUAL_INT(300, wd->price);
}

/* Free market OFF: active prices equal base prices */
void test_free_market_off_prices_equal_base(void)
{
    g_config.option_toggle[1] = 0;  /* free market OFF */
    shop_compute_prices();

    /* Small bomb base price = 1 */
    TEST_ASSERT_EQUAL_INT(1, shop_get_active_price(0));
    /* Mine base price = 650 */
    TEST_ASSERT_EQUAL_INT(650, shop_get_active_price(3));
    /* Money bomb base price = 575 */
    TEST_ASSERT_EQUAL_INT(575, shop_get_active_price(26));
}

/* Free market ON: active prices differ from base prices (statistical) */
void test_free_market_on_prices_randomized(void)
{
    g_config.option_toggle[1] = 1;  /* free market ON */

    /* Run multiple times to verify at least some prices differ from base.
     * With random(60), multiplier range is [0.5, 1.48] — prices change
     * unless random(60) lands exactly on 30 (multiplier = 1.0). */
    int different_count = 0;
    for (int trial = 0; trial < 100; trial++) {
        mb_prng_set_seed((uint32_t)(trial * 17 + 42));
        shop_compute_prices();

        /* Check if mine price (base 650) differs from 650 */
        int16_t mine_price = shop_get_active_price(3);
        if (mine_price != 650) {
            different_count++;
        }

        /* All prices must be >= 1 (the +1 minimum) */
        for (int i = 0; i < 27; i++) {
            TEST_ASSERT_TRUE(shop_get_active_price(i) >= 1);
        }
    }

    /* At least 90% of trials should have different prices */
    TEST_ASSERT_TRUE(different_count > 90);

    /* Restore default */
    g_config.option_toggle[1] = 0;
}

/* Free market ON: all prices scale by same factor */
void test_free_market_uniform_scaling(void)
{
    g_config.option_toggle[1] = 1;
    mb_prng_set_seed(12345u);
    shop_compute_prices();

    /* If both prices are > 1 (after +1 floor), the ratio should be
     * approximately the same as base ratio.
     * Small bomb base=1, Mine base=650.
     * With uniform multiplier m: prices are m*1+1 and m*650+1.
     * Check that mine price is roughly 650x the small bomb price
     * (with +1 offset making this imprecise for very small bases). */
    int16_t small_price = shop_get_active_price(0);
    int16_t mine_price = shop_get_active_price(3);

    /* Both must be positive */
    TEST_ASSERT_TRUE(small_price >= 1);
    TEST_ASSERT_TRUE(mine_price >= 1);

    /* Mine should be significantly more expensive than small bomb */
    TEST_ASSERT_TRUE(mine_price > small_price * 10);

    g_config.option_toggle[1] = 0;
}

/* ---- Shop paging & flow regression pins ----
 * Original calls the shop screen once per player pair (seg_1000:7103-7119):
 * 1P → one single-panel call, 2P → one dual call, 3P → dual (P1/P2) +
 * single (P3), 4P → dual + dual. Each call rerolls free-market prices
 * (FUN_1010_a2de at seg_1010:7004). */

void test_page_count_per_player_count(void)
{
    TEST_ASSERT_EQUAL_INT(1, shop_page_count(1));
    TEST_ASSERT_EQUAL_INT(1, shop_page_count(2));
    TEST_ASSERT_EQUAL_INT(2, shop_page_count(3));
    TEST_ASSERT_EQUAL_INT(2, shop_page_count(4));
}

void test_page_membership(void)
{
    int first, last;

    shop_page_range(1, 0, &first, &last);   /* 1P: page 0 = {P1} */
    TEST_ASSERT_EQUAL_INT(0, first);
    TEST_ASSERT_EQUAL_INT(1, last);

    shop_page_range(2, 0, &first, &last);   /* 2P: page 0 = {P1,P2} */
    TEST_ASSERT_EQUAL_INT(0, first);
    TEST_ASSERT_EQUAL_INT(2, last);

    shop_page_range(3, 0, &first, &last);   /* 3P: page 0 = {P1,P2} */
    TEST_ASSERT_EQUAL_INT(0, first);
    TEST_ASSERT_EQUAL_INT(2, last);
    shop_page_range(3, 1, &first, &last);   /* 3P: page 1 = {P3} only */
    TEST_ASSERT_EQUAL_INT(2, first);
    TEST_ASSERT_EQUAL_INT(3, last);

    shop_page_range(4, 1, &first, &last);   /* 4P: page 1 = {P3,P4} */
    TEST_ASSERT_EQUAL_INT(2, first);
    TEST_ASSERT_EQUAL_INT(4, last);
}

void test_page_panel_layout(void)
{
    /* Mirrors the original's param_1 per call: 0 = single panel,
     * 1 = dual (seg_1000:7104/7107/7110-7112/7117-7119). */
    TEST_ASSERT_FALSE(shop_page_is_dual(1, 0));  /* 1P: single */
    TEST_ASSERT_TRUE(shop_page_is_dual(2, 0));   /* 2P: dual */
    TEST_ASSERT_TRUE(shop_page_is_dual(3, 0));   /* 3P page 0: dual */
    TEST_ASSERT_FALSE(shop_page_is_dual(3, 1));  /* 3P page 1: single */
    TEST_ASSERT_TRUE(shop_page_is_dual(4, 0));   /* 4P: dual + dual */
    TEST_ASSERT_TRUE(shop_page_is_dual(4, 1));
}

void test_selling_gate(void)
{
    /* SP forces selling ON regardless of the option; MP follows
     * option_toggle[2]. */
    TEST_ASSERT_TRUE(shop_selling_enabled_for(1, 0));
    TEST_ASSERT_TRUE(shop_selling_enabled_for(1, 1));
    TEST_ASSERT_FALSE(shop_selling_enabled_for(2, 0));
    TEST_ASSERT_TRUE(shop_selling_enabled_for(2, 1));
    TEST_ASSERT_FALSE(shop_selling_enabled_for(4, 0));
    TEST_ASSERT_TRUE(shop_selling_enabled_for(4, 1));
}

void test_free_market_rerolls_per_page(void)
{
    /* The page-advance path calls shop_compute_prices again (one reroll
     * per shop call in the original). Under free market, consecutive
     * rerolls must draw a fresh multiplier — page 1 prices differ from
     * page 0's. */
    g_config.option_toggle[1] = 1;
    mb_prng_set_seed(777u);

    shop_compute_prices();
    int16_t page0[27];
    for (int i = 0; i < 27; i++) page0[i] = shop_get_active_price(i);

    shop_compute_prices();  /* page advance */
    int diffs = 0;
    for (int i = 0; i < 27; i++) {
        if (shop_get_active_price(i) != page0[i]) diffs++;
    }
    TEST_ASSERT_TRUE(diffs > 0);

    g_config.option_toggle[1] = 0;
}

/* ---- Shop auto-repeat / tap behavior, driven through shop_update ----
 * shop_update needs a GL context (SHOPPIC texture, palette upload), so these
 * open a 1x1 window like test_sprites does, then drive player 0 purely
 * through input injection. Buy/sell effects are read from the inventory. */

static bool open_shop(int players, int sell_option)
{
    InitWindow(1, 1, "test");
    harness_env_apply_monitor();
    if (!FileExists("assets/SHOPPIC.SPY")) {
        CloseWindow();
        return false;
    }
    shop_screen_open = true;

    player_input_init_defaults();
    player_input_inject_mode(true);
    g_config.num_players = (uint8_t)players;
    g_config.option_toggle[1] = 0;            /* fixed prices */
    g_config.option_toggle[2] = (uint8_t)sell_option;
    for (int i = 0; i < 4; i++) {
        player_init_defaults(&g_players[i], i);
        g_players[i].cash = 10000;
    }

    shop_init();
    /* Run out the fade-in so the shop is accepting input */
    for (int i = 0; i < 30; i++) shop_update();
    return true;
}

/* One frame with player 0's BOMB / CYCLE / RIGHT injected. */
static void frame(bool bomb_down, bool bomb_press,
                  bool cycle_down, bool cycle_press, bool right_press)
{
    player_input_inject_clear(0);
    player_input_inject(0, PLAYER_INPUT_BOMB, bomb_down, bomb_press);
    player_input_inject(0, PLAYER_INPUT_CYCLE, cycle_down, cycle_press);
    player_input_inject(0, PLAYER_INPUT_RIGHT, right_press, right_press);
    ShopResult r = shop_update();
    TEST_ASSERT_EQUAL_INT(SHOP_ACTIVE, r);
}

static int small_qty(void)  { return g_players[0].weapons[weapon_inv_index(WEAPON_SMALL_BOMB)]; }
static int medium_qty(void) { return g_players[0].weapons[weapon_inv_index(WEAPON_MEDIUM_BOMB)]; }

/* Every discrete tap on the same cell buys: the delay counter left over from
 * the previous fire must not swallow the next press. */
void test_shop_separate_taps_each_buy(void)
{
    if (!open_shop(1, 0)) TEST_IGNORE_MESSAGE("SHOPPIC.SPY not available in assets/");

    int before = small_qty();
    for (int tap = 0; tap < 5; tap++) {
        frame(true, true, false, false, false);   /* press */
        frame(false, false, false, false, false); /* release */
    }
    TEST_ASSERT_EQUAL_INT(before + 5, small_qty());
}

/* A held key follows the accelerating ramp: first buy on the press edge,
 * then at interval 0x14 - speed/3 (20, 13, 9, 6, then 4 at the speed cap) —
 * never once per frame. */
void test_shop_held_key_follows_ramp(void)
{
    if (!open_shop(1, 0)) TEST_IGNORE_MESSAGE("SHOPPIC.SPY not available in assets/");

    static const int expected[] = {1, 21, 34, 43, 49, 53, 57};
    int fires[16];
    int nfires = 0;
    int last = small_qty();

    for (int f = 1; f <= 60; f++) {
        frame(true, f == 1, false, false, false);
        int q = small_qty();
        TEST_ASSERT_TRUE(q - last <= 1);
        if (q != last && nfires < 16) fires[nfires++] = f;
        last = q;
    }

    TEST_ASSERT_EQUAL_INT(7, nfires);
    TEST_ASSERT_EQUAL_INT_ARRAY(expected, fires, 7);
}

/* Moving the cursor resets speed/delay (decompiled 6586-6589): the new cell
 * fires on the move frame and the next repeat is a full 20 frames later,
 * not at the shortened interval the ramp had reached. */
void test_shop_cursor_move_resets_ramp(void)
{
    if (!open_shop(1, 0)) TEST_IGNORE_MESSAGE("SHOPPIC.SPY not available in assets/");

    /* Hold BOMB on cell 1 for 25 frames: buys at frames 1 and 21 */
    int small_before = small_qty();
    for (int f = 1; f <= 25; f++) frame(true, f == 1, false, false, false);
    int small_after_ramp = small_qty();
    TEST_ASSERT_EQUAL_INT(small_before + 2, small_after_ramp);
    int medium_before = medium_qty();

    /* Frame 26: step RIGHT to cell 2 while BOMB stays held */
    frame(true, false, false, false, true);
    TEST_ASSERT_EQUAL_INT(small_after_ramp, small_qty());
    TEST_ASSERT_EQUAL_INT(medium_before + 1, medium_qty());

    /* Frames 27..45: ramp restarted at interval 20, so nothing fires */
    for (int f = 27; f <= 45; f++) {
        frame(true, false, false, false, false);
        TEST_ASSERT_EQUAL_INT(medium_before + 1, medium_qty());
    }
    /* Frame 46: next repeat */
    frame(true, false, false, false, false);
    TEST_ASSERT_EQUAL_INT(medium_before + 2, medium_qty());
    TEST_ASSERT_EQUAL_INT(small_after_ramp, small_qty());
}

/* The CYCLE (sell) key gets the same tap behavior when selling is enabled
 * (single-player forces it on). */
void test_shop_separate_taps_each_sell(void)
{
    if (!open_shop(1, 0)) TEST_IGNORE_MESSAGE("SHOPPIC.SPY not available in assets/");

    frame(false, false, false, false, true);   /* cursor to cell 2 (medium bomb) */
    g_players[0].weapons[weapon_inv_index(WEAPON_MEDIUM_BOMB)] = 10;
    int32_t cash = g_players[0].cash;

    for (int tap = 0; tap < 4; tap++) {
        frame(false, false, true, true, false);
        frame(false, false, false, false, false);
    }
    TEST_ASSERT_EQUAL_INT(6, medium_qty());
    TEST_ASSERT_TRUE(g_players[0].cash > cash);
}

/* With selling disabled (2+ players, option_toggle[2] off) the CYCLE key does
 * nothing in the shop, tap or hold. */
void test_shop_sell_disabled_ignores_cycle(void)
{
    if (!open_shop(2, 0)) TEST_IGNORE_MESSAGE("SHOPPIC.SPY not available in assets/");

    frame(false, false, false, false, true);   /* cursor to cell 2 */
    g_players[0].weapons[weapon_inv_index(WEAPON_MEDIUM_BOMB)] = 10;

    for (int tap = 0; tap < 4; tap++) {
        frame(false, false, true, true, false);
        frame(false, false, false, false, false);
    }
    for (int f = 0; f < 30; f++) frame(false, false, true, false, false);
    TEST_ASSERT_EQUAL_INT(10, medium_qty());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_buy_deducts_cash);
    RUN_TEST(test_buy_insufficient_funds);
    RUN_TEST(test_sell_refunds_half_price);
    RUN_TEST(test_sell_empty_slot);
    RUN_TEST(test_prices_match_original);
    RUN_TEST(test_free_market_off_prices_equal_base);
    RUN_TEST(test_free_market_on_prices_randomized);
    RUN_TEST(test_free_market_uniform_scaling);
    RUN_TEST(test_page_count_per_player_count);
    RUN_TEST(test_page_membership);
    RUN_TEST(test_page_panel_layout);
    RUN_TEST(test_selling_gate);
    RUN_TEST(test_free_market_rerolls_per_page);
    RUN_TEST(test_shop_separate_taps_each_buy);
    RUN_TEST(test_shop_held_key_follows_ramp);
    RUN_TEST(test_shop_cursor_move_resets_ramp);
    RUN_TEST(test_shop_separate_taps_each_sell);
    RUN_TEST(test_shop_sell_disabled_ignores_cycle);
    return UNITY_END();
}
