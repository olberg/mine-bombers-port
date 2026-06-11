# Input Binding Philosophy

How the port's input system differs from the original DOS game, and the design decisions behind it.

## Original DOS Input System

The original used a 3-layer architecture:

1. **INT 9 ISR** — hardware keyboard interrupt writing to a 184-byte scancode state array (0x00–0xB7)
2. **Joystick polling** — game port 0x201 analog axes translated to virtual scancodes 0x59–0x66 (2 joysticks, 4 directions + 2 buttons each)
3. **Per-player bindings** — 8 scancodes per player stored at player struct offsets 0xF3–0xFA

Key config screen (`FUN_1010_d4c4`): sequential capture — walks P1–P4, 8 actions each, waits for keypress, stores scancode. ESC skips, F10 aborts. Background: `KEYS.SPY`. Key names displayed in Finnish via 183-entry scancode name table.

Persistence: text file with 32 space-separated integers (8 per player × 4 players).

## Port Input System — What Changes

### Abstraction layer

The port does NOT replicate the DOS scancode array or interrupt handler. Raylib's `IsKeyDown()`/`IsKeyPressed()` and gamepad API replace the hardware layer entirely. This is a **utility-level** concern (results-match fidelity) — the input abstraction is a Raylib concern, not a fidelity target.

### Binding data model

Each binding is a `{type, code}` pair:
- `type`: 0 = keyboard (`KeyboardKey` enum), 1 = gamepad button (`GamepadButton` enum)
- `code`: the Raylib enum value

This differs from the original's single-scancode model because the port supports two input device types per binding. The original unified keyboard and joystick into one scancode namespace (virtual scancodes 0x59+ for joystick); the port keeps them as separate types with a discriminator.

### Gamepad assignment

**Fixed mapping: player N uses gamepad N-1.** No dynamic assignment or press-to-join. The original similarly had fixed joystick-to-player mapping (joy1 = scancodes 0x59–0x5E, joy2 = 0x5F–0x64).

### Mixed bindings

Each of a player's 8 actions can independently be a keyboard key OR a gamepad button. A player can mix devices freely (e.g., d-pad for movement, keyboard for bomb). The original allowed this too since joystick directions were just scancodes in the same namespace as keyboard keys.

### Analog stick

The left analog stick always provides directional movement input, regardless of what's bound to the 4 direction actions. This is a convenience addition — the original had no analog input. It does not affect game mechanics (digital 4-way movement is preserved; the stick is thresholded to digital directions).

### Key names

English, not Finnish. The original's 183-entry Finnish name table ("OUTOPILKKU", "NOKKA", etc.) is replaced with English names ("Left Shift", "Num 5", "Tab", etc.). This is a **cosmetic** change — the key config screen is scaffolding-level fidelity.

### Persistence

A binary file `assets/keybinds.dat` — 64 integers (4 players × 8 actions × 2 ints per binding), written with Raylib's `SaveFileData`. Not the original's text format, which is acceptable because the stored values are Raylib key codes, not DOS scancodes — the file format is inherently incompatible with the original anyway.

### Key config UI flow

**Matches the original**: sequential capture cycling P1→P4, 8 actions each. ESC skips a binding, F10 aborts entire config. `KEYS.SPY` background. The capture loop listens for both keyboard and gamepad input simultaneously — whichever fires first is stored.

## What's Preserved (1:1)

- 8 actions per player: Left, Right, Up, Down, Stop, Bomb, Remote, Cycle
- 4 players
- Sequential capture UI flow (ESC skip, F10 abort)
- KEYS.SPY background
- Default key bindings match original layout (numpad for P1, WASD-area for P2, OKL-area for P3, arrows for P4)

## What's Different

| Aspect | Original | Port | Why |
|--------|----------|------|-----|
| Device types | Unified scancode namespace | Separate keyboard/gamepad types | Raylib has distinct APIs |
| Joystick mapping | Virtual scancodes 0x59–0x66 | Fixed pad N-1 per player | Modern gamepads don't need scancode translation |
| Analog stick | Not applicable | Always active for movement | Convenience; no mechanical impact |
| Key names | Finnish | English | Audience |
| Persistence format | Text file, 32 ints | Binary keybinds.dat, 64 ints | Different key code space |
| Joystick calibration | joystic1.cfg/joystic2.cfg, 3-frame smoothing | Not needed | Modern gamepads self-calibrate |
