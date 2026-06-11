#include "game/round.h"
#include "game/movement.h"
#include "game/bombs.h"
#include "game/ai.h"
#include "game/hud.h"
#include "game/sprites.h"
#include "game/map_renderer.h"
#include "game/visibility.h"
#include "game/config.h"
#include "game/player_db.h"
#include "input/input.h"
#include "audio/sfx.h"
#include "gfx/palette.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "util/prng.h"

/* Palette fade steps matching original (all calls use 7).
 * Decompiled ref: seg_1000_game_logic.c:7135 (fade in), 7142/7161/7273/7291 (fade out). */
#define ROUND_FADE_STEPS 7

/* Starting positions for players on the map (corners).
 * Verified against decompiled FUN_1010_c4f2 (seg_1010:7232-7288):
 *   Position A: pixel (0x0F, 0x2D)  = tile (row=1, col=1)   = top-left
 *   Position B: pixel (0x271, 0x1D1) = tile (row=62, col=43) = bottom-right
 *   Position C: pixel (0x271, 0x2D)  = tile (row=62, col=1)  = bottom-left
 *   Position D: pixel (0x0F, 0x1D1)  = tile (row=1, col=43)  = top-right
 * P1/P2 get A/B or B/A (random). P3/P4 get C/D or D/C (random). */
static const int START_COL[MAX_PLAYERS] = { 1, 43, 1, 43 };
static const int START_ROW[MAX_PLAYERS] = { 1, 62, 62, 1 };

/* Scan map for entity spawn tiles ('G'-'V') and create entity list */
static void spawn_entities(Round *r)
{
    r->entity_head = NULL;
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = r->map.tiles[row][col];
            if (tile >= 'G' && tile <= 'V') {
                Entity *e = entity_spawn(tile, col, row);
                if (e) {
                    entity_list_add(&r->entity_head, e);
                    /* Clear spawn tile to floor */
                    r->map.tiles[row][col] = '0';
                }
            }
        }
    }
}

/* Clear a vertical strip of tiles to floor at a given column.
 * Decompiled ref: seg_1010:7458-7512, path clearing near spawn corners. */
static void clear_vertical_strip(TileMap *map, int col, int start_row, int end_row)
{
    if (start_row <= end_row) {
        for (int r = start_row; r <= end_row; r++)
            map->tiles[r][col] = '0';
    } else {
        for (int r = start_row; r >= end_row; r--)
            map->tiles[r][col] = '0';
    }
}

/* Clear a horizontal strip of tiles to floor at a given row. */
static void clear_horizontal_strip(TileMap *map, int row, int start_col, int end_col)
{
    if (start_col <= end_col) {
        for (int c = start_col; c <= end_col; c++)
            map->tiles[row][c] = '0';
    } else {
        for (int c = start_col; c >= end_col; c--)
            map->tiles[row][c] = '0';
    }
}

/* Clear random-length paths near spawn corners to ensure players can move.
 * Decompiled ref: seg_1010:7456-7512. Each strip is 4+random(6) tiles long.
 * For 2+ players: clear near top-left and bottom-right corners.
 * For 3+ players: also clear near bottom-left and top-right corners. */
static void clear_spawn_paths(TileMap *map, int num_players)
{
    if (num_players < 2) return;

    /* Top-left corner (row=1, col=1): clear rightward + downward */
    int len = 4 + mb_random(6);
    clear_horizontal_strip(map, 1, 1, len);
    len = 4 + mb_random(6);
    clear_vertical_strip(map, 1, 1, len);

    /* Bottom-right corner (row=62, col=43): clear leftward + upward */
    len = 4 + mb_random(6);
    clear_horizontal_strip(map, 62, 43, 43 - len + 1);
    len = 4 + mb_random(6);
    clear_vertical_strip(map, 43, 62, 62 - len + 1);

    if (num_players < 3) return;

    /* Bottom-left corner (row=62, col=1): clear rightward + upward */
    len = 4 + mb_random(6);
    clear_horizontal_strip(map, 62, 1, len);
    len = 4 + mb_random(6);
    clear_vertical_strip(map, 1, 62, 62 - len + 1);

    /* Top-right corner (row=1, col=43): clear leftward + downward */
    len = 4 + mb_random(6);
    clear_horizontal_strip(map, 1, 43, 43 - len + 1);
    len = 4 + mb_random(6);
    clear_vertical_strip(map, 43, 1, len);
}

/* Place players at starting positions and reset per-round state.
 * Decompiled ref: game_state_update (seg_1010:7399-7425) resets dead flags,
 * recalculates health via FUN_1010_c15c, and FUN_1010_c4f2 sets positions.
 *
 * In multiplayer, corner assignments are randomly swapped per pair:
 *   P1/P2: randomly swap between top-left/bottom-right (seg_1010:7254-7268)
 *   P3/P4: randomly swap between bottom-left/top-right (seg_1010:7270-7286) */
void round_place_players(Round *r, Player players[], int num_players)
{
    int col[MAX_PLAYERS] = { START_COL[0], START_COL[1], START_COL[2], START_COL[3] };
    int row[MAX_PLAYERS] = { START_ROW[0], START_ROW[1], START_ROW[2], START_ROW[3] };

    /* Random corner swap for multiplayer (FUN_1010_c4f2, seg_1010:7254-7268) */
    if (num_players > 1) {
        if (mb_random(2)) {
            int tc = col[0]; col[0] = col[1]; col[1] = tc;
            int tr = row[0]; row[0] = row[1]; row[1] = tr;
        }
    }
    if (num_players > 2) {
        if (mb_random(2)) {
            int tc = col[2]; col[2] = col[3]; col[3] = tc;
            int tr = row[2]; row[2] = row[3]; row[3] = tr;
        }
    }

    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        players[i].x_pos = tile_to_pixel_x(row[i]);
        players[i].y_pos = tile_to_pixel_y(col[i]);
        /* Round start inits facing (+0xA6) to 1 = RIGHT (seg_1010:7404-7407) */
        players[i].last_direction = DIR_RIGHT;

        /* Reset per-round state and recalculate health */
        player_reset_for_round(&players[i]);
    }

    /* Clear paths near spawn corners so players can move */
    clear_spawn_paths(&r->map, num_players);
}

bool round_init(Round *r, const char *map_path, int round_number,
                bool single_player, int time_limit)
{
    memset(r, 0, sizeof(Round));

    if (!map_load(&r->map, map_path)) {
        TraceLog(LOG_ERROR, "ROUND: Failed to load map: %s", map_path);
        return false;
    }
    map_init_collision(&r->map);

    r->round_number = round_number;
    r->single_player = single_player;
    r->frame_counter = 0;
    r->inactivity = 0;
    r->state = ROUND_FADE_IN;
    r->fade_step = 0;
    r->escaped = false;

    /* Darkness/fog-of-war: always on in single-player, optional in multiplayer
     * via "Darkness" config option (g_config.option_toggle[0]).
     * Decompiled ref: seg_1010:7197-7201. When active, minimap overlay is
     * also drawn (player needs it to navigate the hidden map). */
    /* Install the in-game (SIKA.SPY) palette: the original fades the round
     * in to the palette at DS:0x688 (seg_1000:7135). Without this the
     * palette module still holds the previous screen's palette — or black
     * after its fade-out — and all in-round palette_get_color draws (HUD
     * text, timer bar) are invisible. */
    palette_init(sprites_get_palette());

    r->darkness_enabled = (single_player || g_config.option_toggle[0] != 0);
    r->map.darkness_enabled = r->darkness_enabled;

    /* Initialize visibility: mark all tiles as hidden (seg_1010:7436-7449).
     * The visibility reveal system will reveal tiles as the player moves. */
    if (r->darkness_enabled) {
        visibility_init(&r->map);
        map_renderer_set_darkness(true);
    } else {
        map_renderer_set_darkness(false);
    }

    if (time_limit > 0) {
        r->time_remaining = time_limit;
        r->time_total = time_limit;
    } else {
        r->time_remaining = -1;
        r->time_total = -1;
    }

    spawn_entities(r);

    return true;
}

bool round_init_random(Round *r, int round_number, int time_limit,
                       int treasure_count)
{
    memset(r, 0, sizeof(Round));

    map_generate_random(&r->map, treasure_count);
    map_init_collision(&r->map);

    r->round_number = round_number;
    r->single_player = false;  /* random maps are multiplayer only */
    r->frame_counter = 0;
    r->inactivity = 0;
    r->state = ROUND_FADE_IN;
    r->fade_step = 0;
    r->escaped = false;
    /* Install the in-game palette (see round_init) */
    palette_init(sprites_get_palette());

    r->darkness_enabled = (g_config.option_toggle[0] != 0);
    r->map.darkness_enabled = r->darkness_enabled;

    if (r->darkness_enabled) {
        visibility_init(&r->map);
        map_renderer_set_darkness(true);
    } else {
        map_renderer_set_darkness(false);
    }

    if (time_limit > 0) {
        r->time_remaining = time_limit;
        r->time_total = time_limit;
    } else {
        r->time_remaining = -1;
        r->time_total = -1;
    }

    spawn_entities(r);

    return true;
}

int round_count_alive(const Player players[], int num_players)
{
    int alive = 0;
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (!players[i].dead) alive++;
    }
    return alive;
}

bool round_check_exit_tile(const Player *p, const TileMap *map)
{
    int row = pixel_to_tile_row(p->x_pos);
    int col = pixel_to_tile_col(p->y_pos);
    return map_get_tile(map, row, col) == 'k';
}

int round_count_treasures(const TileMap *map)
{
    /* Count cash treasure tiles matching decompiled FUN_1000_6ddc (seg_1000:4286-4311):
     * 0x73 ('s' super treasure) or 0x92-0x9A (cash treasures).
     * Note: stat gems (0x8F-0x91) are NOT treasures for round-end purposes. */
    int count = 0;
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            uint8_t tile = map->tiles[row][col];
            if (tile == 0x73 || (tile > 0x91 && tile < 0x9B)) {
                count++;
            }
        }
    }
    return count;
}

int round_resolve_direction(int player_idx, int current_direction)
{
    /* Original key→direction resolution (process_weapons seg_1000:2608-2630).
     * Held-key priority is the nesting order Up > Right > Down > Left > Stop
     * (original values 3/1/4/2/0 — note the original's numeric encoding is
     * 1=RIGHT 2=LEFT 3=UP 4=DOWN; the port's DIR_* constants use the same encoding).
     *  Stop is level-triggered and LOWEST priority: a held direction
     * key wins over a held stop key. With nothing held the direction is left
     * unchanged — the player keeps sliding until stopped or blocked. */
    if (player_input_down(player_idx, PLAYER_INPUT_UP))    return DIR_UP;
    if (player_input_down(player_idx, PLAYER_INPUT_RIGHT)) return DIR_RIGHT;
    if (player_input_down(player_idx, PLAYER_INPUT_DOWN))  return DIR_DOWN;
    if (player_input_down(player_idx, PLAYER_INPUT_LEFT))  return DIR_LEFT;
    if (player_input_down(player_idx, PLAYER_INPUT_STOP))  return DIR_STOP;
    return current_direction;
}

static void process_player_input(Player *p, int player_idx)
{
    /* Facing := direction at the head of the weapon tick (process_weapons,
     * seg_1000:2602-2604) — independent of movement success, so pushing
     * against a wall turns the sprite toward it. Runs BEFORE the key
     * re-resolution below, preserving the original's one-tick lag. */
    if (p->direction != DIR_STOP)
        p->last_direction = p->direction;
    p->direction = (uint8_t)round_resolve_direction(player_idx, p->direction);
}

/* Check if player should die from accumulated damage.
 * Handles deaths caused by entity collision (bombs handle their own deaths
 * via bombs_check_player_damage). Plays death sound #12 (aargh.voc) and
 * places corpse tile 'f' at death position.
 * Decompiled ref: check_player_death (seg_1000:5975-6008). */
static void check_player_death(Player *p, TileMap *map)
{
    if (p->dead) return;
    if (p->health <= 0) {
        /* Play death sound: trigger #12 = aargh.voc (seg_1000:6001) */
        sfx_play(SFX_AARGH);

        /* Place corpse tile 'f' at death position (seg_1000:6002-6005).
         * Original: g_tile_map[y_pos/10 * 0x2d + (x_pos-0x1E)/10] = 0x66 */
        int death_row = pixel_to_tile_row(p->x_pos + SPRITE_W / 2);
        int death_col = pixel_to_tile_col(p->y_pos + SPRITE_H / 2);
        if (death_row >= 0 && death_row < MAP_ROWS &&
            death_col >= 0 && death_col < MAP_COLS) {
            map->tiles[death_row][death_col] = 'f';
        }

        p->dead = 1;
        p->active = 0;
        p->deaths++;  /* harness observability */
        /* The original's death handler also increments match-stats block
         * +0x20 = dword 8 (seg_1000:5994-5998) — identified as the DEATHS
         * counter. */
        p->match_stats[STAT_DEATHS] += 1;
    }
}

/* Helper: begin fade-out transition from gameplay */
static void round_begin_fade_out(Round *r)
{
    r->state = ROUND_FADE_OUT;
    r->fade_step = 0;
}

/* One-shot key latches: capture IsKeyPressed on any frame so the
 * every-2-frames weapon tick doesn't miss presses on odd frames (raylib's
 * edge is true for exactly one frame, while the original's ISR key byte
 * stays set from key-make until the handler consumes it). Cleared when a
 * new round starts (ROUND_FADE_IN → ROUND_RUNNING transition).
 * Decompiled ref: bomb seg_1000:2631-2632, choose seg_1000:2795-2796 —
 * both read key_state then clear (one-shot). */
static bool bomb_latch[MAX_PLAYERS];
static bool cycle_latch[MAX_PLAYERS];

RoundState round_update(Round *r, Player players[], int num_players)
{
    if (r->state == ROUND_OVER) return ROUND_OVER;

    /* Fade-in: advance overlay from black to clear, then start gameplay */
    if (r->state == ROUND_FADE_IN) {
        r->fade_step++;
        if (r->fade_step >= ROUND_FADE_STEPS) {
            r->state = ROUND_RUNNING;
            /* Clear any stale key latches from previous round */
            memset(bomb_latch, 0, sizeof(bomb_latch));
            memset(cycle_latch, 0, sizeof(cycle_latch));
        }
        return r->state;
    }

    /* Fade-out: advance overlay from clear to black, then signal done */
    if (r->state == ROUND_FADE_OUT) {
        r->fade_step++;
        if (r->fade_step >= ROUND_FADE_STEPS) {
            r->state = ROUND_OVER;
            return ROUND_OVER;
        }
        return r->state;
    }

    r->frame_counter++;

    /* Debug: log first few frames */
    if (r->frame_counter <= 25) {
        int alive = round_count_alive(players, num_players);
        TraceLog(LOG_INFO, "ROUND: frame=%d alive=%d/%d inactivity=%d time_rem=%d treasures=%d hp=[%d,%d] dead=[%d,%d]",
                 r->frame_counter, alive, num_players, r->inactivity,
                 r->time_remaining, round_count_treasures(&r->map),
                 num_players > 0 ? players[0].health : -1,
                 num_players > 1 ? players[1].health : -1,
                 num_players > 0 ? players[0].dead : -1,
                 num_players > 1 ? players[1].dead : -1);
    }

    /* In-round special keys — identities re-derived from MB.EXE bytes at
     * 49020-49135 (seg_1000:7141-7163; key-state array byte = 0x7553 +
     * scancode): "g_key_mp_sync" = 0x7554 → ESC, "g_key_pause" = 0x756C →
     * P, "g_key_screen_toggle" = 0x7592 → F5 (music mute), "g_key_esc" =
     * 0x7597 → F10. The decompiler names had ESC and the abort key
     * swapped. Original check order: ESC, P, F5, F10. */

    /* ESC: ends the ROUND (no match abort), multiplayer only
     * (seg_1000:7141-7145). In single-player ESC does nothing. */
    if (num_players > 1 && input_pressed(INPUT_CANCEL)) {
        TraceLog(LOG_INFO, "ROUND: ended by ESC at frame %d", r->frame_counter);
        r->end_reason = ROUND_END_SYNC;
        round_begin_fade_out(r);
        return r->state;
    }

    /* Pause: P (scancode 0x19; seg_1000:7146, FUN_1000_7194).
     * Original darkens palette by >>1 (50% brightness), waits for any key
     * to resume via FUN_1010_a259 (scans scancodes 1-0xB7). */
    if (!r->paused && IsKeyPressed(KEY_P)) {
        r->paused = true;
        return r->state;
    }
    if (r->paused) {
        /* Any key press unpauses (matching FUN_1010_a259) */
        if (GetKeyPressed() != 0) {
            r->paused = false;
        }
        return r->state;
    }

    /* F10: abort the match — both modes (seg_1000:7160-7163:
     * rounds_remaining = 0, round over). */
    if (IsKeyPressed(KEY_F10)) {
        TraceLog(LOG_INFO, "ROUND: match aborted by F10 at frame %d",
                 r->frame_counter);
        r->escaped = true;
        r->end_reason = ROUND_END_ESC;
        round_begin_fade_out(r);
        return r->state;
    }

    /* === Every frame === */
    /* Latch one-shot key presses so the every-2-frames check doesn't miss
     * them. */
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (player_input_pressed(i, PLAYER_INPUT_BOMB))
            bomb_latch[i] = true;
        if (player_input_pressed(i, PLAYER_INPUT_CYCLE))
            cycle_latch[i] = true;
    }

    /* Movement runs every frame; key reads (direction, bomb, choose,
     * remote) only on even frames inside process_weapons, AFTER the move —
     * original round loop seg_1000:7186-7250.
     * Entity list must be current before the loop: player_dig's bomb-push
     * path checks for entities on the destination tile. */
    bombs_set_entity_list(r->entity_head);
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (players[i].dead) continue;
        player_move(&players[i], &r->map);

        /* Digging: apply wall damage when pushing against walls
         * (seg_1000:3920-4043, called from movement handler) */
        player_dig(&players[i], &r->map);

        /* Speed bonus: move again */
        if (players[i].speed_divisor > 1) {
            player_move(&players[i], &r->map);
            player_dig(&players[i], &r->map);
        }

        /* Fog-of-war reveal now happens inside player_check_pickup at
         * tile-center crossings (original FUN_1000_5073, seg_1000:3351-3362)
         * — not per frame. */
    }

    /* Set entity list so explosion damage can check entities */
    bombs_set_entity_list(r->entity_head);
    bombs_update(&r->map);

    /* === Every 2 frames === */
    if (r->frame_counter % 2 == 0) {
        for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
            if (players[i].dead) continue;

            /* Key processing order matches process_weapons (seg_1000:2582):
             * direction (2608), bomb +0xF8 (2631), choose +0xFA (2795),
             * remote +0xF9 (2801). */
            process_player_input(&players[i], i);

            /* Bomb placement — latched across frames so we don't miss
             * presses that land on odd frames. Latch is consumed here,
             * matching original (seg_1000:2631-2632) clear-after-read. */
            if (bomb_latch[i]) {
                bomb_latch[i] = false;
                if (players[i].cooldown <= 0) {
                    /* Creature spawner (0x6E): spawn entity at player position.
                     * Decompiled ref: seg_1000:2641-2655 — process_weapons
                     * calls FUN_1000_3b40(player_num, col, row). */
                    bool is_creature = (players[i].selected_weapon == WEAPON_CREATURE_N);
                    if (bomb_place(&players[i], &r->map) && is_creature) {
                        int sr = pixel_to_tile_row(players[i].x_pos + SPRITE_W / 2);
                        int sc = pixel_to_tile_col(players[i].y_pos + SPRITE_H / 2);
                        Entity *e = entity_spawn_creature(i, sc, sr);
                        if (e) {
                            /* Clone inherits the owner's direction and
                             * facing (seg_1000:2575-2576) */
                            e->direction = players[i].direction;
                            e->prev_direction = players[i].last_direction;
                            entity_list_add(&r->entity_head, e);
                        }
                    }
                }
            }

            /* Weapon cycling — latched across frames like the bomb key;
             * one-shot (original clears the key byte on consume). */
            if (cycle_latch[i]) {
                cycle_latch[i] = false;
                player_cycle_weapon(&players[i], 1);
            }

            /* Remote detonation — LEVEL-triggered: the original does NOT
             * clear the key byte (seg_1000:2801-2817), so the signature-tile
             * scan re-runs every weapon tick while the key is held. */
            if (player_input_down(i, PLAYER_INPUT_REMOTE)) {
                bombs_remote_detonate(&r->map, &players[i]);
            }

            /* Cooldown tick */
            if (players[i].cooldown > 0)
                players[i].cooldown--;

            check_player_death(&players[i], &r->map);
        }
    }

    /* Entity damage: every frame (matching decompiled monster_player_collision
     * at seg_1000:7261, outside any frame-counter modulo check) */
    entities_deal_damage(r->entity_head, players, num_players);

    /* === Every 5 frames === */
    if (r->frame_counter % 5 == 0) {
        /* Entity activation: every 5 frames (matching decompiled
         * player_collision_check at seg_1000:7253-7259) */
        entities_activate(r->entity_head, players, num_players, &r->map);

        /* Check alive count */
        int alive = round_count_alive(players, num_players);

        if (r->single_player) {
            /* Single-player: check exit tile */
            if (alive > 0 && round_check_exit_tile(&players[0], &r->map)) {
                r->end_reason = ROUND_END_EXIT;
                round_begin_fade_out(r);
                return r->state;
            }
            /* Single-player death: inactivity ramp before ending round.
             * (from decompiled lines 7170-7179)
             * Lives decrement and retry/game-over decision happen in main.c
             * after ROUND_OVER is returned. */
            if (alive == 0 && players[0].dead) {
                r->inactivity += 2;
            }
        } else {
            /* Multiplayer: <2 alive starts ending */
            if (alive < 2) {
                if (r->inactivity == 0)
                    TraceLog(LOG_INFO, "ROUND: <2 alive (%d), starting inactivity ramp at frame %d", alive, r->frame_counter);
                r->inactivity += INACTIVITY_FEW_ALIVE;
                if (r->state != ROUND_ENDING)
                    r->state = ROUND_ENDING;
            }
        }

        if (r->inactivity > INACTIVITY_MAX) {
            TraceLog(LOG_INFO, "ROUND: inactivity reached %d, ending at frame %d", r->inactivity, r->frame_counter);
            r->end_reason = ROUND_END_INACTIVITY;
            round_begin_fade_out(r);
            return r->state;
        }
    }

    /* === Every 18 frames === (game_tick_update, seg_1010:7692) */
    if (r->frame_counter % 18 == 0) {
        for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
            player_money_bomb_tick(&players[i]);
        }
    }

    /* Time limit countdown — PIT-tick model.
     * The original's INT8 ISR decrements g_time_remaining once per
     * 18.2065 Hz tick, wall-clock; at our locked 60 fps that is one
     * tick every 60*65536/1193182 = 3.2955 frames. Exact integer
     * accumulator: add 1193182 per frame, a tick elapses per 3932160.
     * Expiry (remaining < 1) is checked every frame (seg_1000:7290):
     *   MP: expiry ends the round (fade out).
     *   SP: expiry RESETS the timer to full — does not end the round.
     */
    if (r->time_remaining > 0) {
        r->tick_accum += 1193182;
        while (r->tick_accum >= 3932160 && r->time_remaining > 0) {
            r->tick_accum -= 3932160;
            r->time_remaining--;
        }
        if (r->time_remaining < 1) {
            if (r->single_player) {
                TraceLog(LOG_INFO, "ROUND: SP time limit hit, resetting at frame %d",
                         r->frame_counter);
                r->time_remaining = r->time_total;
            } else {
                TraceLog(LOG_INFO, "ROUND: time limit expired at frame %d",
                         r->frame_counter);
                r->end_reason = ROUND_END_TIME;
                round_begin_fade_out(r);
                return r->state;
            }
        }
    }

    /* === Every 20 frames === */
    if (r->frame_counter % 20 == 0) {

        /* Treasure collection end condition (from decompiled lines 7264-7266):
         * when all treasures collected and >1 player, add 20 to inactivity */
        if (num_players > 1 && round_count_treasures(&r->map) == 0) {
            if (r->inactivity == 0)
                TraceLog(LOG_INFO, "ROUND: no treasures left, starting inactivity ramp at frame %d", r->frame_counter);
            r->inactivity += INACTIVITY_TREASURES;
            if (r->state != ROUND_ENDING)
                r->state = ROUND_ENDING;
        }
    }

    /* === Every 26 frames === */
    if (r->frame_counter % AI_DECISION_TICK == 0) {
        int treasures = round_count_treasures(&r->map);
        Entity *e = r->entity_head;
        while (e) {
            if (!e->dead) {
                ai_update(e, &r->map, players, num_players,
                          r->frame_counter, r->entity_head, treasures);
            }
            e = e->next;
        }
    }

    /* Entity movement (every frame, throttled internally by speed_divisor) */
    entities_update(r->entity_head, &r->map, players, num_players,
                    r->frame_counter);

    return r->state;
}

/* Map DIR_ values to sprite sheet band indices.
 * Sprite sheet bands: 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN — i.e. the original's
 * direction value minus 1, now that DIR_* uses the original encoding
 * (1=RIGHT 2=LEFT 3=UP 4=DOWN; the original indexes the per-
 * direction sprite-set pointers at player +0x12 + dir*0x10).
 * Used for both player and entity sprite rendering. */
static const int dir_to_spr[] = {
    3, /* DIR_STOP=0  → DOWN band (fallback; facing is never 0 in-game) */
    0, /* DIR_RIGHT=1 → band 0 */
    1, /* DIR_LEFT=2  → band 1 */
    2, /* DIR_UP=3    → band 2 */
    3, /* DIR_DOWN=4  → band 3 */
};

void round_draw(Round *r, const Player players[], int num_players)
{
    /* Screen shake (seg_1010:7705-7725): on odd shake values, offset the
     * rendered frame vertically by shake_value pixels (original uses CRTC
     * Start Address offset of shake_value * 0x50 bytes = 1 row per unit).
     * On even values, render normally. */
    int shake_offset = 0;
    int shake = r->map.screen_shake;
    if (shake > 0) {
        if (shake % 2 != 0) {
            shake_offset = shake;
        }
    }

    /* Draw map tiles — fixed viewport, no scrolling camera.
     * Only screen shake shifts the rendered frame vertically. */
    map_renderer_draw(shake_offset);

    /* Draw entities using animated directional sprites (same system as players).
     * Entity types 0-3 map to sprite sets 8-11 in PLAYER_SETS.
     * Decompiled: entities share the same 4-dir × 4-frame sprite layout as
     * players (loaded at seg_1010:4867-4870 into sets at Y=50/60/70/80). */
    Texture2D atlas = sprites_get_atlas();
    Entity *e = r->entity_head;
    while (e) {
        if (!e->dead) {
            /* In darkness mode, only draw entities on revealed tiles */
            if (r->darkness_enabled) {
                int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
                int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
                if (!visibility_is_revealed(&r->map, erow, ecol)) {
                    e = e->next;
                    continue;
                }
            }
            int draw_x = e->x_pos;
            int draw_y = e->y_pos + shake_offset;
            if (draw_y >= -SPRITE_H && draw_y < 480 + SPRITE_H) {
                /* Sprite set: type 0-3 → sets 8-11 */
                int variant = 8 + e->type;

                /* Direction mapping (same as players):
                 * Sprite bands: 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN */
                int dir = e->direction;
                if (dir < 0 || dir > 4) dir = 0;
                int spr_dir = dir_to_spr[dir];

                /* Animation frame: anim_state cycles 0-7, map to 4 sprite frames */
                int frame = e->anim_state % 4;

                Rectangle src = sprites_get_player_rect(variant, spr_dir, frame);
                DrawTextureRec(atlas, src, (Vector2){draw_x, draw_y}, WHITE);
            }
        }
        e = e->next;
    }

    /* Draw players */
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (players[i].dead) continue;

        /* Invis cheat: player sprite not drawn (FUN_1000_3095 overwrites all
         * 16 sprite slots with floor tile data, making player invisible) */
        if (players[i].cheat_visual == CHEAT_INVIS) continue;

        int draw_x = players[i].x_pos;
        int draw_y = players[i].y_pos + shake_offset;
        if (draw_y >= -SPRITE_H && draw_y < 480 + SPRITE_H) {
            /* Sprite band: moving uses the CURRENT direction (+0xA4,
             * animate_player_sprite); stopped uses FACING (+0xA6,
             * move_player's standing blit at seg_1000:3893-3898). */
            bool moving = (players[i].direction != DIR_STOP);
            int dir = moving ? players[i].direction : players[i].last_direction;
            if (dir < 0 || dir > 4) dir = 0;
            int spr_dir = dir_to_spr[dir];

            /* Map 0-30 anim_frame to 0-3 sprite frame via 6 thresholds:
             * 0-4→0, 5-9→1, 10-14→2, 15-19→3, 20-24→2, 25-29→1 (ping-pong;
             * same thresholds for walk and dig — animate_player_sprite
             * modes 0 and 1). Stopped: standing frame 0. */
            int frame;
            if (!moving) {
                frame = 0;
            } else {
                int af = players[i].anim_frame;
                if (af < 5)       frame = 0;
                else if (af < 10) frame = 1;
                else if (af < 15) frame = 2;
                else if (af < 20) frame = 3;
                else if (af < 25) frame = 2;
                else              frame = 1;
            }

            int variant;
            if (players[i].cheat_visual == CHEAT_MUTATION) {
                /* Mutation cheat: use monster sprite set 10 (Mon 3, Y=70, X=160).
                 * Original (FUN_1000_3129) copies DAT_1038_2352 (monster variant
                 * 11 loaded at seg_1010:4869) into player sprite data. */
                variant = 10;
            } else if (moving && players[i].digging) {
                /* Digging: per-player DIG sprite set (loader calls 5-8 at
                 * seg_1010:4863-4866 fill player +0x62, which is what
                 * animate_player_sprite mode 1 draws from). */
                variant = 4 + i;
            } else {
                /* Walk sets 0-3. (Sets 4-7 were previously misread as
                 * option_toggle-selected ALT colors; the loader proves they
                 * are the dig sets, and option_toggle[] holds the game
                 * options, not colors.) */
                variant = i;
            }
            Rectangle src = sprites_get_player_rect(variant, spr_dir, frame);
            DrawTextureRec(atlas, src, (Vector2){draw_x, draw_y}, WHITE);
        }
    }

    /* Draw HUD (not affected by screen shake) */
    hud_draw(players, num_players, r->frame_counter, r->single_player);

    /* Timer bar: multiplayer only (seg_1000:7278/2937), drawn even with
     * no time limit set — it then just stays full. */
    if (!r->single_player) {
        hud_draw_timer(r->time_remaining, r->time_total);
    }

    /* Draw minimap overlay when darkness is active (not affected by screen shake) */
    if (r->darkness_enabled) {
        hud_draw_minimap(&r->map, players, num_players);
    }

    /* Decrement screen shake after rendering (seg_1010:7724).
     * When shake reaches 1, the next frame resets CRTC to 0 (normal). */
    if (r->map.screen_shake > 0) {
        r->map.screen_shake--;
    }

    /* Palette flash: white overlay for mine detonations (seg_1010:1213).
     * The original calls set_palette_to_white() then set_palette() to restore.
     * We render a white rectangle for the flash duration. */
    if (r->map.palette_flash > 0) {
        DrawRectangle(0, 0, 640, 480, WHITE);
        r->map.palette_flash--;
    }

    /* Pause overlay: darken screen to ~50% brightness (seg_1000:4513-4516).
     * Original shifts all palette entries right by 1 bit. */
    if (r->paused) {
        DrawRectangle(0, 0, 640, 480, (Color){0, 0, 0, 128});
    }

    /* Fade overlay: black rectangle with alpha for fade-in/fade-out.
     * Original uses palette_fade_in/out(7, ...) which linearly interpolates
     * all palette entries. We approximate with a screen overlay.
     * fade_step goes 0..ROUND_FADE_STEPS.
     * Fade-in: alpha = 255 * (1 - step/steps)  [starts opaque, ends transparent]
     * Fade-out: alpha = 255 * (step/steps)      [starts transparent, ends opaque] */
    if (r->state == ROUND_FADE_IN || r->state == ROUND_FADE_OUT) {
        int alpha;
        if (r->state == ROUND_FADE_IN) {
            alpha = 255 - (255 * r->fade_step / ROUND_FADE_STEPS);
        } else {
            alpha = 255 * r->fade_step / ROUND_FADE_STEPS;
        }
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        DrawRectangle(0, 0, 640, 480, (Color){0, 0, 0, (unsigned char)alpha});
    }
}

/*
 * Round-end scoring — port of FUN_1000_a17c (seg_1000:6459-6640).
 * Re-derived 2026-06-10 from the decompile plus the raw MB.EXE code bytes
 * (file offsets 45436-46251).
 *
 * Original semantics:
 *   SP (num_players < 2): wallet += this round's earnings. Nothing else.
 *   MP: dead players forfeit this round's earnings into a pool. If exactly
 *   one player is still alive, the pool also gets Trunc(value of treasure
 *   left on the map / 2.5). Every survivor receives pool/survivors plus
 *   their own earnings into the wallet, and round_wins++ — but the win is
 *   only counted when at least one player died (survivors < num_players).
 *   Then a welfare floor (MP only): any wallet below 100 gets +150 added.
 *
 * Runs unconditionally at round end in both modes, even on ESC abort
 * (seg_1000:7310 is inside the unconditional post-round block).
 * `earned` is NOT cleared here — the original zeroes it at the next round
 * start (game_state_update, seg_1010:7428), mirrored by
 * player_reset_for_round. A dead player's wallet is untouched: they keep
 * banked cash and lose only this round's pickups.
 *
 * The original iterates all 4 player slots gated only on the dead flag
 * (inactive slots are reset alive with earned == 0 each round, so they
 * silently receive pool shares/wins/floor). Those slots are never
 * displayed, ranked, or saved, so iterating only active players here is
 * observably identical.
 */
void round_apply_scoring(Player players[], int num_players, const TileMap *map)
{
    /* FUN_1000_a17c also accumulates into every player's match-stats
     * block, both modes: +0x08 (dword 2) += 1 round played, +0x14
     * (dword 5) += this round's earned (MB.EXE bytes 46068-46196). */
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        players[i].match_stats[STAT_ROUNDS] += 1;
        if (players[i].earned > 0) {
            players[i].match_stats[STAT_MONEY] += (uint32_t)players[i].earned;
        }
    }

    if (num_players < 2) {
        players[0].cash += players[0].earned;
        return;
    }

    int32_t pool = 0;
    int survivors = 0;
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (players[i].dead) {
            pool += players[i].earned;
        } else {
            survivors++;
        }
    }

    /* Sole survivor: bonus = Trunc(remaining treasure value / 2.5).
     * The original divides by the Real48 constant 2.5 (MB.EXE bytes at
     * 45599: CX:SI:DI = 82 00 / 00 00 / 00 20) and truncates; for the
     * non-negative sums involved, (v * 2) / 5 is exact. */
    if (survivors == 1) {
        pool += map_treasure_value_remaining(map) * 2 / 5;
    }

    if (survivors > 0) {
        int32_t share = pool / survivors;
        for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
            if (!players[i].dead) {
                players[i].cash += share + players[i].earned;
                if (survivors < num_players) {
                    players[i].round_wins++;
                }
            }
        }
    }

    /* Welfare floor: wallets below 100 get +150 ADDED (not set to 150).
     * MB.EXE bytes at 45897-45934: cmp [wallet_hi],0 / cmp [wallet_lo],100
     * (signed 32-bit compare) then add ax,150 / adc dx,0. */
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        if (players[i].cash < CASH_FLOOR_THRESHOLD) {
            players[i].cash += CASH_FLOOR_BONUS;
        }
    }
}

/*
 * 7% savings interest — port of FUN_1010_ceb3 (seg_1010:7628, MB.EXE bytes
 * at file offset 130995-131058). HISTORIA.TXT v3.1: "Säästetty raha kasvaa
 * korkoa (7%)" (saved money earns 7% interest).
 *
 * Original: wallet := Round(wallet * 1.07) using Turbo Pascal Real48
 * arithmetic. The stored constant (MB.EXE @131023: 81 5C 8F C2 F5 08) is
 * the Real48 nearest to 1.07 = 1 + 0x08F5C28F5C/2^39; RTL entry 1551 is
 * RRound (nearest, half away from zero — TP Round semantics).
 *
 * Port computes in double: for every wallet value where the Real48 product
 * lands clear of a .5 boundary this is exact; at exact-half boundaries
 * (wallet ≡ 50 mod 100, e.g. 150 → 160.5) the result depends on the TP
 * software RMUL's mantissa rounding, which is unverified — double+llround
 * rounds these up (150 → 161); exact-half behavior awaits
 * DOSBox verification.
 */
void round_apply_interest(Player players[], int num_players)
{
    for (int i = 0; i < num_players && i < MAX_PLAYERS; i++) {
        players[i].cash =
            (int32_t)llround((double)players[i].cash * 1.07);
    }
}

void round_cleanup(Round *r)
{
    entities_cleanup(&r->entity_head);
    map_renderer_set_darkness(false);
    memset(r, 0, sizeof(Round));
}
