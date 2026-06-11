#include "key_config.h"
#include "loaders/spy_loader.h"
#include "loaders/font_loader.h"
#include "gfx/palette.h"
#include "input/input.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FADE_STEPS  7
#define NUM_PLAYERS 4
#define NUM_ACTIONS 8

/* Debounce: wait this many frames after each capture before accepting next input */
#define DEBOUNCE_FRAMES 6

/* Layout from decompiled key_config_screen (seg_1010:7731):
 * Text Y = (player * 8 + 10 + action_row) * 10
 * Label X = 180 (0xB4), Key name X = 356 (0x164)
 * 3-pass shadow text: (color 12, X-1), (color 4, X+1), (color 8, X) */
#define LABEL_X     180
#define KEY_NAME_X  356

/* Capture order matches original FUN_1010_d4c4 (seg_1010:7894):
 * offsets 0xF3=Left, 0xF4=Right, 0xF5=Up, 0xF6=Down,
 * 0xF7=Stop, 0xF8=Bomb, 0xFA=Remote, 0xF9=Choose/Sell */
static const PlayerInputAction capture_order[NUM_ACTIONS] = {
    PLAYER_INPUT_LEFT,
    PLAYER_INPUT_RIGHT,
    PLAYER_INPUT_UP,
    PLAYER_INPUT_DOWN,
    PLAYER_INPUT_STOP,
    PLAYER_INPUT_BOMB,
    PLAYER_INPUT_REMOTE,
    PLAYER_INPUT_CYCLE,
};

static const char *action_labels[NUM_ACTIONS] = {
    "Left",
    "Right",
    "Up",
    "Down",
    "Stop",
    "Bomb/Buy",
    "Remote",
    "Choose/Sell",
};

typedef enum {
    KC_FADE_IN,
    KC_CAPTURING,
    KC_FADE_OUT,
    KC_FINISHED
} KCState;

/* Background */
static Image bg_img;
static Texture2D bg_tex;
static uint8_t bg_palette[768];
static uint8_t *bg_indexed;

/* Font */
static BitmapFont font;

/* State */
static KCState state;
static int capture_player;    /* 0-3 */
static int capture_action;    /* 0-7 (index into capture_order) */
static int debounce_counter;  /* frames remaining before accepting input */

/* Temp buffer for bindings during capture */
static InputBinding temp_bindings[NUM_PLAYERS][NUM_ACTIONS];

static int text_y(int player, int action_row)
{
    return (player * 8 + 10 + action_row) * 10;
}

static void draw_shadow_text(const char *text, int x, int y)
{
    /* 3-pass shadow matching decompiled: color 12 at X-1, color 4 at X+1, color 8 at X */
    Color c12 = palette_get_color(12);
    Color c4  = palette_get_color(4);
    Color c8  = palette_get_color(8);
    DrawTextFON(&font, text, x - 1, y, c12);
    DrawTextFON(&font, text, x + 1, y, c4);
    DrawTextFON(&font, text, x, y, c8);
}

static void copy_bindings_from_system(void)
{
    for (int p = 0; p < NUM_PLAYERS; p++) {
        for (int a = 0; a < NUM_ACTIONS; a++) {
            temp_bindings[p][a] = player_input_get_binding(p, capture_order[a]);
        }
    }
}

static void apply_bindings_to_system(void)
{
    for (int p = 0; p < NUM_PLAYERS; p++) {
        for (int a = 0; a < NUM_ACTIONS; a++) {
            player_input_set_binding(p, capture_order[a], temp_bindings[p][a]);
        }
    }
}

void key_config_init(void)
{
    bg_indexed = malloc(SPY_WIDTH * SPY_HEIGHT);
    bg_img = LoadSPY("assets/KEYS.SPY", bg_palette, bg_indexed);
    bg_tex = LoadTextureFromImage(bg_img);

    font = LoadFON("assets/FONTTI.FON", true);

    palette_init(bg_palette);
    palette_start_fade_in(FADE_STEPS);

    capture_player = 0;
    capture_action = 0;
    debounce_counter = 0;
    state = KC_FADE_IN;

    copy_bindings_from_system();
}

/* Scan for any gamepad button press on the given gamepad index.
 * Returns the button code (0-17), or -1 if none pressed. */
static int scan_gamepad_button(int pad)
{
    if (!IsGamepadAvailable(pad)) return -1;
    for (int b = 0; b <= 17; b++) {
        if (IsGamepadButtonPressed(pad, b)) return b;
    }
    return -1;
}

KeyConfigResult key_config_update(void)
{
    switch (state) {
    case KC_FADE_IN:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = KC_CAPTURING;
            debounce_counter = DEBOUNCE_FRAMES;
        }
        break;

    case KC_CAPTURING:
        /* Debounce: wait for release + frame delay */
        if (debounce_counter > 0) {
            debounce_counter--;
            /* Drain key queue during debounce */
            while (GetKeyPressed() != 0) {}
            break;
        }

        /* F10 abort: original (seg_1010:7968-7971) stops asking for more bindings
         * but still saves whatever was captured so far (no discard). */
        if (IsKeyPressed(KEY_F10)) {
            apply_bindings_to_system();
            player_input_save("assets/keybinds.dat");
            palette_start_fade_out(FADE_STEPS);
            state = KC_FADE_OUT;
            break;
        }

        /* ESC = skip this binding (keep existing) */
        if (IsKeyPressed(KEY_ESCAPE)) {
            /* Advance to next */
            capture_action++;
            if (capture_action >= NUM_ACTIONS) {
                capture_action = 0;
                capture_player++;
            }
            if (capture_player >= NUM_PLAYERS) {
                /* All done */
                apply_bindings_to_system();
                player_input_save("assets/keybinds.dat");
                palette_start_fade_out(FADE_STEPS);
                state = KC_FADE_OUT;
            } else {
                debounce_counter = DEBOUNCE_FRAMES;
            }
            break;
        }

        {
            /* Scan keyboard */
            int key = GetKeyPressed();
            if (key != 0 && key != KEY_F10 && key != KEY_ESCAPE && key != KEY_F12) {
                temp_bindings[capture_player][capture_action] =
                    (InputBinding){BIND_KEYBOARD, key};
                goto advance;
            }

            /* Scan gamepad for current player */
            int btn = scan_gamepad_button(capture_player);
            if (btn >= 0) {
                temp_bindings[capture_player][capture_action] =
                    (InputBinding){BIND_GAMEPAD, btn};
                goto advance;
            }
        }
        break;

    advance:
        capture_action++;
        if (capture_action >= NUM_ACTIONS) {
            capture_action = 0;
            capture_player++;
        }
        if (capture_player >= NUM_PLAYERS) {
            apply_bindings_to_system();
            player_input_save("assets/keybinds.dat");
            palette_start_fade_out(FADE_STEPS);
            state = KC_FADE_OUT;
        } else {
            debounce_counter = DEBOUNCE_FRAMES;
        }
        break;

    case KC_FADE_OUT:
        palette_update();
        palette_apply_to_pixels(bg_indexed, (uint8_t *)bg_img.data, SPY_WIDTH * SPY_HEIGHT);
        UpdateTexture(bg_tex, bg_img.data);
        if (!palette_is_fading()) {
            state = KC_FINISHED;
            return KEY_CONFIG_DONE;
        }
        break;

    case KC_FINISHED:
        return KEY_CONFIG_DONE;
    }

    return KEY_CONFIG_ACTIVE;
}

void key_config_draw(void)
{
    DrawTexture(bg_tex, 0, 0, WHITE);

    if (state == KC_FADE_IN && palette_is_fading()) return;

    /* Draw all player bindings.
     * Original format: "Player N  ActionLabel" on each line, key name at X=356. */
    for (int p = 0; p < NUM_PLAYERS; p++) {
        for (int a = 0; a < NUM_ACTIONS; a++) {
            int y = text_y(p, a);

            /* "Player N  ActionLabel" */
            char label[48];
            snprintf(label, sizeof(label), "Player %d  %s", p + 1, action_labels[a]);
            draw_shadow_text(label, LABEL_X, y);

            /* Key name */
            const char *name;
            if (state == KC_CAPTURING && p == capture_player && a == capture_action) {
                name = "???";
            } else {
                name = input_binding_name(temp_bindings[p][a]);
            }
            draw_shadow_text(name, KEY_NAME_X, y);
        }
    }
}

void key_config_cleanup(void)
{
    if (bg_tex.id) UnloadTexture(bg_tex);
    if (bg_img.data) UnloadImage(bg_img);
    free(bg_indexed);
    bg_indexed = NULL;
    bg_tex = (Texture2D){0};
    bg_img = (Image){0};

    UnloadFON(&font);
}
