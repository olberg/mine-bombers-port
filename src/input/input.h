#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

/* --- Menu-level input (single user) --- */

typedef enum {
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CONFIRM,
    INPUT_CANCEL,
    INPUT_QUIT,
    INPUT_ANY_KEY,     /* true if any key/button was just pressed */
    INPUT_ACTION_COUNT
} InputAction;

/* Poll input state. Call once at start of each frame. */
void input_update(void);

/* Returns true if the action was just pressed this frame. */
bool input_pressed(InputAction action);

/* --- Per-player input (gameplay) --- */

typedef enum {
    PLAYER_INPUT_UP,
    PLAYER_INPUT_DOWN,
    PLAYER_INPUT_LEFT,
    PLAYER_INPUT_RIGHT,
    PLAYER_INPUT_STOP,
    PLAYER_INPUT_BOMB,
    PLAYER_INPUT_REMOTE,
    PLAYER_INPUT_CYCLE,
    PLAYER_INPUT_COUNT
} PlayerInputAction;

/* Input binding: keyboard key OR gamepad button */
typedef enum {
    BIND_KEYBOARD = 0,
    BIND_GAMEPAD  = 1
} InputBindType;

typedef struct {
    InputBindType type;
    int code;  /* KeyboardKey or GamepadButton enum value */
} InputBinding;

/* Set default key bindings for all 4 players (matching original DOS defaults). */
void player_input_init_defaults(void);

/* Set custom key binding for a player action (keyboard type). */
void player_input_set_key(int player_idx, PlayerInputAction action, int raylib_key);

/* Set a binding (keyboard or gamepad) for a player action. */
void player_input_set_binding(int player_idx, PlayerInputAction action, InputBinding bind);

/* Get the binding for a player action. */
InputBinding player_input_get_binding(int player_idx, PlayerInputAction action);

/* Get the Raylib key code for a player's action binding (keyboard bindings only).
 * Returns 0 for gamepad bindings. */
int player_input_get_key(int player_idx, PlayerInputAction action);

/* --- Scripted input injection (autoplay harness) ---
 * When injection mode is on, player_input_down/pressed report ONLY the
 * injected state (real devices are ignored, keeping scripted runs
 * deterministic). The harness clears and re-injects every frame. */
void player_input_inject_mode(bool enabled);
void player_input_inject_clear(int player_idx);
void player_input_inject(int player_idx, PlayerInputAction action,
                         bool down, bool pressed);

/* Returns true if the action key is currently held down for this player. */
bool player_input_down(int player_idx, PlayerInputAction action);

/* Returns true if the action key was just pressed this frame for this player. */
bool player_input_pressed(int player_idx, PlayerInputAction action);

/* Like player_input_pressed but WITHOUT the OS typematic repeat — true only
 * on the physical press edge. For consumers that have their own hold/repeat
 * logic (e.g. the shop's buy/sell ramp) where typematic would double-fire. */
bool player_input_edge(int player_idx, PlayerInputAction action);

/* Get a human-readable name for a binding ("W", "Num 8", "Pad A", etc.) */
const char *input_binding_name(InputBinding bind);

/* Save all player bindings to a binary file. Returns true on success. */
bool player_input_save(const char *path);

/* Load player bindings from a binary file. Returns true on success.
 * On failure, keeps current bindings unchanged. */
bool player_input_load(const char *path);

#endif
