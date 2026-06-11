#include "unity.h"
#include "input/input.h"
#include "raylib.h"
#include <string.h>

void setUp(void) { player_input_init_defaults(); }
void tearDown(void) {}

/* Pins all 32 default bindings against the original's fallback scancodes
 * (FUN_1010_9fbb, seg_1010:5690-5721), key identities verified from the
 * scancode-name tables inside MB.EXE. Of note:
 *   - P1 Bomb is PAGE DOWN: scancode 0xB5 names as "PAGEDOWN", not Num /.
 *   - +0xF9 = Remote, +0xFA = Choose/Sell (config screen lists Choose/Sell
 *     first; older docs and the previous defaults had the pair swapped).
 *   - P4 Remote is RIGHT ALT (0x9C), not Numpad Enter. */
void test_default_bindings(void)
{
    /* Player 1: Numpad + nav cluster */
    TEST_ASSERT_EQUAL(KEY_KP_8, player_input_get_key(0, PLAYER_INPUT_UP));
    TEST_ASSERT_EQUAL(KEY_KP_2, player_input_get_key(0, PLAYER_INPUT_DOWN));
    TEST_ASSERT_EQUAL(KEY_KP_4, player_input_get_key(0, PLAYER_INPUT_LEFT));
    TEST_ASSERT_EQUAL(KEY_KP_6, player_input_get_key(0, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_EQUAL(KEY_KP_5, player_input_get_key(0, PLAYER_INPUT_STOP));
    TEST_ASSERT_EQUAL(KEY_PAGE_DOWN, player_input_get_key(0, PLAYER_INPUT_BOMB));
    TEST_ASSERT_EQUAL(KEY_KP_1, player_input_get_key(0, PLAYER_INPUT_REMOTE));
    TEST_ASSERT_EQUAL(KEY_PAGE_UP, player_input_get_key(0, PLAYER_INPUT_CYCLE));

    /* Player 2: WASD */
    TEST_ASSERT_EQUAL(KEY_W, player_input_get_key(1, PLAYER_INPUT_UP));
    TEST_ASSERT_EQUAL(KEY_X, player_input_get_key(1, PLAYER_INPUT_DOWN));
    TEST_ASSERT_EQUAL(KEY_A, player_input_get_key(1, PLAYER_INPUT_LEFT));
    TEST_ASSERT_EQUAL(KEY_D, player_input_get_key(1, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_EQUAL(KEY_S, player_input_get_key(1, PLAYER_INPUT_STOP));
    TEST_ASSERT_EQUAL(KEY_TAB, player_input_get_key(1, PLAYER_INPUT_BOMB));
    TEST_ASSERT_EQUAL(KEY_Z, player_input_get_key(1, PLAYER_INPUT_REMOTE));
    TEST_ASSERT_EQUAL(KEY_GRAVE, player_input_get_key(1, PLAYER_INPUT_CYCLE));

    /* Player 3: OKL */
    TEST_ASSERT_EQUAL(KEY_O, player_input_get_key(2, PLAYER_INPUT_UP));
    TEST_ASSERT_EQUAL(KEY_PERIOD, player_input_get_key(2, PLAYER_INPUT_DOWN));
    TEST_ASSERT_EQUAL(KEY_K, player_input_get_key(2, PLAYER_INPUT_LEFT));
    TEST_ASSERT_EQUAL(KEY_SEMICOLON, player_input_get_key(2, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_EQUAL(KEY_L, player_input_get_key(2, PLAYER_INPUT_STOP));
    TEST_ASSERT_EQUAL(KEY_I, player_input_get_key(2, PLAYER_INPUT_BOMB));
    TEST_ASSERT_EQUAL(KEY_COMMA, player_input_get_key(2, PLAYER_INPUT_REMOTE));
    TEST_ASSERT_EQUAL(KEY_EIGHT, player_input_get_key(2, PLAYER_INPUT_CYCLE));

    /* Player 4: Arrow keys + right-hand modifiers */
    TEST_ASSERT_EQUAL(KEY_UP, player_input_get_key(3, PLAYER_INPUT_UP));
    TEST_ASSERT_EQUAL(KEY_DOWN, player_input_get_key(3, PLAYER_INPUT_DOWN));
    TEST_ASSERT_EQUAL(KEY_LEFT, player_input_get_key(3, PLAYER_INPUT_LEFT));
    TEST_ASSERT_EQUAL(KEY_RIGHT, player_input_get_key(3, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_EQUAL(KEY_GRAVE, player_input_get_key(3, PLAYER_INPUT_STOP));
    TEST_ASSERT_EQUAL(KEY_RIGHT_CONTROL, player_input_get_key(3, PLAYER_INPUT_BOMB));
    TEST_ASSERT_EQUAL(KEY_RIGHT_ALT, player_input_get_key(3, PLAYER_INPUT_REMOTE));
    TEST_ASSERT_EQUAL(KEY_RIGHT_SHIFT, player_input_get_key(3, PLAYER_INPUT_CYCLE));
}

void test_default_bindings_type(void)
{
    /* All defaults should be keyboard type */
    for (int p = 0; p < 4; p++) {
        for (int a = 0; a < PLAYER_INPUT_COUNT; a++) {
            InputBinding b = player_input_get_binding(p, a);
            TEST_ASSERT_EQUAL(BIND_KEYBOARD, b.type);
            TEST_ASSERT_NOT_EQUAL(0, b.code);
        }
    }
}

void test_no_key_conflicts(void)
{
    /* Collect all key bindings */
    int all_keys[4 * PLAYER_INPUT_COUNT];
    int count = 0;

    for (int p = 0; p < 4; p++) {
        for (int a = 0; a < PLAYER_INPUT_COUNT; a++) {
            all_keys[count++] = player_input_get_key(p, a);
        }
    }

    int conflicts = 0;
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (all_keys[i] == all_keys[j] && all_keys[i] != 0) {
                conflicts++;
            }
        }
    }

    /* The original ships EXACTLY one conflict: scancode 0x29 is both
     * Player 2 Choose/Sell and Player 4 Stop (FUN_1010_9fbb defaults
     * 1dee = 1fff = 0x29). The port reproduces it as KEY_GRAVE. */
    TEST_ASSERT_EQUAL(1, conflicts);
    TEST_ASSERT_EQUAL(player_input_get_key(1, PLAYER_INPUT_CYCLE),
                      player_input_get_key(3, PLAYER_INPUT_STOP));
}

/* Simultaneous inputs across players must not interfere — each
 * player's state is read independently (the original's ISR key array is a
 * shared snapshot; the port polls per binding). Exercised through the
 * injection layer, which uses the same per-player state tables. */
void test_four_player_simultaneous_independence(void)
{
    player_input_inject_mode(true);

    /* All four players press different things in the same frame */
    player_input_inject(0, PLAYER_INPUT_UP,     true, false);
    player_input_inject(1, PLAYER_INPUT_RIGHT,  true, false);
    player_input_inject(2, PLAYER_INPUT_BOMB,   true, true);
    player_input_inject(3, PLAYER_INPUT_REMOTE, true, false);

    TEST_ASSERT_TRUE(player_input_down(0, PLAYER_INPUT_UP));
    TEST_ASSERT_TRUE(player_input_down(1, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_TRUE(player_input_down(2, PLAYER_INPUT_BOMB));
    TEST_ASSERT_TRUE(player_input_pressed(2, PLAYER_INPUT_BOMB));
    TEST_ASSERT_TRUE(player_input_down(3, PLAYER_INPUT_REMOTE));

    /* No crosstalk: nobody sees a neighbour's keys */
    TEST_ASSERT_FALSE(player_input_down(0, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_FALSE(player_input_down(1, PLAYER_INPUT_UP));
    TEST_ASSERT_FALSE(player_input_down(0, PLAYER_INPUT_BOMB));
    TEST_ASSERT_FALSE(player_input_pressed(1, PLAYER_INPUT_BOMB));
    TEST_ASSERT_FALSE(player_input_down(2, PLAYER_INPUT_REMOTE));
    TEST_ASSERT_FALSE(player_input_down(3, PLAYER_INPUT_UP));

    /* Clearing one player leaves the others held */
    player_input_inject_clear(0);
    TEST_ASSERT_FALSE(player_input_down(0, PLAYER_INPUT_UP));
    TEST_ASSERT_TRUE(player_input_down(1, PLAYER_INPUT_RIGHT));
    TEST_ASSERT_TRUE(player_input_down(3, PLAYER_INPUT_REMOTE));

    player_input_inject_mode(false);
}

void test_set_custom_key(void)
{
    player_input_set_key(0, PLAYER_INPUT_BOMB, KEY_SPACE);
    TEST_ASSERT_EQUAL(KEY_SPACE, player_input_get_key(0, PLAYER_INPUT_BOMB));

    /* Verify it's a keyboard binding */
    InputBinding b = player_input_get_binding(0, PLAYER_INPUT_BOMB);
    TEST_ASSERT_EQUAL(BIND_KEYBOARD, b.type);
    TEST_ASSERT_EQUAL(KEY_SPACE, b.code);
}

void test_set_gamepad_binding(void)
{
    InputBinding pad_a = {BIND_GAMEPAD, GAMEPAD_BUTTON_RIGHT_FACE_DOWN};
    player_input_set_binding(1, PLAYER_INPUT_BOMB, pad_a);

    InputBinding b = player_input_get_binding(1, PLAYER_INPUT_BOMB);
    TEST_ASSERT_EQUAL(BIND_GAMEPAD, b.type);
    TEST_ASSERT_EQUAL(GAMEPAD_BUTTON_RIGHT_FACE_DOWN, b.code);

    /* get_key should return 0 for gamepad bindings */
    TEST_ASSERT_EQUAL(0, player_input_get_key(1, PLAYER_INPUT_BOMB));
}

void test_binding_name_keyboard(void)
{
    InputBinding b_w = {BIND_KEYBOARD, KEY_W};
    TEST_ASSERT_EQUAL_STRING("W", input_binding_name(b_w));

    InputBinding b_num8 = {BIND_KEYBOARD, KEY_KP_8};
    TEST_ASSERT_EQUAL_STRING("Num 8", input_binding_name(b_num8));

    InputBinding b_esc = {BIND_KEYBOARD, KEY_ESCAPE};
    TEST_ASSERT_EQUAL_STRING("Esc", input_binding_name(b_esc));

    InputBinding b_tab = {BIND_KEYBOARD, KEY_TAB};
    TEST_ASSERT_EQUAL_STRING("Tab", input_binding_name(b_tab));

    InputBinding b_rctrl = {BIND_KEYBOARD, KEY_RIGHT_CONTROL};
    TEST_ASSERT_EQUAL_STRING("R Ctrl", input_binding_name(b_rctrl));
}

void test_binding_name_gamepad(void)
{
    InputBinding b_a = {BIND_GAMEPAD, GAMEPAD_BUTTON_RIGHT_FACE_DOWN};
    TEST_ASSERT_EQUAL_STRING("Pad A", input_binding_name(b_a));

    InputBinding b_start = {BIND_GAMEPAD, GAMEPAD_BUTTON_MIDDLE_RIGHT};
    TEST_ASSERT_EQUAL_STRING("Start", input_binding_name(b_start));

    InputBinding b_lb = {BIND_GAMEPAD, GAMEPAD_BUTTON_LEFT_TRIGGER_1};
    TEST_ASSERT_EQUAL_STRING("LB", input_binding_name(b_lb));
}

void test_save_load_bindings(void)
{
    /* Set some custom bindings */
    InputBinding pad_b = {BIND_GAMEPAD, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT};
    player_input_set_binding(0, PLAYER_INPUT_BOMB, pad_b);
    player_input_set_key(2, PLAYER_INPUT_UP, KEY_SPACE);

    /* Save */
    const char *path = "test_keybinds.dat";
    TEST_ASSERT_TRUE(player_input_save(path));

    /* Reset to defaults */
    player_input_init_defaults();
    TEST_ASSERT_EQUAL(BIND_KEYBOARD, player_input_get_binding(0, PLAYER_INPUT_BOMB).type);

    /* Load */
    TEST_ASSERT_TRUE(player_input_load(path));

    /* Verify restored */
    InputBinding b0 = player_input_get_binding(0, PLAYER_INPUT_BOMB);
    TEST_ASSERT_EQUAL(BIND_GAMEPAD, b0.type);
    TEST_ASSERT_EQUAL(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, b0.code);

    InputBinding b2 = player_input_get_binding(2, PLAYER_INPUT_UP);
    TEST_ASSERT_EQUAL(BIND_KEYBOARD, b2.type);
    TEST_ASSERT_EQUAL(KEY_SPACE, b2.code);
}

void test_load_missing_file(void)
{
    /* Loading a nonexistent file should return false and keep current bindings */
    int old_key = player_input_get_key(0, PLAYER_INPUT_UP);
    TEST_ASSERT_FALSE(player_input_load("nonexistent_keybinds.dat"));
    TEST_ASSERT_EQUAL(old_key, player_input_get_key(0, PLAYER_INPUT_UP));
}

void test_out_of_bounds(void)
{
    /* Out-of-bounds player/action should return safe defaults */
    InputBinding b = player_input_get_binding(-1, PLAYER_INPUT_UP);
    TEST_ASSERT_EQUAL(BIND_KEYBOARD, b.type);
    TEST_ASSERT_EQUAL(0, b.code);

    b = player_input_get_binding(4, PLAYER_INPUT_UP);
    TEST_ASSERT_EQUAL(0, b.code);

    TEST_ASSERT_EQUAL(0, player_input_get_key(0, PLAYER_INPUT_COUNT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_bindings);
    RUN_TEST(test_default_bindings_type);
    RUN_TEST(test_no_key_conflicts);
    RUN_TEST(test_four_player_simultaneous_independence);
    RUN_TEST(test_set_custom_key);
    RUN_TEST(test_set_gamepad_binding);
    RUN_TEST(test_binding_name_keyboard);
    RUN_TEST(test_binding_name_gamepad);
    RUN_TEST(test_save_load_bindings);
    RUN_TEST(test_load_missing_file);
    RUN_TEST(test_out_of_bounds);
    return UNITY_END();
}
