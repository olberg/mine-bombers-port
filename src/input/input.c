#include "input.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GAMEPAD_DEADZONE 0.5f

static bool pressed[INPUT_ACTION_COUNT];

/* Check if any key was pressed this frame (excluding F12 = screenshot) */
static bool any_key_pressed(void)
{
    int key = GetKeyPressed();
    if (key != 0 && key != KEY_F12) return true;

    /* Check gamepad buttons */
    if (IsGamepadAvailable(0)) {
        for (int b = 0; b < 16; b++) {
            if (IsGamepadButtonPressed(0, b)) return true;
        }
    }
    return false;
}

void input_update(void)
{
    /* Keyboard - pressed */
    pressed[INPUT_UP] = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_KP_8);
    pressed[INPUT_DOWN] = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_KP_2);
    pressed[INPUT_LEFT] = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_KP_4);
    pressed[INPUT_RIGHT] = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_KP_6);
    pressed[INPUT_CONFIRM] = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_KP_3);
    pressed[INPUT_CANCEL] = IsKeyPressed(KEY_ESCAPE);
    pressed[INPUT_QUIT] = IsKeyPressed(KEY_F10);

    /* Gamepad */
    if (IsGamepadAvailable(0)) {
        /* D-pad pressed */
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
            pressed[INPUT_UP] = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
            pressed[INPUT_DOWN] = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
            pressed[INPUT_LEFT] = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
            pressed[INPUT_RIGHT] = true;

        /* Buttons pressed */
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))  /* A/South */
            pressed[INPUT_CONFIRM] = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) /* B/East */
            pressed[INPUT_CANCEL] = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))     /* Start */
            pressed[INPUT_QUIT] = true;
    }

    /* Any key */
    pressed[INPUT_ANY_KEY] = any_key_pressed();
}

bool input_pressed(InputAction action)
{
    if (action < 0 || action >= INPUT_ACTION_COUNT) return false;
    return pressed[action];
}

/* --- Per-player input --- */

#define MAX_PLAYERS_INPUT 4

static InputBinding player_bindings[MAX_PLAYERS_INPUT][PLAYER_INPUT_COUNT];

/* Default bindings re-derived from FUN_1010_9fbb's fallback
 * scancodes (seg_1010:5690-5721) with key identities resolved against the
 * scancode-name string table inside MB.EXE itself (standard table at file
 * offset 155819, 19-byte stride; extended-key cases of FUN_1018_2652).
 * Notable corrections vs. the older input-system.md table:
 *   - 0xB5 is PAGEDOWN (not Numpad /), 0x9C is RIGHT ALT (not Num Enter).
 *   - +0xF9 = Remote, +0xFA = Choose/Sell (the config screen lists
 *     Choose/Sell BEFORE Remote; older docs had the pair swapped).
 * Scancode 0x29 (P2 Choose, P4 Stop — the original ships this conflict)
 * is the key left of '1' (Finnish §): KEY_GRAVE is that physical position. */
void player_input_init_defaults(void)
{
    /* Player 1: Numpad + nav cluster */
    player_bindings[0][PLAYER_INPUT_UP]     = (InputBinding){BIND_KEYBOARD, KEY_KP_8};       /* 0x48 NP_UP */
    player_bindings[0][PLAYER_INPUT_DOWN]   = (InputBinding){BIND_KEYBOARD, KEY_KP_2};       /* 0x50 NP_DOWN */
    player_bindings[0][PLAYER_INPUT_LEFT]   = (InputBinding){BIND_KEYBOARD, KEY_KP_4};       /* 0x4B NP_LEFT */
    player_bindings[0][PLAYER_INPUT_RIGHT]  = (InputBinding){BIND_KEYBOARD, KEY_KP_6};       /* 0x4D NP_RIGHT */
    player_bindings[0][PLAYER_INPUT_STOP]   = (InputBinding){BIND_KEYBOARD, KEY_KP_5};       /* 0x4C NP_5 */
    player_bindings[0][PLAYER_INPUT_BOMB]   = (InputBinding){BIND_KEYBOARD, KEY_PAGE_DOWN};  /* 0xB5 PAGEDOWN */
    player_bindings[0][PLAYER_INPUT_REMOTE] = (InputBinding){BIND_KEYBOARD, KEY_KP_1};       /* 0x4F NP_END */
    player_bindings[0][PLAYER_INPUT_CYCLE]  = (InputBinding){BIND_KEYBOARD, KEY_PAGE_UP};    /* 0xAD PAGEUP */

    /* Player 2: WASD area */
    player_bindings[1][PLAYER_INPUT_UP]     = (InputBinding){BIND_KEYBOARD, KEY_W};          /* 0x11 */
    player_bindings[1][PLAYER_INPUT_DOWN]   = (InputBinding){BIND_KEYBOARD, KEY_X};          /* 0x2D */
    player_bindings[1][PLAYER_INPUT_LEFT]   = (InputBinding){BIND_KEYBOARD, KEY_A};          /* 0x1E */
    player_bindings[1][PLAYER_INPUT_RIGHT]  = (InputBinding){BIND_KEYBOARD, KEY_D};          /* 0x20 */
    player_bindings[1][PLAYER_INPUT_STOP]   = (InputBinding){BIND_KEYBOARD, KEY_S};          /* 0x1F */
    player_bindings[1][PLAYER_INPUT_BOMB]   = (InputBinding){BIND_KEYBOARD, KEY_TAB};        /* 0x0F */
    player_bindings[1][PLAYER_INPUT_REMOTE] = (InputBinding){BIND_KEYBOARD, KEY_Z};          /* 0x2C */
    player_bindings[1][PLAYER_INPUT_CYCLE]  = (InputBinding){BIND_KEYBOARD, KEY_GRAVE};      /* 0x29 */

    /* Player 3: OKL area */
    player_bindings[2][PLAYER_INPUT_UP]     = (InputBinding){BIND_KEYBOARD, KEY_O};          /* 0x18 */
    player_bindings[2][PLAYER_INPUT_DOWN]   = (InputBinding){BIND_KEYBOARD, KEY_PERIOD};     /* 0x34 */
    player_bindings[2][PLAYER_INPUT_LEFT]   = (InputBinding){BIND_KEYBOARD, KEY_K};          /* 0x25 */
    player_bindings[2][PLAYER_INPUT_RIGHT]  = (InputBinding){BIND_KEYBOARD, KEY_SEMICOLON};  /* 0x27 */
    player_bindings[2][PLAYER_INPUT_STOP]   = (InputBinding){BIND_KEYBOARD, KEY_L};          /* 0x26 */
    player_bindings[2][PLAYER_INPUT_BOMB]   = (InputBinding){BIND_KEYBOARD, KEY_I};          /* 0x17 */
    player_bindings[2][PLAYER_INPUT_REMOTE] = (InputBinding){BIND_KEYBOARD, KEY_COMMA};      /* 0x33 */
    player_bindings[2][PLAYER_INPUT_CYCLE]  = (InputBinding){BIND_KEYBOARD, KEY_EIGHT};      /* 0x09 */

    /* Player 4: Arrow keys + right-hand modifiers */
    player_bindings[3][PLAYER_INPUT_UP]     = (InputBinding){BIND_KEYBOARD, KEY_UP};            /* 0xAC */
    player_bindings[3][PLAYER_INPUT_DOWN]   = (InputBinding){BIND_KEYBOARD, KEY_DOWN};          /* 0xB4 */
    player_bindings[3][PLAYER_INPUT_LEFT]   = (InputBinding){BIND_KEYBOARD, KEY_LEFT};          /* 0xAF */
    player_bindings[3][PLAYER_INPUT_RIGHT]  = (InputBinding){BIND_KEYBOARD, KEY_RIGHT};         /* 0xB1 */
    player_bindings[3][PLAYER_INPUT_STOP]   = (InputBinding){BIND_KEYBOARD, KEY_GRAVE};         /* 0x29 (= P2 Choose!) */
    player_bindings[3][PLAYER_INPUT_BOMB]   = (InputBinding){BIND_KEYBOARD, KEY_RIGHT_CONTROL}; /* 0x81 */
    player_bindings[3][PLAYER_INPUT_REMOTE] = (InputBinding){BIND_KEYBOARD, KEY_RIGHT_ALT};     /* 0x9C */
    player_bindings[3][PLAYER_INPUT_CYCLE]  = (InputBinding){BIND_KEYBOARD, KEY_RIGHT_SHIFT};   /* 0x36 */
}

void player_input_set_key(int player_idx, PlayerInputAction action, int raylib_key)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return;
    if (action < 0 || action >= PLAYER_INPUT_COUNT) return;
    player_bindings[player_idx][action] = (InputBinding){BIND_KEYBOARD, raylib_key};
}

void player_input_set_binding(int player_idx, PlayerInputAction action, InputBinding bind)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return;
    if (action < 0 || action >= PLAYER_INPUT_COUNT) return;
    player_bindings[player_idx][action] = bind;
}

InputBinding player_input_get_binding(int player_idx, PlayerInputAction action)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT)
        return (InputBinding){BIND_KEYBOARD, 0};
    if (action < 0 || action >= PLAYER_INPUT_COUNT)
        return (InputBinding){BIND_KEYBOARD, 0};
    return player_bindings[player_idx][action];
}

int player_input_get_key(int player_idx, PlayerInputAction action)
{
    InputBinding b = player_input_get_binding(player_idx, action);
    if (b.type == BIND_KEYBOARD) return b.code;
    return 0;
}

/* --- Scripted input injection (autoplay harness) --- */

static bool inject_mode;
static bool inject_down[MAX_PLAYERS_INPUT][PLAYER_INPUT_COUNT];
static bool inject_press[MAX_PLAYERS_INPUT][PLAYER_INPUT_COUNT];

void player_input_inject_mode(bool enabled)
{
    inject_mode = enabled;
    memset(inject_down, 0, sizeof(inject_down));
    memset(inject_press, 0, sizeof(inject_press));
}

void player_input_inject_clear(int player_idx)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return;
    memset(inject_down[player_idx], 0, sizeof(inject_down[player_idx]));
    memset(inject_press[player_idx], 0, sizeof(inject_press[player_idx]));
}

void player_input_inject(int player_idx, PlayerInputAction action,
                         bool down, bool pressed)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return;
    if (action < 0 || action >= PLAYER_INPUT_COUNT) return;
    inject_down[player_idx][action] = down;
    inject_press[player_idx][action] = pressed;
}

bool player_input_down(int player_idx, PlayerInputAction action)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return false;
    if (action < 0 || action >= PLAYER_INPUT_COUNT) return false;

    if (inject_mode) return inject_down[player_idx][action];

    InputBinding b = player_bindings[player_idx][action];

    bool result = false;
    if (b.type == BIND_KEYBOARD) {
        result = IsKeyDown(b.code);
    } else if (b.type == BIND_GAMEPAD) {
        if (IsGamepadAvailable(player_idx))
            result = IsGamepadButtonDown(player_idx, b.code);
    }

    /* For direction actions, also check left analog stick on this player's gamepad */
    if (!result && action >= PLAYER_INPUT_UP && action <= PLAYER_INPUT_RIGHT
        && IsGamepadAvailable(player_idx)) {
        float ax = GetGamepadAxisMovement(player_idx, GAMEPAD_AXIS_LEFT_X);
        float ay = GetGamepadAxisMovement(player_idx, GAMEPAD_AXIS_LEFT_Y);
        switch (action) {
        case PLAYER_INPUT_UP:    result = (ay < -GAMEPAD_DEADZONE); break;
        case PLAYER_INPUT_DOWN:  result = (ay >  GAMEPAD_DEADZONE); break;
        case PLAYER_INPUT_LEFT:  result = (ax < -GAMEPAD_DEADZONE); break;
        case PLAYER_INPUT_RIGHT: result = (ax >  GAMEPAD_DEADZONE); break;
        default: break;
        }
    }

    return result;
}

bool player_input_pressed(int player_idx, PlayerInputAction action)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return false;
    if (action < 0 || action >= PLAYER_INPUT_COUNT) return false;

    if (inject_mode) return inject_press[player_idx][action];

    InputBinding b = player_bindings[player_idx][action];

    if (b.type == BIND_KEYBOARD) {
        /* Include OS key repeat: the original's one-shot keys (bomb, choose)
         * clear the ISR key-state byte after consuming it, but the keyboard's
         * typematic repeat sets it again while the key stays held — so
         * holding bomb re-places at the typematic rate (seg_1000:2631-2632).
         * IsKeyPressedRepeat approximates that with the host repeat rate.
         * Gamepads stay edge-only: the original's joystick virtual scancodes
         * were polled levels with no typematic. */
        return IsKeyPressed(b.code) || IsKeyPressedRepeat(b.code);
    } else if (b.type == BIND_GAMEPAD) {
        if (IsGamepadAvailable(player_idx))
            return IsGamepadButtonPressed(player_idx, b.code);
    }

    return false;
}

bool player_input_edge(int player_idx, PlayerInputAction action)
{
    if (player_idx < 0 || player_idx >= MAX_PLAYERS_INPUT) return false;
    if (action < 0 || action >= PLAYER_INPUT_COUNT) return false;

    if (inject_mode) return inject_press[player_idx][action];

    InputBinding b = player_bindings[player_idx][action];

    if (b.type == BIND_KEYBOARD) {
        return IsKeyPressed(b.code);
    } else if (b.type == BIND_GAMEPAD) {
        if (IsGamepadAvailable(player_idx))
            return IsGamepadButtonPressed(player_idx, b.code);
    }

    return false;
}

/* --- Key name table --- */

typedef struct {
    int code;
    const char *name;
} KeyName;

static const KeyName keyboard_names[] = {
    {KEY_SPACE, "Space"},
    {KEY_APOSTROPHE, "'"},
    {KEY_COMMA, ","},
    {KEY_MINUS, "-"},
    {KEY_PERIOD, "."},
    {KEY_SLASH, "/"},
    {KEY_ZERO, "0"}, {KEY_ONE, "1"}, {KEY_TWO, "2"}, {KEY_THREE, "3"},
    {KEY_FOUR, "4"}, {KEY_FIVE, "5"}, {KEY_SIX, "6"}, {KEY_SEVEN, "7"},
    {KEY_EIGHT, "8"}, {KEY_NINE, "9"},
    {KEY_SEMICOLON, ";"},
    {KEY_EQUAL, "="},
    {KEY_A, "A"}, {KEY_B, "B"}, {KEY_C, "C"}, {KEY_D, "D"},
    {KEY_E, "E"}, {KEY_F, "F"}, {KEY_G, "G"}, {KEY_H, "H"},
    {KEY_I, "I"}, {KEY_J, "J"}, {KEY_K, "K"}, {KEY_L, "L"},
    {KEY_M, "M"}, {KEY_N, "N"}, {KEY_O, "O"}, {KEY_P, "P"},
    {KEY_Q, "Q"}, {KEY_R, "R"}, {KEY_S, "S"}, {KEY_T, "T"},
    {KEY_U, "U"}, {KEY_V, "V"}, {KEY_W, "W"}, {KEY_X, "X"},
    {KEY_Y, "Y"}, {KEY_Z, "Z"},
    {KEY_LEFT_BRACKET, "["}, {KEY_BACKSLASH, "\\"},
    {KEY_RIGHT_BRACKET, "]"}, {KEY_GRAVE, "`"},
    {KEY_ESCAPE, "Esc"},
    {KEY_ENTER, "Enter"},
    {KEY_TAB, "Tab"},
    {KEY_BACKSPACE, "Backspace"},
    {KEY_INSERT, "Insert"},
    {KEY_DELETE, "Delete"},
    {KEY_RIGHT, "Right"},
    {KEY_LEFT, "Left"},
    {KEY_DOWN, "Down"},
    {KEY_UP, "Up"},
    {KEY_PAGE_UP, "Page Up"},
    {KEY_PAGE_DOWN, "Page Down"},
    {KEY_HOME, "Home"},
    {KEY_END, "End"},
    {KEY_CAPS_LOCK, "Caps Lock"},
    {KEY_SCROLL_LOCK, "Scroll Lock"},
    {KEY_NUM_LOCK, "Num Lock"},
    {KEY_PRINT_SCREEN, "Print Scr"},
    {KEY_PAUSE, "Pause"},
    {KEY_F1, "F1"}, {KEY_F2, "F2"}, {KEY_F3, "F3"}, {KEY_F4, "F4"},
    {KEY_F5, "F5"}, {KEY_F6, "F6"}, {KEY_F7, "F7"}, {KEY_F8, "F8"},
    {KEY_F9, "F9"}, {KEY_F10, "F10"}, {KEY_F11, "F11"}, {KEY_F12, "F12"},
    {KEY_KP_0, "Num 0"}, {KEY_KP_1, "Num 1"}, {KEY_KP_2, "Num 2"},
    {KEY_KP_3, "Num 3"}, {KEY_KP_4, "Num 4"}, {KEY_KP_5, "Num 5"},
    {KEY_KP_6, "Num 6"}, {KEY_KP_7, "Num 7"}, {KEY_KP_8, "Num 8"},
    {KEY_KP_9, "Num 9"},
    {KEY_KP_DECIMAL, "Num ."},
    {KEY_KP_DIVIDE, "Num /"},
    {KEY_KP_MULTIPLY, "Num *"},
    {KEY_KP_SUBTRACT, "Num -"},
    {KEY_KP_ADD, "Num +"},
    {KEY_KP_ENTER, "Num Enter"},
    {KEY_KP_EQUAL, "Num ="},
    {KEY_LEFT_SHIFT, "L Shift"},
    {KEY_LEFT_CONTROL, "L Ctrl"},
    {KEY_LEFT_ALT, "L Alt"},
    {KEY_LEFT_SUPER, "L Super"},
    {KEY_RIGHT_SHIFT, "R Shift"},
    {KEY_RIGHT_CONTROL, "R Ctrl"},
    {KEY_RIGHT_ALT, "R Alt"},
    {KEY_RIGHT_SUPER, "R Super"},
    {KEY_KB_MENU, "Menu"},
    {0, NULL}
};

static const char *gamepad_names[] = {
    "Pad ?",         /*  0 = GAMEPAD_BUTTON_UNKNOWN */
    "Pad Up",        /*  1 = GAMEPAD_BUTTON_LEFT_FACE_UP */
    "Pad Right",     /*  2 = GAMEPAD_BUTTON_LEFT_FACE_RIGHT */
    "Pad Down",      /*  3 = GAMEPAD_BUTTON_LEFT_FACE_DOWN */
    "Pad Left",      /*  4 = GAMEPAD_BUTTON_LEFT_FACE_LEFT */
    "Pad Y",         /*  5 = GAMEPAD_BUTTON_RIGHT_FACE_UP */
    "Pad B",         /*  6 = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT */
    "Pad A",         /*  7 = GAMEPAD_BUTTON_RIGHT_FACE_DOWN */
    "Pad X",         /*  8 = GAMEPAD_BUTTON_RIGHT_FACE_LEFT */
    "LB",            /*  9 = GAMEPAD_BUTTON_LEFT_TRIGGER_1 */
    "LT",            /* 10 = GAMEPAD_BUTTON_LEFT_TRIGGER_2 */
    "RB",            /* 11 = GAMEPAD_BUTTON_RIGHT_TRIGGER_1 */
    "RT",            /* 12 = GAMEPAD_BUTTON_RIGHT_TRIGGER_2 */
    "Select",        /* 13 = GAMEPAD_BUTTON_MIDDLE_LEFT */
    "Guide",         /* 14 = GAMEPAD_BUTTON_MIDDLE */
    "Start",         /* 15 = GAMEPAD_BUTTON_MIDDLE_RIGHT */
    "L3",            /* 16 = GAMEPAD_BUTTON_LEFT_THUMB */
    "R3",            /* 17 = GAMEPAD_BUTTON_RIGHT_THUMB */
};

#define GAMEPAD_NAME_COUNT (int)(sizeof(gamepad_names) / sizeof(gamepad_names[0]))

static char unknown_buf[16];

const char *input_binding_name(InputBinding bind)
{
    if (bind.type == BIND_GAMEPAD) {
        if (bind.code >= 0 && bind.code < GAMEPAD_NAME_COUNT)
            return gamepad_names[bind.code];
        snprintf(unknown_buf, sizeof(unknown_buf), "Pad %d", bind.code);
        return unknown_buf;
    }

    /* Keyboard */
    for (int i = 0; keyboard_names[i].name != NULL; i++) {
        if (keyboard_names[i].code == bind.code)
            return keyboard_names[i].name;
    }

    snprintf(unknown_buf, sizeof(unknown_buf), "Key %d", bind.code);
    return unknown_buf;
}

/* --- Save/Load --- */

/* Binary format: 4 players * 8 actions * 2 ints (type, code) = 64 ints */
#define BINDINGS_FILE_INTS (MAX_PLAYERS_INPUT * PLAYER_INPUT_COUNT * 2)

bool player_input_save(const char *path)
{
    int data[BINDINGS_FILE_INTS];
    int idx = 0;
    for (int p = 0; p < MAX_PLAYERS_INPUT; p++) {
        for (int a = 0; a < PLAYER_INPUT_COUNT; a++) {
            data[idx++] = (int)player_bindings[p][a].type;
            data[idx++] = player_bindings[p][a].code;
        }
    }
    return SaveFileData(path, data, sizeof(data));
}

bool player_input_load(const char *path)
{
    int size = 0;
    unsigned char *raw = LoadFileData(path, &size);
    if (!raw) return false;

    if (size != BINDINGS_FILE_INTS * (int)sizeof(int)) {
        UnloadFileData(raw);
        return false;
    }

    int *data = (int *)raw;
    int idx = 0;
    for (int p = 0; p < MAX_PLAYERS_INPUT; p++) {
        for (int a = 0; a < PLAYER_INPUT_COUNT; a++) {
            int type = data[idx++];
            int code = data[idx++];
            if (type == BIND_KEYBOARD || type == BIND_GAMEPAD) {
                player_bindings[p][a].type = (InputBindType)type;
                player_bindings[p][a].code = code;
            }
        }
    }

    UnloadFileData(raw);
    return true;
}
