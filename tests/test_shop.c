#include "unity.h"
#include "util/prng.h"
#include "game/shop.h"
#include "game/player.h"
#include "game/weapons.h"
#include "game/config.h"
#include <stdlib.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

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
    return UNITY_END();
}
