# Input System

Reverse-engineered from `seg_1008_sound.c`, `seg_1010_graphics.c`, and `seg_1018_system.c`.

## Architecture Overview

The input system has three layers:
1. **Hardware layer** — INT 9 keyboard ISR + game port 0x201 joystick polling
2. **Virtual scancode layer** — unified 184-byte key state array (scancodes 0x00–0xB7)
3. **Per-player binding layer** — 8 scancodes per player mapped to actions

Joystick axes/buttons are translated into virtual scancodes (0x59–0x66) and written into the same key state array as keyboard scancodes. Game logic only reads the key state array — it never touches hardware directly.

## Keyboard Interrupt Handler

### Installation

`FUN_1018_2ede()` (seg_1018:1855):
1. Clears all key states: indices 1 through 0xB7 (183 entries) set to 0
2. Saves old INT 9 vector to `DAT_1038_760c`/`DAT_1038_760e`
3. Installs new INT 9 handler at address 0x2E2C in seg_1018 via DOS `INT 0x21` function 0x25

### Restoration

`FUN_1018_2f27()` restores the original INT 9 vector from saved values.

### Key State Array

**Base**: `DAT_1038_7552` — 184 bytes (indices 0–0xB7).

Access pattern:
```
*(byte*)(&DAT_1038_7552 + scancode + 1)
```
- Value `1` = key pressed
- Value `0` = key released

The ISR reads the scancode from port 0x60 and sets/clears the corresponding byte. Covers:
- Standard keyboard scancodes 0x01–0x58
- Joystick virtual scancodes 0x59–0x66
- Extended keyboard scancodes 0x80–0xB7

### In-round special keys (re-derived from MB.EXE bytes 49020-49135)

The named key globals in the decompile are direct bytes of this array
(`address = 0x7553 + scancode`); the decompiler's names for two of them
were misleading:

| Global (decompile name) | DS addr | Scancode | Key | Actual function |
|---|---|---|---|---|
| `g_key_mp_sync` | 0x7554 | 0x01 | **ESC** | End the current ROUND (multiplayer only; no match abort) — seg_1000:7141 |
| `g_key_pause` | 0x756C | 0x19 | **P** | Pause with palette dim — seg_1000:7146 |
| `g_key_screen_toggle` | 0x7592 | 0x3F | **F5** | Music mute/unmute toggle — seg_1000:7149 |
| `g_key_esc` | 0x7597 | 0x44 | **F10** | Abort the match (`rounds_remaining = 0`) — seg_1000:7160 |

So in the original, ESC mid-round merely ends the round in multiplayer (and
does nothing in single-player); F10 is the abort/quit key, consistent with
F10 quitting the main menu.

## Per-Player Key Bindings

### Storage

8 scancode values per player, stored within the player struct (266 bytes, stride 0x10A):

| Offset | Action | Description |
|--------|--------|-------------|
| +0xF3 | Left | Move left |
| +0xF4 | Right | Move right |
| +0xF5 | Up | Move up |
| +0xF6 | Down | Move down |
| +0xF7 | Stop | Stop movement |
| +0xF8 | Bomb/Buy | Place bomb / buy in shop |
| +0xF9 | Remote/Extra | Remote detonate / extra action |
| +0xFA | Choose/Sell | Cycle weapon / sell in shop |

### Default Scancodes

> Ground truth: defaults from `FUN_1010_9fbb` (seg_1010:5690–5721); key
> names decoded from MB.EXE's own scancode-name tables (standard table at
> file offset 155819, 19 B/entry; extended-key cases of `FUN_1018_2652`).
> 0xB5 names as **PAGEDOWN** (not Numpad /) and 0x9C as **RIGHT ALT** (not
> Numpad Enter). +0xF9 = Remote (detonation scan, seg_1000:2801), +0xFA =
> Choose/Sell (weapon cycler, seg_1000:2795).

| Action | Player 1 | Player 2 | Player 3 | Player 4 |
|--------|----------|----------|----------|----------|
| Left | 0x4B (Numpad Left) | 0x1E (A) | 0x25 (K) | 0xAF (Arrow Left) |
| Right | 0x4D (Numpad Right) | 0x20 (D) | 0x27 (;) | 0xB1 (Arrow Right) |
| Up | 0x48 (Numpad Up) | 0x11 (W) | 0x18 (O) | 0xAC (Arrow Up) |
| Down | 0x50 (Numpad Down) | 0x2D (X) | 0x34 (.) | 0xB4 (Arrow Down) |
| Stop | 0x4C (Numpad 5) | 0x1F (S) | 0x26 (L) | 0x29 (§, left of 1) |
| Bomb/Buy | 0xB5 (Page Down) | 0x0F (Tab) | 0x17 (I) | 0x81 (Right Ctrl) |
| Remote | 0x4F (Numpad End/1) | 0x2C (Z) | 0x33 (,) | 0x9C (Right Alt) |
| Choose/Sell | 0xAD (Page Up) | 0x29 (§, left of 1) | 0x09 (8) | 0x36 (Right Shift) |

The defaults ship exactly one conflict: scancode 0x29 is both Player 2
Choose/Sell and Player 4 Stop.

### Runtime Lookup

Game logic checks key state by reading the scancode from the player struct and indexing into the array:
```c
if (*(char *)(&DAT_1038_7552 + player_data[0xF5] + 1) != 0)
    // Up key is pressed for this player
```

### Key-read semantics in the round loop

All per-player key reads happen in `process_weapons` (seg_1000:2582–2819),
which runs for each ALIVE player on **even frames only**, after
`move_player` (which runs every frame):

- **Directions** are level-triggered with priority **Up > Right > Down >
  Left > Stop**; Stop is level-triggered and lowest priority. With no
  direction held the player **keeps sliding** in the current direction
  (the port's testable unit is `round_resolve_direction` in `round.c`).
- **Bomb** and **Choose/Sell** are one-shot per press: the handler clears
  the shared key-state byte after acting, and BIOS typematic repeat
  re-sets it for auto-repeat while held (port: `IsKeyPressedRepeat`).
- **Remote** is level-triggered — the byte is never cleared.
- Known (accepted) deviation: because the original CLEARS the shared
  byte, two players bound to the same Bomb/Choose key race (lowest player
  index wins). The port polls per player, so both trigger. Only reachable
  with non-default bindings.

### Loading & Saving

- **Load**: `FUN_1010_9fbb()` (seg_1010:5641) reads 32 integers from a text config file (8 per player x 4 players). Falls back to hardcoded defaults if file missing.
- **Save**: `FUN_1010_c1c5()` (seg_1010:7103) writes 32 integers to the same text file.

## Key Configuration Screen

### Display

`key_config_screen()` (seg_1010:7731):
1. Iterates through all 4 players
2. For each player, draws 8 binding labels with 3-pass shadow text (colors 0x0C, 4, 8).
   Note the row order — the last two rows are Choose/Sell then Remote
   (label strings at 0x1030:0xD099 / 0xD0A8 decoded from MB.EXE):
   ```
   Player N Left       : [key_name]   (struct offset 0xF3)
   Player N Right      : [key_name]   (struct offset 0xF4)
   Player N Up         : [key_name]   (struct offset 0xF5)
   Player N Down       : [key_name]   (struct offset 0xF6)
   Player N Stop       : [key_name]   (struct offset 0xF7)
   Player N Bomb/Buy   : [key_name]   (struct offset 0xF8)
   Player N Choose/Sell: [key_name]   (struct offset 0xFA)
   Player N Remote     : [key_name]   (struct offset 0xF9)
   ```
3. Y positions: `(player * 8 + 10 + line_offset) * 10` pixels

### Binding Capture

`FUN_1010_d4c4()` (seg_1010:7894):
1. Loads background image for key config screen
2. Displays current bindings via `key_config_screen()`
3. Fades in palette
4. For each player (0–3), for each of 8 bindings:
   - Waits 100 ticks
   - Moves cursor to appropriate line
   - Calls `FUN_1010_a200()` — scans key state array for any pressed key
   - ESC (0x01) → skip this binding
   - F10 (0x44) → stop asking for more bindings, save what was captured so far
   - Otherwise → stores scancode at offset 0xF3–0xFA
   - Displays key name, waits for key release
5. After all players done (or F10), saves bindings via `FUN_1010_c1c5()`

### Scancode Scanner

`FUN_1010_a200()` (seg_1010:5728): Loops calling `process_all_key_inputs()` and scanning indices 1 through 0xB7 in the key state array, returning the first pressed scancode.

## Joystick System

### Hardware Reading

`FUN_1008_3ba1()` (seg_1008:3258) reads both joysticks via game port 0x201:

```
Parameters:
  param_1  — joystick 2 enabled flag
  param_2  — joystick 1 enabled flag
  param_3  — joy2 button2 state (out)
  param_4  — joy2 button1 state (out)
  param_5  — joy2 Y axis value (out)
  param_6  — joy2 X axis value (out)
  param_7  — joy1 button2 state (out)
  param_8  — joy1 button1 state (out)
  param_9  — joy1 Y axis value (out)
  param_10 — joy1 X axis value (out)
```

Algorithm:
1. Triggers read: `out(0x201, 0x17)`
2. Polls port 0x201 in a loop (up to 10,001 iterations), timing how long each axis bit stays high
3. Axis bits: bit 0 = joy1 X, bit 1 = joy1 Y, bit 2 = joy2 X, bit 3 = joy2 Y
4. Button bits (active low): bit 4 = joy1 btn1, bit 5 = joy1 btn2, bit 6 = joy2 btn1, bit 7 = joy2 btn2

### Joystick Detection

- `FUN_1008_3d58()` (joy1) and `FUN_1008_3db1()` (joy2): Write to port 0x201, wait 150 ticks, check if axis bits changed. Unchanged bits = no joystick connected.

### Joystick-to-Scancode Translation

`process_all_key_inputs()` (seg_1010:172–418) converts analog joystick values to virtual scancodes:

1. Axis values averaged over 3 frames for smoothing
2. Compared against calibrated thresholds from `joystic1.cfg`/`joystic2.cfg`
3. Results written to virtual direction flags in the key state array

**Virtual Scancode Mapping**:

| Scancode | Joystick Action |
|----------|----------------|
| 0x59 | Joystick 1 Left |
| 0x5A | Joystick 1 Right |
| 0x5B | Joystick 1 Up |
| 0x5C | Joystick 1 Down |
| 0x5D | Joystick 1 Button 1 |
| 0x5E | Joystick 1 Button 2 |
| 0x5F | Joystick 2 Left |
| 0x60 | Joystick 2 Right |
| 0x61 | Joystick 2 Up |
| 0x62 | Joystick 2 Down |
| 0x63 | Joystick 2 Button 1 |
| 0x64 | Joystick 2 Button 2 |
| 0x65 | Joystick 1 Both Buttons |
| 0x66 | Joystick 2 Both Buttons |

When single-axis input detected, only that direction scancode is set. When diagonal (2+ axes), both direction scancodes are set simultaneously.

### Joystick Calibration

Calibration data loaded from `joystic1.cfg` / `joystic2.cfg` (16 bytes each, 8 x int16):
- X-axis min/max thresholds
- Y-axis min/max thresholds
- Center X low/high
- Center Y low/high

F1 and F2 keys double as joystick calibration triggers (noted in scancode name table as "F1/Joy1 Center", "F2/Joy2 Center").

## Scancode Name Table

183 entries covering scancodes 0x01–0xB7. Each name is a 19-character fixed-width Pascal string. Rendered by `FUN_1018_2652()` (seg_1018:1384) via a large switch/case.

### Standard Keyboard (0x01–0x58)

| Range | Keys |
|-------|------|
| 0x01 | ESC |
| 0x02–0x0B | 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 |
| 0x0C | PLUS |
| 0x0D | OUTOPILKKU (Finnish: accent mark) |
| 0x0E | BACKSPACE |
| 0x0F | TAB |
| 0x10–0x19 | Q, W, E, R, T, Y, U, I, O, P |
| 0x1A | (bracket) |
| 0x1B | ^ |
| 0x1C | ENTER |
| 0x1D | LEFT CONTROL |
| 0x1E–0x26 | A, S, D, F, G, H, J, K, L |
| 0x2A | LEFT SHIFT |
| 0x2B | APOSTROPHE |
| 0x2C–0x35 | Z, X, C, V, B, N, M, comma, period, slash |
| 0x36 | RIGHT SHIFT |
| 0x37 | NUMPAD * |
| 0x38 | LEFT ALT |
| 0x39 | SPACEBAR |
| 0x3A | CAPS LOCK |
| 0x3B–0x44 | F1–F10 |
| 0x45 | NUMLOCK |
| 0x46 | SCROLL LOCK |
| 0x47–0x53 | NUMPAD HOME through NUMPAD DEL |
| 0x56 | NOKKA (Finnish: angle bracket key) |
| 0x57–0x58 | F11, F12 |

### Extended Keyboard (0x80–0xB7)

| Scancode | Key |
|----------|-----|
| 0x80 | NUMPAD ENTER |
| 0x81 | RIGHT CONTROL |
| 0x99 | NUMPAD / |
| 0x9C | RIGHT ALT |
| 0xAB | HOME |
| 0xAC | UP ARROW |
| 0xAD | PAGE UP |
| 0xAF | LEFT ARROW |
| 0xB1 | RIGHT ARROW |
| 0xB3 | END |
| 0xB4 | DOWN ARROW |
| 0xB5 | PAGE DOWN |
| 0xB6 | INSERT |
| 0xB7 | DELETE |

Note: Finnish keyboard layout used throughout (e.g., "OUTOPILKKU", "NOKKA").

## Port Mapping

The port (`src/input/input.c`) replaces the INT 9 ISR with Raylib key
polling behind the same per-player 8-action binding model. Joystick
virtual scancodes (0x59–0x66) map to the Raylib gamepad API, with
Raylib's analog axis handling replacing the original's 3-frame smoothing
and calibration thresholds.
