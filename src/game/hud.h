#ifndef HUD_H
#define HUD_H

#include "game/player.h"
#include "game/map.h"
#include <stdbool.h>

/*
 * In-game HUD: per-player status panels at the top of the screen,
 * plus score display, timer bar, and optional minimap.
 *
 * Panel X positions (from decompiled draw_player_status_panels):
 *   Player 1: X=12, Player 2: X=174, Player 3: X=337, Player 4: X=500
 * Y=0 for all panels. Text color: palette index 1.
 */

#define HUD_PANEL_Y          0
#define HUD_PANEL_X_P1      12
#define HUD_PANEL_X_P2     174
#define HUD_PANEL_X_P3     337
#define HUD_PANEL_X_P4     500

/* Panel width covers the space between panels (approx 150 pixels) */
#define HUD_PANEL_WIDTH    150
#define HUD_PANEL_HEIGHT    30  /* top 30 pixels reserved for HUD */

/* Health bar: VERTICAL bar at panel_x + 130, Y=2.
 * Original FUN_1010_6150 (seg_1010:3371-3415):
 *   P1: X=0x8e (142), P2: X=0x130 (304), P3: X=0x1d3 (467), P4: X=0x276 (630)
 *   Y=2, width=7, max_height=25 (grows upward). */
#define HUD_HEALTH_BAR_X  130   /* offset from panel left */
#define HUD_HEALTH_BAR_Y    2   /* offset from panel top */
#define HUD_HEALTH_BAR_W    7   /* bar width */
#define HUD_HEALTH_BAR_H   25   /* bar max height */

/* Y=11 line: DIGGING POWER (dig + tool bonus), at panel_x + 50.
 * From the mislabeled "draw_score_displays" (seg_1010:3438-3491):
 *   P1: X=0x3e (62), P2: X=0xe0 (224), P3: X=0x183 (387), P4: X=0x226 (550).
 * (NOTE: there is no "cash at 11 / points at 21" — the manual:
 * "Digging power and your current cash are displayed under your name".) */
#define HUD_DIG_X          50   /* offset from panel left */
#define HUD_DIG_Y          11

/* Y=21 line: MONEY = wallet + this round's earnings, at panel_x + 50.
 * From the mislabeled "draw_points_displays" (seg_1010:3495-3547) */
#define HUD_MONEY_X        50   /* offset from panel left */
#define HUD_MONEY_Y        21

/* Text color palette indices */
#define HUD_TEXT_COLOR       1
#define HUD_TEXT_HIGHLIGHT   3

/* Lives display (single-player).
 * Decompiled FUN_1000_4667 (seg_1000:2874-2903): draws up to 3 life icons.
 * Full life: blit_sprite(page=0, spr, seg, Y=2, X=(i-1)*16+0x9F)
 * Empty life: blit_sprite(page=0, spr, seg, Y=16, X=(i-1)*16+0x9F)
 * Life sprites captured from SIKA.SPY:
 *   Full:  capture_screen_region(buf, Y_end=0x6F, X_end=0x2A, Y_start=0x5B, X_start=0x1F)
 *          → (31, 91) 12×21 pixels
 *   Empty: capture_screen_region(buf, Y_end=0x6F, X_end=0x3D, Y_start=0x69, X_start=0x2B)
 *          → (43, 105) 19×7 pixels */
#define HUD_LIVES_BASE_X   159   /* 0x9F */
#define HUD_LIVES_Y_FULL     2
#define HUD_LIVES_Y_EMPTY   16
#define HUD_LIVES_SPACING   16   /* 0x10 */
#define HUD_LIFE_FULL_SX    31   /* source X in SIKA.SPY */
#define HUD_LIFE_FULL_SY    91   /* source Y in SIKA.SPY */
#define HUD_LIFE_FULL_W     12   /* sprite width */
#define HUD_LIFE_FULL_H     21   /* sprite height */
#define HUD_LIFE_EMPTY_SX   43   /* source X in SIKA.SPY */
#define HUD_LIFE_EMPTY_SY  105   /* source Y in SIKA.SPY */
#define HUD_LIFE_EMPTY_W    19   /* sprite width */
#define HUD_LIFE_EMPTY_H     7   /* sprite height */

/* Initialize HUD resources (font, sprites). */
void hud_init(int num_players);

/* Draw HUD overlay. Call after map/sprites are drawn. */
void hud_draw(const Player players[], int num_players,
              int frame_counter, bool single_player);

/* Timer bar: thin horizontal bar at screen bottom-right, growing right-to-left.
 * Decompiled (seg_1000:7278-7289, redrawn every 20 frames, MP only):
 *   fill_rect(0x1dd, 0x27d, 0x1d9, 0x27d - fill)
 *   = fill_rect(Y_bottom=477, X_right=637, Y_top=473, X_left=637-fill)
 * The full-width bar background is drawn at round start spanning X=2..637
 * (redraw_game_screen, seg_1000:2937-2938: fill_rect(0x1dd,0x27d,0x1d9,2)).
 * Fill scale VERIFIED by DOSBox pixel measurement:
 *   fill = Trunc(elapsed_ticks * 635 / total_ticks)
 * (97 boundary samples across three time-limit configs fit exactly;
 * the 640 candidate is excluded by the config-16 run). */
#define HUD_TIMER_Y_TOP      473
#define HUD_TIMER_Y_BOTTOM   477
#define HUD_TIMER_X_RIGHT    637
#define HUD_TIMER_X_LEFT       2   /* bar background left edge */
/* fill_rect coordinates are INCLUSIVE on both ends (DOSBox
 * captures show gold on rows 473..477 = 5 rows, X 2..637 = 636 px). */
#define HUD_TIMER_H          (HUD_TIMER_Y_BOTTOM - HUD_TIMER_Y_TOP + 1)  /* 5 */
#define HUD_TIMER_BACK_W     (HUD_TIMER_X_RIGHT - HUD_TIMER_X_LEFT + 1)  /* 636 */
/* Gold span in the steady state (after the first periodic erase, whose
 * x_left is 637-fill even at fill 0): X 2..636 = 635 px. The fill scale
 * runs over this span. */
#define HUD_TIMER_MAX_W      (HUD_TIMER_X_RIGHT - HUD_TIMER_X_LEFT)      /* 635 */
#define HUD_TIMER_COLOR        6   /* palette index 6 = gold (255,203,0) in
                                    * SIKA.SPY — DOSBox captures show a gold
                                    * bar. The decompiled round-start
                                    * draw passes 6 to the color setter (the
                                    * set_draw_page/set_draw_color labels are
                                    * swapped); an earlier reading misread fill_rect's
                                    * trailing 2 (the X-start coord) as the
                                    * color. Periodic erase color is 0. */

/* Pure helper: elapsed-time fill width in pixels (0..HUD_TIMER_MAX_W).
 * Exposed for the geometry pin tests. */
int hud_timer_fill_width(int time_remaining, int time_total);

/* Draw the timer bar (call in MP only). Remaining time = left-anchored
 * gold bar (HUD_TIMER_COLOR); elapsed erased in black from the right.
 * With no time limit (total <= 0) the bar stays full. */
void hud_draw_timer(int time_remaining, int time_total);

/* Clean up HUD resources. */
void hud_cleanup(void);

/* Get panel X position for a player index (0-3). */
int hud_panel_x(int player_idx);

/* Minimap: scaled-down overview of the tile map (1 pixel per tile).
 * Position from decompiled FUN_1010_b227 call: base_x=0x120 (288), base_y=0x33 (51).
 * Tile-to-color mapping from FUN_1010_dab7 (seg_1010:8150-8199). */
#define MINIMAP_X  288   /* 0x120 */
#define MINIMAP_Y   51   /* 0x33 */

/* Draw the minimap overlay. Shows each tile as a single colored pixel.
 * Also marks player positions. No camera offset — fixed viewport. */
void hud_draw_minimap(const TileMap *map, const Player players[],
                      int num_players);

#endif
