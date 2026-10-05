#include "unity.h"
#include "game/hud.h"
#include "game/config.h"
#include "game/map_thumbnail.h"
#include "game/sprites.h"
#include "gfx/palette.h"
#include "util/harness_env.h"

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

/* Verify the shop thumbnail position matches the decompiled FUN_1010_b227 call */
void test_minimap_constants(void)
{
    TEST_ASSERT_EQUAL_INT(288, MAP_THUMBNAIL_SHOP_X); /* 0x120 */
    TEST_ASSERT_EQUAL_INT(51, MAP_THUMBNAIL_SHOP_Y);  /* 0x33 */

    /* It fits on the 640x480 screen: rows run along X, cols along Y */
    TEST_ASSERT_TRUE(MAP_THUMBNAIL_SHOP_X + MAP_ROWS <= 640);
    TEST_ASSERT_TRUE(MAP_THUMBNAIL_SHOP_Y + MAP_COLS <= 480);
}

/* Verify the thumbnail's tile-to-color mapping matches FUN_1010_dab7
 * (seg_1010:8150-8199). */
#define test_tile_color map_thumbnail_color

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

/* FUN_1010_6030: first char of the runtime name (the "N " prefix digit) is
 * blanked and at most 10 characters are shown. */
void test_name_line_blanks_prefix_and_truncates(void)
{
    char buf[32];
    hud_format_name(buf, sizeof(buf), "1 Plr 1");
    TEST_ASSERT_EQUAL_STRING("  Plr 1", buf);
    hud_format_name(buf, sizeof(buf), "3 Abcdefghijklmn");
    TEST_ASSERT_EQUAL_STRING("  Abcdefgh", buf);
    hud_format_name(buf, sizeof(buf), "");
    TEST_ASSERT_EQUAL_STRING("", buf);
    char small[4];
    hud_format_name(small, sizeof(small), "2 Bob");
    TEST_ASSERT_EQUAL_STRING("  B", small);
}

/* Names that never went through player select have no prefix to hide. */
void test_name_line_keeps_names_without_prefix(void)
{
    char buf[32];
    hud_format_name(buf, sizeof(buf), "BOT1");
    TEST_ASSERT_EQUAL_STRING("BOT1", buf);
    hud_format_name(buf, sizeof(buf), "Player 1");
    TEST_ASSERT_EQUAL_STRING("Player 1", buf);
    hud_format_name(buf, sizeof(buf), "5 Five");
    TEST_ASSERT_EQUAL_STRING("5 Five", buf);
}

static int count_color(const Image *img, int x0, int y0, int w, int h, Color c)
{
    const Color *px = (const Color *)img->data;
    int n = 0;
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            Color p = px[y * img->width + x];
            if (p.r == c.r && p.g == c.g && p.b == c.b) n++;
        }
    }
    return n;
}

/* draw_score_displays prints dig power in colour 3 and draw_points_displays
 * prints money in colour 5 (seg_1010:3438, 3495); neither uses the white of
 * the name and the ammo count. Rendered for real and read back. */
void test_dig_and_money_are_drawn_in_their_colors(void)
{
    InitWindow(1, 1, "test");
    harness_env_apply_monitor();
    if (!sprites_init()) {
        CloseWindow();
        TEST_IGNORE_MESSAGE("SIKA.SPY not available in assets/");
        return;
    }
    palette_init(sprites_get_palette());
    hud_init(1);

    Player p;
    player_init_defaults(&p, 0);
    p.cash = 250;

    RenderTexture2D target = LoadRenderTexture(640, 480);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    hud_draw(&p, 1, 0, true);
    EndTextureMode();
    Image frame = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&frame);
    ImageFormat(&frame, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    int x = hud_panel_x(0) + HUD_DIG_X;
    Color white = palette_get_color(HUD_TEXT_COLOR);
    int dig_own   = count_color(&frame, x, HUD_DIG_Y, 41, 9, palette_get_color(HUD_DIG_COLOR));
    int dig_white = count_color(&frame, x, HUD_DIG_Y, 41, 9, white);
    int money_own   = count_color(&frame, x, HUD_MONEY_Y, 41, 9, palette_get_color(HUD_MONEY_COLOR));
    int money_white = count_color(&frame, x, HUD_MONEY_Y, 41, 9, white);

    UnloadImage(frame);
    UnloadRenderTexture(target);
    hud_cleanup();
    sprites_cleanup();
    CloseWindow();

    TEST_ASSERT_GREATER_THAN_INT(0, dig_own);
    TEST_ASSERT_EQUAL_INT(0, dig_white);
    TEST_ASSERT_GREATER_THAN_INT(0, money_own);
    TEST_ASSERT_EQUAL_INT(0, money_white);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_name_line_blanks_prefix_and_truncates);
    RUN_TEST(test_name_line_keeps_names_without_prefix);
    RUN_TEST(test_dig_and_money_are_drawn_in_their_colors);
    RUN_TEST(test_panel_positions);
    RUN_TEST(test_health_bar_constants);
    RUN_TEST(test_minimap_constants);
    RUN_TEST(test_minimap_tile_colors);
    RUN_TEST(test_timer_bar_constants);
    RUN_TEST(test_timer_bar_fill_width);
    return UNITY_END();
}
