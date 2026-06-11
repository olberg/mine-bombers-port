#include "unity.h"
#include "game/hud.h"
#include "game/config.h"

void setUp(void) {}
void tearDown(void) {}

/* Verify the 4 panel X positions match original decompiled values */
void test_panel_positions(void)
{
    TEST_ASSERT_EQUAL_INT(12,  hud_panel_x(0));
    TEST_ASSERT_EQUAL_INT(174, hud_panel_x(1));
    TEST_ASSERT_EQUAL_INT(337, hud_panel_x(2));
    TEST_ASSERT_EQUAL_INT(500, hud_panel_x(3));
}

/* Verify health bar constants match decompiled FUN_1010_6150 (seg_1010:3419-3434).
 * Original: vertical bar at panel_x+130, Y=2, 7px wide, 25px max height. */
void test_health_bar_constants(void)
{
    /* Verify bar position offset from panel base */
    TEST_ASSERT_EQUAL_INT(130, HUD_HEALTH_BAR_X);
    TEST_ASSERT_EQUAL_INT(2,   HUD_HEALTH_BAR_Y);
    TEST_ASSERT_EQUAL_INT(7,   HUD_HEALTH_BAR_W);
    TEST_ASSERT_EQUAL_INT(25,  HUD_HEALTH_BAR_H);

    /* Verify panel dimensions are reasonable */
    TEST_ASSERT_EQUAL_INT(150, HUD_PANEL_WIDTH);
    TEST_ASSERT_EQUAL_INT(30, HUD_PANEL_HEIGHT);

    /* Health bar Y + height fits within panel */
    TEST_ASSERT_TRUE(HUD_HEALTH_BAR_Y + HUD_HEALTH_BAR_H <= HUD_PANEL_HEIGHT);

    /* Y=11 = digging power, Y=21 = money
     * (earned + wallet); both at panel_x + 50 */
    TEST_ASSERT_EQUAL_INT(50, HUD_DIG_X);
    TEST_ASSERT_EQUAL_INT(11, HUD_DIG_Y);
    TEST_ASSERT_EQUAL_INT(50, HUD_MONEY_X);
    TEST_ASSERT_EQUAL_INT(21, HUD_MONEY_Y);
}

/* Verify minimap position constants match decompiled FUN_1010_b227 call */
void test_minimap_constants(void)
{
    TEST_ASSERT_EQUAL_INT(288, MINIMAP_X); /* 0x120 */
    TEST_ASSERT_EQUAL_INT(51, MINIMAP_Y);  /* 0x33 */

    /* Minimap fits within 640x480 screen:
     * X: 288 + MAP_COLS(45) = 333 < 640
     * Y: 51 + MAP_ROWS(64) = 115 < 480 */
    TEST_ASSERT_TRUE(MINIMAP_X + MAP_COLS <= 640);
    TEST_ASSERT_TRUE(MINIMAP_Y + MAP_ROWS <= 480);
}

/* Verify minimap tile-to-color mapping matches FUN_1010_dab7.
 * The function is static in hud.c so we replicate its logic here for testing. */
static uint8_t test_tile_color(uint8_t tile)
{
    /* Reproduce the exact branching from FUN_1010_dab7 (seg_1010:8150-8199) */
    if (tile >= 0x32 && tile <= 0x34) return 12;
    if ((tile >= 0x37 && tile <= 0x39) || (tile >= 0x41 && tile <= 0x46)) return 9;
    if (tile == 0x73 || (tile > 0x91 && tile < 0x9B)) return 5;
    if (tile == 0x30 || tile == 0x66 || tile == 0xAF) return 14;
    if (tile >= 0x35 && tile <= 0x36) return 12;
    if (tile == 0x31) return 8;
    if (tile == 0xA4 || tile == 0x70 || tile == 0x71) return 9;
    if (tile == 0x65) return 14;
    if (tile == 0x79) return 12;
    if (tile == 0x9C) return 12;
    if (tile == 0x6F) return 4;
    return 12;
}

void test_minimap_tile_colors(void)
{
    /* Floor tiles → bright (14) */
    TEST_ASSERT_EQUAL_UINT8(14, test_tile_color(0x30)); /* '0' empty */
    TEST_ASSERT_EQUAL_UINT8(14, test_tile_color(0x66)); /* 'f' floor */
    TEST_ASSERT_EQUAL_UINT8(14, test_tile_color(0xAF)); /* floor alt */
    TEST_ASSERT_EQUAL_UINT8(14, test_tile_color(0x65)); /* 'e' explosive */

    /* Indestructible walls → dark gray (8 for '1', 12 for '2'-'4') */
    TEST_ASSERT_EQUAL_UINT8(8,  test_tile_color(0x31)); /* '1' */
    TEST_ASSERT_EQUAL_UINT8(12, test_tile_color(0x32)); /* '2' */
    TEST_ASSERT_EQUAL_UINT8(12, test_tile_color(0x34)); /* '4' */

    /* Destructible walls → medium (9) */
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0x37)); /* '7' */
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0x39)); /* '9' */
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0x41)); /* 'A' */
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0x46)); /* 'F' */

    /* Damaged walls → dark (12) */
    TEST_ASSERT_EQUAL_UINT8(12, test_tile_color(0x35)); /* '5' */
    TEST_ASSERT_EQUAL_UINT8(12, test_tile_color(0x36)); /* '6' */

    /* Treasure → bright (5) */
    TEST_ASSERT_EQUAL_UINT8(5, test_tile_color(0x73)); /* 's' */
    TEST_ASSERT_EQUAL_UINT8(5, test_tile_color(0x92)); /* treasure range */
    TEST_ASSERT_EQUAL_UINT8(5, test_tile_color(0x9A)); /* treasure range end */

    /* Proximity mine → red (4) */
    TEST_ASSERT_EQUAL_UINT8(4, test_tile_color(0x6F)); /* 'o' */

    /* Mystery box → dark (12) */
    TEST_ASSERT_EQUAL_UINT8(12, test_tile_color(0x79)); /* 'y' */

    /* Teleporter → dark (12) */
    TEST_ASSERT_EQUAL_UINT8(12, test_tile_color(0x9C));

    /* Special tiles → medium (9) */
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0xA4));
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0x70)); /* 'p' */
    TEST_ASSERT_EQUAL_UINT8(9, test_tile_color(0x71)); /* 'q' */
}

/* Verify timer bar constants match decompiled seg_1000:7278-7289.
 * fill_rect(0x1dd, 0x27d, 0x1d9, 0x27d - fill)
 *   = fill_rect(Y_bottom=477, X_right=637, Y_top=473, X_left=637-fill) */
void test_timer_bar_constants(void)
{
    TEST_ASSERT_EQUAL_INT(473, HUD_TIMER_Y_TOP);
    TEST_ASSERT_EQUAL_INT(477, HUD_TIMER_Y_BOTTOM);
    TEST_ASSERT_EQUAL_INT(637, HUD_TIMER_X_RIGHT);
    /* Inclusive fill_rect coords: rows 473..477 = 5 px tall, backdrop
     * X 2..637 = 636 px (DOSBox capture pixel evidence). */
    TEST_ASSERT_EQUAL_INT(5,   HUD_TIMER_H);
    TEST_ASSERT_EQUAL_INT(636, HUD_TIMER_BACK_W);
    /* SIKA.SPY palette: index 6 = (255,203,0) gold. DOSBox captures of the original
     * show the remaining-time bar in gold;
     * the decompiled round-start draw passes color 6, and the trailing 2
     * in fill_rect is the X-start coordinate, not a color. */
    TEST_ASSERT_EQUAL_INT(6,   HUD_TIMER_COLOR);

    /* The full bar background drawn at round start spans X = 2..637
     * (redraw_game_screen seg_1000:2937-2938), so max fill = 635 */
    TEST_ASSERT_EQUAL_INT(2,   HUD_TIMER_X_LEFT);
    TEST_ASSERT_EQUAL_INT(635, HUD_TIMER_MAX_W);

    /* Bar fits within 640x480 screen */
    TEST_ASSERT_TRUE(HUD_TIMER_X_RIGHT < 640);
    TEST_ASSERT_TRUE(HUD_TIMER_Y_BOTTOM <= 480);
}

/* Fill width = elapsed/total scaled onto the 635px background span.
 * The bar shows ELAPSED time, growing right-to-left from X=637. */
void test_timer_bar_fill_width(void)
{
    /* Round start: nothing elapsed */
    TEST_ASSERT_EQUAL_INT(0, hud_timer_fill_width(7662, 7662));

    /* Half elapsed */
    TEST_ASSERT_EQUAL_INT(317, hud_timer_fill_width(3831, 7662)); /* 635/2 trunc */

    /* Time up: full span */
    TEST_ASSERT_EQUAL_INT(635, hud_timer_fill_width(0, 7662));

    /* Clamps: negative remaining (expired) and over-total remaining */
    TEST_ASSERT_EQUAL_INT(635, hud_timer_fill_width(-100, 7662));
    TEST_ASSERT_EQUAL_INT(0, hud_timer_fill_width(9000, 7662));

    /* No time limit: no bar */
    TEST_ASSERT_EQUAL_INT(0, hud_timer_fill_width(0, 0));
    TEST_ASSERT_EQUAL_INT(0, hud_timer_fill_width(0, -1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_panel_positions);
    RUN_TEST(test_health_bar_constants);
    RUN_TEST(test_minimap_constants);
    RUN_TEST(test_minimap_tile_colors);
    RUN_TEST(test_timer_bar_constants);
    RUN_TEST(test_timer_bar_fill_width);
    return UNITY_END();
}
