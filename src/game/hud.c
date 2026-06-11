#include "game/hud.h"
#include "game/weapons.h"
#include "game/sprites.h"
#include "game/map.h"
#include "game/map_renderer.h"
#include "loaders/spy_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Panel X positions indexed by player (0-3) */
static const int panel_x[MAX_PLAYERS] = {
    HUD_PANEL_X_P1, HUD_PANEL_X_P2, HUD_PANEL_X_P3, HUD_PANEL_X_P4
};

static BitmapFont hud_font;
static int hud_num_players;

/* HUD chrome background — top 30 pixels of PLAYERS.SPY contain per-panel
 * portrait, gray-frame weapon slot, and the per-player colored "HEALTH"
 * strip. Original re-blits this every frame (seg_1000:2917
 * load_and_display_image with "players.spy"). The rest of the 640x480
 * image is empty in-game, so we only ever source the top strip. */
static Image     hud_bg_img;
static Texture2D hud_bg_tex;
static bool      hud_bg_loaded;

int hud_panel_x(int player_idx)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS) return 0;
    return panel_x[player_idx];
}

void hud_init(int num_players)
{
    hud_font = LoadFON("assets/FONTTI.FON", true);
    hud_num_players = num_players;

    /* Load PLAYERS.SPY with its own palette baked to RGBA. The in-game
     * palette is compatible (all SPY files share index semantics), so the
     * baked colors render correctly on top of the current framebuffer. */
    uint8_t palette[768];
    uint8_t *indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    if (indexed) {
        hud_bg_img = LoadSPY("assets/PLAYERS.SPY", palette, indexed);
        if (hud_bg_img.width > 0) {
            hud_bg_tex = LoadTextureFromImage(hud_bg_img);
            hud_bg_loaded = true;
        }
        free(indexed);
    }
}

/*
 * Map a weapon tile id to the (x, y) of its 30×30 sprite in SIKA.SPY.
 * Extracted from draw_player_status_panels (seg_1010:3554-3667), which
 * switches on the player's selected-weapon tile byte (offset 0xF2) and
 * blits one of the shop-size sprites (_DAT_1038_05f0..0658). Returns
 * false if the weapon has no HUD sprite (original's switch falls through
 * and draws nothing).
 */
typedef struct { int x, y; } HudWeaponSprite;

static bool weapon_hud_sprite(uint8_t tile, HudWeaponSprite *out)
{
    int x, y;
    switch (tile) {
    case 'W':  x =   0; y = 170; break;  /* small bomb */
    case 'X':  x =  30; y = 170; break;  /* medium bomb */
    case 'Y':  x =  60; y = 170; break;  /* large bomb */
    case 'Z':  x =  90; y = 170; break;  /* rocket */
    case 0x7F: x = 120; y = 170; break;  /* mega bomb */
    case 0x80: x = 150; y = 170; break;  /* cluster bomb */
    case 0x81: x = 180; y = 170; break;  /* super bomb */
    case 'c': case 0x82: case 'g': case 'i':  /* small signature bomb variants */
               x = 240; y = 170; break;
    case 'd': case 0x83: case 'h': case 'j':  /* large signature bomb variants */
               x = 210; y = 170; break;
    case 'e':  x = 270; y = 170; break;  /* explosive */
    case 0x8A: x =  90; y = 140; break;  /* napalm */
    case 0x9D: x = 216; y = 140; break;  /* mine */
    case 0xA1: x = 276; y = 140; break;  /* timer bomb */
    case 0xA2: x = 276; y = 110; break;  /* remote bomb */
    case 0xA4: x = 246; y = 110; break;  /* power bomb */
    case 0xA5: x = 246; y = 140; break;  /* arrow */
    case 0xA9: x = 216; y = 110; break;  /* tele bomb */
    case 0x9C: x =  30; y =  40; break;  /* warp bomb */
    case 'n':  x = 232; y =  80; break;  /* creature spawner */
    case 'o':  x = 262; y =  80; break;  /* proximity mine */
    case 'r':  x =   0; y =  40; break;  /* rocket launcher */
    case 't':  x = 105; y =  40; break;  /* steel plate icon */
    case 0xAB: x =  60; y =  40; break;  /* random bomb */
    case 0xB0: x =   0; y =  90; break;  /* money bomb */
    default: return false;
    }
    out->x = x;
    out->y = y;
    return true;
}

#define HUD_WEAPON_SPRITE_SIZE 30

/* Health bar: overlay black on the damaged (top) portion.
 *
 * PLAYERS.SPY's per-player "HEALTH" strip is already rendered as a full
 * colored bar (P1 blue, P2 red, P3 green, P4 yellow) by the HUD chrome
 * draw. The original FUN_1010_6150 (seg_1010:3371-3415) computes a
 * damaged-height rectangle and fills it with color 2 (dark) to erase
 * the top of the strip down to the current health line; the remaining
 * lower portion of the colored strip stays visible as "current health".
 *
 * We mirror that by only drawing the damaged overlay. Geometry is
 * unchanged from the original: X=panel_x+130, Y=2, W=7, max H=25.
 */
static void draw_health_bar(int px, int py, int health, int max_health)
{
    if (max_health <= 0) return;

    int fill = health;
    if (fill < 0) fill = 0;
    if (fill > max_health) fill = max_health;

    int fill_h = (fill * HUD_HEALTH_BAR_H) / max_health;
    int damaged_h = HUD_HEALTH_BAR_H - fill_h;
    if (damaged_h <= 0) return;

    int bar_x = px + HUD_HEALTH_BAR_X;
    int bar_y = py + HUD_HEALTH_BAR_Y;
    DrawRectangle(bar_x, bar_y, HUD_HEALTH_BAR_W, damaged_h, BLACK);
}

static void draw_player_panel(const Player *p, int px, int py)
{
    char buf[32];
    Color text_col = palette_get_color(HUD_TEXT_COLOR);

    /* Selected-weapon 30×30 sprite at (panel_x, 0).
     * Source: draw_player_status_panels (seg_1010:3554). The original
     * switches on the weapon tile and blits _DAT_1038_05f0..0658 (shop
     * sprite set). Tiles not in the switch draw nothing. */
    HudWeaponSprite ws;
    if (weapon_hud_sprite(p->selected_weapon, &ws)) {
        Texture2D atlas = sprites_get_atlas();
        Rectangle src = { (float)ws.x, (float)ws.y,
                          HUD_WEAPON_SPRITE_SIZE, HUD_WEAPON_SPRITE_SIZE };
        Rectangle dst = { (float)px, (float)py,
                          HUD_WEAPON_SPRITE_SIZE, HUD_WEAPON_SPRITE_SIZE };
        DrawTexturePro(atlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }

    /* Ammo count at (panel_x, 0), overlaying weapon sprite's top-left.
     * draw_game_hud (seg_1010:3674+) prints the inventory count for the
     * currently selected weapon — including signature bombs (+0xB8/+0xBA
     * cases at seg_1010:3778-3796). player_weapon_index resolves sig tiles
     * to their slots; weapon_inv_index returned -1 for them, which left
     * remote/signature bombs with NO count in the HUD. */
    int widx = player_weapon_index(p->selected_weapon);
    if (widx >= 0) {
        snprintf(buf, sizeof(buf), "%d", (int)p->weapons[widx]);
        DrawTextFON(&hud_font, buf, px, py, text_col);
    }

    /* Player name at (panel_x+20, 1). Matches FUN_1010_6030 (seg_1010:3329)
     * which copies up to 10 chars of name and prints at X=0x20/0xc2/0x165/0x208. */
    DrawTextFON(&hud_font, p->name, px + 20, py + 1, text_col);

    /* Health bar: overlay black on damaged portion of the PLAYERS.SPY strip.
     * (PLAYERS.SPY already renders the full colored "HEALTH" strip.) */
    draw_health_bar(px, py, p->health, p->max_health);

    /* Y=11 line: DIGGING POWER (digging_power + tool bonus). The original's
     * "draw_score_displays" (seg_1010:3458) prints DAT_1038_1c92 + 1c96 =
     * in-level dig power + rockpick/drill bonus. Raw integer, no prefix.
     * (The manual: "Digging power and your current cash are displayed
     * under your name".) */
    snprintf(buf, sizeof(buf), "%d", (int)(p->digging_power + p->bonus_stat));
    DrawTextFON(&hud_font, buf, px + HUD_DIG_X, py + HUD_DIG_Y, text_col);

    /* Y=21 line: MONEY = wallet + this round's earnings. The original's
     * "draw_points_displays" (seg_1010:3515) prints DAT_1038_1cd0 + 1cd4
     * (earned + wallet). */
    snprintf(buf, sizeof(buf), "%d", (int)(p->cash + p->earned));
    DrawTextFON(&hud_font, buf, px + HUD_MONEY_X, py + HUD_MONEY_Y, text_col);
}

void hud_draw(const Player players[], int num_players,
              int frame_counter, bool single_player)
{
    (void)frame_counter;

    /* HUD chrome: portraits, weapon-slot frames, "HEALTH" strips.
     * Only the top 30px of PLAYERS.SPY is the chrome — the rest is black
     * and would overwrite the map, so clip to the HUD band. */
    if (hud_bg_loaded) {
        Rectangle src = { 0, 0, 640, HUD_PANEL_HEIGHT };
        Rectangle dst = { 0, 0, 640, HUD_PANEL_HEIGHT };
        DrawTexturePro(hud_bg_tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }

    /* Clear unused panel slots (black fills over the chrome for missing
     * players). Matches seg_1000:2923-2926:
     *   if (num_players < 4)
     *     fill_rect(Y=0..29, X = num_players*0xa0 - 1 .. 0x27f) */
    if (num_players < 4) {
        int clear_x = num_players * 160 - 1;
        if (clear_x < 0) clear_x = 0;
        DrawRectangle(clear_x, 0, 640 - clear_x, HUD_PANEL_HEIGHT, BLACK);
    }

    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (!players[i].dead) {
            draw_player_panel(&players[i], panel_x[i], HUD_PANEL_Y);
        } else {
            /* Dead player: just show name dimmed */
            Color dim = palette_get_color(8);
            DrawTextFON(&hud_font, players[i].name,
                        panel_x[i] + 20, HUD_PANEL_Y + 1, dim);
        }
    }

    /* Single-player lives display: draw up to 3 life icon sprites.
     * Decompiled FUN_1000_4667 (seg_1000:2874-2903):
     *   bVar1 = min(lives, 3);  loop i=1..bVar1
     *   if i <= lives: draw full at (Y=2, X=(i-1)*16+0x9F)
     *   else:          draw empty at (Y=16, X=(i-1)*16+0x9F) */
    if (single_player && num_players > 0) {
        Texture2D hud_atlas = sprites_get_atlas();
        int lives = players[0].lives;
        int slots = (lives < 3) ? lives : 3;
        if (slots < 1) slots = 1;  /* show at least 1 empty slot on game over */
        for (int i = 0; i < slots; i++) {
            int x = i * HUD_LIVES_SPACING + HUD_LIVES_BASE_X;
            if (i < lives) {
                /* Full life icon */
                Rectangle src = { HUD_LIFE_FULL_SX, HUD_LIFE_FULL_SY,
                                   HUD_LIFE_FULL_W, HUD_LIFE_FULL_H };
                DrawTextureRec(hud_atlas, src, (Vector2){x, HUD_LIVES_Y_FULL}, WHITE);
            } else {
                /* Empty life icon */
                Rectangle src = { HUD_LIFE_EMPTY_SX, HUD_LIFE_EMPTY_SY,
                                   HUD_LIFE_EMPTY_W, HUD_LIFE_EMPTY_H };
                DrawTextureRec(hud_atlas, src, (Vector2){x, HUD_LIVES_Y_EMPTY}, WHITE);
            }
        }
    }
}

int hud_timer_fill_width(int time_remaining, int time_total)
{
    if (time_total <= 0) return 0;

    int elapsed = time_total - time_remaining;
    if (elapsed < 0) elapsed = 0;
    if (elapsed > time_total) elapsed = time_total;

    /* elapsed/total scaled onto the bar background span (X 2..637 = 635px,
     * see HUD_TIMER_MAX_W derivation in hud.h). */
    long long fill_w = ((long long)elapsed * HUD_TIMER_MAX_W) / time_total;
    if (fill_w < 0) fill_w = 0;
    if (fill_w > HUD_TIMER_MAX_W) fill_w = HUD_TIMER_MAX_W;
    return (int)fill_w;
}

void hud_draw_timer(int time_remaining, int time_total)
{
    /* Visual model (color/geometry verified against DOSBox
     * captures): the REMAINING time is the left-anchored gold bar; the
     * elapsed portion is erased in black from the right edge inward.
     * The original draws the full-width bar once at round start
     * (redraw_game_screen seg_1000:2937-2938, MP only, NOT gated on a
     * time limit being set) and then every 20 frames paints
     * fill_rect(0x1dd,0x27d,0x1d9,0x27d-fill) over the right side
     * (seg_1000:7278-7289) — its x_left is 637-fill, so x=637 goes
     * black at the first erase even with fill 0, leaving the gold span
     * at 2..636. We always draw that steady state; the original's
     * 637th gold column exists only for the first 20 frames. With no
     * time limit the bar stays full. */
    int fill_w = hud_timer_fill_width(time_remaining, time_total);

    DrawRectangle(HUD_TIMER_X_LEFT, HUD_TIMER_Y_TOP, HUD_TIMER_BACK_W,
                  HUD_TIMER_H, BLACK);
    int remaining_w = HUD_TIMER_MAX_W - fill_w;
    if (remaining_w > 0) {
        DrawRectangle(HUD_TIMER_X_LEFT, HUD_TIMER_Y_TOP, remaining_w,
                      HUD_TIMER_H, palette_get_color(HUD_TIMER_COLOR));
    }
}

/* Map tile type to minimap color index (palette 0-15).
 * Faithfully reproduces FUN_1010_dab7 (seg_1010:8150-8199). */
static uint8_t tile_to_minimap_color(uint8_t tile)
{
    /* Indestructible walls '2'-'4' */
    if (tile >= 0x32 && tile <= 0x34) return 12;

    /* Destructible walls '7'-'9', 'A'-'F' */
    if ((tile >= 0x37 && tile <= 0x39) || (tile >= 0x41 && tile <= 0x46)) return 9;

    /* Treasure 's' (0x73) or 0x92-0x9A */
    if (tile == 0x73 || (tile > 0x91 && tile < 0x9B)) return 5;

    /* Empty floor '0', 'f', 0xAF */
    if (tile == 0x30 || tile == 0x66 || tile == 0xAF) return 14;

    /* Damaged walls '5'-'6' */
    if (tile >= 0x35 && tile <= 0x36) return 12;

    /* Indestructible wall '1' */
    if (tile == 0x31) return 8;

    /* Special tiles: 0xA4, 'p' (0x70), 'q' (0x71) */
    if (tile == 0xA4 || tile == 0x70 || tile == 0x71) return 9;

    /* Explosive 'e' (0x65) */
    if (tile == 0x65) return 14;

    /* Mystery box 'y' (0x79) */
    if (tile == 0x79) return 12;

    /* Teleporter 0x9C */
    if (tile == 0x9C) return 12;

    /* Proximity mine 'o' (0x6F) */
    if (tile == 0x6F) return 4;

    /* Default: dark */
    return 12;
}

void hud_draw_minimap(const TileMap *map, const Player players[],
                      int num_players)
{
    /* Draw each tile as a 1x1 pixel rectangle at the minimap position.
     * VGA convention: row (0-63) = screen X, col (0-44) = screen Y.
     * Minimap: row → horizontal, col → vertical. */
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = map->tiles[row][col];
            uint8_t ci = tile_to_minimap_color(tile);
            Color c = palette_get_color(ci);
            DrawRectangle(MINIMAP_X + row, MINIMAP_Y + col, 1, 1, c);
        }
    }

    /* Draw player positions as bright dots (palette index 15 = white) */
    Color player_color = palette_get_color(15);
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (players[i].dead) continue;
        int row = pixel_to_tile_row(players[i].x_pos);
        int col = pixel_to_tile_col(players[i].y_pos);
        if (row >= 0 && row < MAP_ROWS && col >= 0 && col < MAP_COLS) {
            DrawRectangle(MINIMAP_X + row, MINIMAP_Y + col, 1, 1, player_color);
        }
    }
}

void hud_cleanup(void)
{
    UnloadFON(&hud_font);
    if (hud_bg_loaded) {
        UnloadTexture(hud_bg_tex);
        UnloadImage(hud_bg_img);
        hud_bg_loaded = false;
    }
}
