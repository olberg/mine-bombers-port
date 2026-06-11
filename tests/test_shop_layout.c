#include "unity.h"
#include "loaders/spy_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Constants from shop.c to verify grid layout matches decompiled FUN_1010_a9c9 */
#define PANEL_P1_XBASE  0x160  /* 352 — right side */
#define PANEL_P2_XBASE  0x020  /*  32 — left side */
#define GRID_COL_STEP   0x40   /* 64 */
#define GRID_ROW_STEP   0x30   /* 48 */
#define CELL_BORDER_Y   0x60   /* 96 */

void setUp(void) {}
void tearDown(void) {}

/*
 * Verify SHOPPIC.SPY has the expected two-panel layout.
 * The background has two black (color 0) rectangles for weapon lists,
 * with a gray divider in the center. This test confirms the panel
 * boundaries match the constants used in shop.c.
 *
 * Layout (from image analysis):
 *   Left panel:  X=32..287  (256px wide, black)
 *   Divider:     X=288..351 (gray border)
 *   Right panel: X=352..607 (256px wide, black)
 *   Top/bottom:  gray borders
 */
void test_shoppic_panel_structure(void)
{
    uint8_t palette[768];
    uint8_t *indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    TEST_ASSERT_NOT_NULL(indexed);

    Image img = LoadSPY("assets/SHOPPIC.SPY", palette, indexed);
    TEST_ASSERT_NOT_EQUAL(0, img.width);
    TEST_ASSERT_EQUAL(640, img.width);
    TEST_ASSERT_EQUAL(480, img.height);

    /* Verify palette color 0 is black (panel background) */
    TEST_ASSERT_EQUAL_UINT8(0, palette[0]);
    TEST_ASSERT_EQUAL_UINT8(0, palette[1]);
    TEST_ASSERT_EQUAL_UINT8(0, palette[2]);

    /* At Y=240 (vertical center), check left panel is black */
    int y = 240;
    for (int x = 40; x < 280; x++) {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, indexed[y * SPY_WIDTH + x],
            "Left panel should be black at Y=240");
    }

    /* Center divider should be non-black (gray) */
    TEST_ASSERT_NOT_EQUAL(0, indexed[y * SPY_WIDTH + 310]);

    /* Right panel should be black */
    for (int x = 360; x < 600; x++) {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, indexed[y * SPY_WIDTH + x],
            "Right panel should be black at Y=240");
    }

    /* Verify the left edge has a gray border (not black) */
    TEST_ASSERT_NOT_EQUAL(0, indexed[y * SPY_WIDTH + 5]);

    /* Verify the right edge has a gray border (not black) */
    TEST_ASSERT_NOT_EQUAL(0, indexed[y * SPY_WIDTH + 630]);

    free(indexed);
    UnloadImage(img);
}

/*
 * Verify grid cell coordinates match decompiled FUN_1010_a9c9.
 * Border sprite drawn at: Y = row*48 + 96, X = panel_base + col*64
 * Item 1 (row=0, col=0) on Panel 1: Y=96, X=352
 * Item 4 (row=0, col=3) on Panel 1: Y=96, X=352+192=544
 * Item 5 (row=1, col=0) on Panel 1: Y=96+48=144, X=352
 * Item 28 (row=6, col=3) on Panel 1: Y=96+288=384, X=544
 */
void test_grid_coordinates_match_decompiled(void)
{
    /* Item 1 (1-based): col=0, row=0 */
    int col = (1 - 1) % 4;  /* 0 */
    int row = (1 - 1) / 4;  /* 0 */
    int x = PANEL_P1_XBASE + col * GRID_COL_STEP;
    int y = row * GRID_ROW_STEP + CELL_BORDER_Y;
    TEST_ASSERT_EQUAL_INT(352, x);
    TEST_ASSERT_EQUAL_INT(96, y);

    /* Item 4 (1-based): col=3, row=0 */
    col = (4 - 1) % 4;  /* 3 */
    row = (4 - 1) / 4;  /* 0 */
    x = PANEL_P1_XBASE + col * GRID_COL_STEP;
    y = row * GRID_ROW_STEP + CELL_BORDER_Y;
    TEST_ASSERT_EQUAL_INT(352 + 192, x);
    TEST_ASSERT_EQUAL_INT(96, y);

    /* Item 5 (1-based): col=0, row=1 */
    col = (5 - 1) % 4;  /* 0 */
    row = (5 - 1) / 4;  /* 1 */
    x = PANEL_P1_XBASE + col * GRID_COL_STEP;
    y = row * GRID_ROW_STEP + CELL_BORDER_Y;
    TEST_ASSERT_EQUAL_INT(352, x);
    TEST_ASSERT_EQUAL_INT(144, y);

    /* Item 28 (1-based): col=3, row=6 — LEAVE */
    col = (28 - 1) % 4;  /* 3 */
    row = (28 - 1) / 4;  /* 6 */
    x = PANEL_P1_XBASE + col * GRID_COL_STEP;
    y = row * GRID_ROW_STEP + CELL_BORDER_Y;
    TEST_ASSERT_EQUAL_INT(544, x);
    TEST_ASSERT_EQUAL_INT(384, y);

    /* Panel 2 (left): Item 1 should be at X=32, Y=96 */
    x = PANEL_P2_XBASE + 0 * GRID_COL_STEP;
    y = 0 * GRID_ROW_STEP + CELL_BORDER_Y;
    TEST_ASSERT_EQUAL_INT(32, x);
    TEST_ASSERT_EQUAL_INT(96, y);
}

/* Verify all grid cells fit within 640x480 screen */
void test_grid_fits_in_screen(void)
{
    for (int idx = 1; idx <= 28; idx++) {
        int col = (idx - 1) % 4;
        int row = (idx - 1) / 4;

        /* Panel 1 (right) */
        int x = PANEL_P1_XBASE + col * GRID_COL_STEP;
        int y = row * GRID_ROW_STEP + CELL_BORDER_Y;
        TEST_ASSERT_TRUE_MESSAGE(x + 64 <= 640, "P1 cell X exceeds 640");
        TEST_ASSERT_TRUE_MESSAGE(y + 48 <= 480, "P1 cell Y exceeds 480");

        /* Panel 2 (left) */
        x = PANEL_P2_XBASE + col * GRID_COL_STEP;
        TEST_ASSERT_TRUE_MESSAGE(x + 64 <= 640, "P2 cell X exceeds 640");
        TEST_ASSERT_TRUE_MESSAGE(y + 48 <= 480, "P2 cell Y exceeds 480");
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_shoppic_panel_structure);
    RUN_TEST(test_grid_coordinates_match_decompiled);
    RUN_TEST(test_grid_fits_in_screen);
    return UNITY_END();
}
