#include "debug_overlay.h"
#include "game/map_renderer.h"
#include "game/sprites.h"
#include "util/prng.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

static bool g_debug_enabled = false;
static bool g_overlay_visible = true;  /* toggled by F3 */
static const Round *g_round = NULL;

void debug_parse_args(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            g_debug_enabled = true;
            SetTraceLogLevel(LOG_DEBUG);
        }
    }
}

void debug_set_round(const Round *r)
{
    g_round = r;
}

/* Draw text with a dark background panel for readability. */
static void draw_panel_text(const char *text, int x, int y, int font_size, Color color)
{
    DrawText(text, x, y, font_size, color);
}

static const char *dir_name(int dir)
{
    /* Original direction encoding */
    switch (dir) {
    case 0: return "STOP";
    case 1: return "RIGHT";
    case 2: return "LEFT";
    case 3: return "UP";
    case 4: return "DOWN";
    default: return "???";
    }
}

static const char *round_state_name(RoundState s)
{
    switch (s) {
    case ROUND_FADE_IN: return "FADE_IN";
    case ROUND_RUNNING: return "RUNNING";
    case ROUND_ENDING:  return "ENDING";
    case ROUND_FADE_OUT: return "FADE_OUT";
    case ROUND_OVER:    return "OVER";
    default: return "???";
    }
}

void debug_draw(const Player players[], int num_players)
{
    if (!g_debug_enabled) return;

    /* F3 toggles overlay visibility */
    if (IsKeyPressed(KEY_F3)) {
        g_overlay_visible = !g_overlay_visible;
    }

    if (!g_overlay_visible) return;

    const int fs = 16;  /* font size */
    const int lh = 18;  /* line height */
    char buf[256];

    /* === Top-right: FPS / frame time / RNG seed === */
    {
        int x = 1280 - 260;
        int y = 4;
        DrawRectangle(x - 4, y - 2, 260, lh * 4 + 4, (Color){0, 0, 0, 160});

        snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
        draw_panel_text(buf, x, y, fs, GREEN);
        y += lh;

        snprintf(buf, sizeof(buf), "Frame time: %.2f ms", GetFrameTime() * 1000.0f);
        draw_panel_text(buf, x, y, fs, GREEN);
        y += lh;

        snprintf(buf, sizeof(buf), "RNG: 0x%08X", mb_prng_get_seed());
        draw_panel_text(buf, x, y, fs, GREEN);
        y += lh;

        if (g_round) {
            snprintf(buf, sizeof(buf), "Round frame: %d", g_round->frame_counter);
            draw_panel_text(buf, x, y, fs, GREEN);
        }
    }

    /* === Left side: per-player data (only during gameplay) === */
    if (g_round && players) {
        int x = 4;
        int y = 4;
        const int lines_per_player = 7;  /* name + pos + tile + hp + weapon + dig + lives/points */
        int panel_h = num_players * (lh * lines_per_player + 8) + 4;
        DrawRectangle(x - 2, y - 2, 340, panel_h, (Color){0, 0, 0, 160});

        for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
            const Player *p = &players[i];
            Color col = (p->dead) ? RED : LIME;

            snprintf(buf, sizeof(buf), "P%d: %s %s", i + 1, p->name,
                     p->dead ? "[DEAD]" : "");
            draw_panel_text(buf, x, y, fs, col);
            y += lh;

            /* Intra-tile position: x_pos % 10 and (y_pos - MAP_Y_OFFSET) % 10 */
            int intra_x = p->x_pos % 10;
            int intra_y = (p->y_pos - MAP_Y_OFFSET) % 10;
            if (intra_y < 0) intra_y += 10;
            snprintf(buf, sizeof(buf), "  Pos: (%d, %d)  Intra: (%d, %d)  Dir: %s",
                     p->x_pos, p->y_pos, intra_x, intra_y, dir_name(p->last_direction));
            draw_panel_text(buf, x, y, fs, col);
            y += lh;

            /* Tile under player's center */
            int cx = p->x_pos + SPRITE_W / 2;
            int cy = p->y_pos + SPRITE_H / 2;
            int prow = pixel_to_tile_row(cx);
            int pcol = pixel_to_tile_col(cy);
            uint8_t tile = 0;
            uint16_t overlay = 0;
            if (prow >= 0 && prow < MAP_ROWS && pcol >= 0 && pcol < MAP_COLS) {
                tile = g_round->map.tiles[prow][pcol];
                overlay = g_round->map.overlay[prow][pcol];
            }
            snprintf(buf, sizeof(buf), "  Tile[%d][%d]=0x%02X  ovl=%u",
                     prow, pcol, tile, overlay);
            draw_panel_text(buf, x, y, fs, col);
            y += lh;

            snprintf(buf, sizeof(buf), "  HP: %d/%d  Cash: $%d",
                     p->health, p->max_health, p->cash);
            draw_panel_text(buf, x, y, fs, col);
            y += lh;

            snprintf(buf, sizeof(buf), "  Weapon: 0x%02X  Speed: %d",
                     p->selected_weapon, p->speed_divisor);
            draw_panel_text(buf, x, y, fs, col);
            y += lh;

            snprintf(buf, sizeof(buf), "  Dig: %d+%d  Kills: %d  Wins: %d",
                     p->digging_power, p->bonus_stat, p->kills, p->round_wins);
            draw_panel_text(buf, x, y, fs, col);
            y += lh;

            if (g_round->single_player) {
                snprintf(buf, sizeof(buf), "  Lives: %d", p->lives);
                draw_panel_text(buf, x, y, fs, col);
                y += lh;
            } else {
                snprintf(buf, sizeof(buf), "  Earned: %d", p->earned);
                draw_panel_text(buf, x, y, fs, col);
                y += lh;
            }

            y += 8;  /* gap between players */
        }
    }

    /* === Bottom-left: round state (only during gameplay) === */
    if (g_round) {
        int x = 4;
        int y = 960 - (lh * 6 + 8);
        DrawRectangle(x - 2, y - 2, 360, lh * 6 + 4, (Color){0, 0, 0, 160});

        snprintf(buf, sizeof(buf), "Round: %d  State: %s",
                 g_round->round_number, round_state_name(g_round->state));
        draw_panel_text(buf, x, y, fs, YELLOW);
        y += lh;

        if (g_round->time_remaining > 0) {
            int secs = g_round->time_remaining / 60;
            snprintf(buf, sizeof(buf), "Time: %d:%02d", secs / 60, secs % 60);
        } else {
            snprintf(buf, sizeof(buf), "Time: unlimited");
        }
        draw_panel_text(buf, x, y, fs, YELLOW);
        y += lh;

        snprintf(buf, sizeof(buf), "Inactivity: %d / %d",
                 g_round->inactivity, INACTIVITY_MAX);
        draw_panel_text(buf, x, y, fs, YELLOW);
        y += lh;

        /* Count entities */
        int entity_count = 0;
        Entity *e = g_round->entity_head;
        while (e) {
            if (!e->dead) entity_count++;
            e = e->next;
        }
        snprintf(buf, sizeof(buf), "Entities: %d", entity_count);
        draw_panel_text(buf, x, y, fs, YELLOW);
        y += lh;

        snprintf(buf, sizeof(buf), "Treasures: %d",
                 round_count_treasures(&g_round->map));
        draw_panel_text(buf, x, y, fs, YELLOW);
        y += lh;

        snprintf(buf, sizeof(buf), "Shake: %d  Darkness: %s",
                 g_round->map.screen_shake,
                 g_round->darkness_enabled ? "ON" : "OFF");
        draw_panel_text(buf, x, y, fs, YELLOW);
    }
}
