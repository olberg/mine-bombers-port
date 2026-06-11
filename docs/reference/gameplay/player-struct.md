# Player Data Structure

Reconstructed from offset access patterns in the decompiled code.

## Memory Layout

Player data is stored in 4 parallel blocks in the data segment (0x1038).
The base is **0x1BEA** — the address of the `g_player1_data` variable;
every struct offset is `DAT address − 0x1BEA`:

| Player | Base Address | End Address | Size |
|--------|-------------|-------------|------|
| Player 1 | 0x1BEA | 0x1CF3 | 266 bytes (0x10A) |
| Player 2 | 0x1CF4 | 0x1DFD | 266 bytes (0x10A) |
| Player 3 | 0x1DFE | 0x1F07 | 266 bytes (0x10A) |
| Player 4 | 0x1F08 | 0x2011 | 266 bytes (0x10A) |

**Stride between players: 0x10A (266) bytes.**

## Known Fields (by offset from player base)

Reconstructed from how the code accesses player data relative to the base pointers.

### Health & Stats (confirmed)

| Offset | Size | Field | Evidence |
|--------|------|-------|----------|
| +0x1B | 2 | max_health | `DAT_1038_1c05 = DAT_1038_1c07` (seg_1010:7097) |
| +0x1D | 2 | health | `DAT_1038_1c07 = steel_plates * 100 + 100` (seg_1010:7090) |
| +0x21 | 1 | dead flag | `cmp byte [1C0B],0` in FUN_1000_a17c (MB.EXE file offset 45464) |

### Money & scoring fields (verified 2026-06-10)

| Offset | Size | Field | Evidence |
|--------|------|-------|----------|
| +0xA8 | 4 | digging power (in-level) | `DAT_1038_1c92`: reset to 1/round (seg_1010:7073), gems +1/+3/+5 (seg_1000:3467), money-bomb ±300 target |
| +0xAC | 4 | dig tool bonus | `DAT_1038_1c96` = lg_rockpick*3 + rockpick + powerdrill*5 (seg_1010:7065); HUD Y=11 prints +0xA8 + +0xAC |
| +0xE6 | 4 | **earned this round** | `DAT_1038_1cd0`: pickups += value (seg_1000:3503), zeroed at round start (seg_1010:7428), pooled/banked by FUN_1000_a17c |
| +0xEA | 4 | **wallet** | `DAT_1038_1cd4`: shop debits/credits (seg_1010:6694/6751), scoring payouts, results ranking, Lottery cheat = 50000 |
| +0xFD | 2 | round wins | `DAT_1038_1ce7`: `inc` for survivors in FUN_1000_a17c; results ranking when winner-by = wins |
| +0xFF | 4 | far ptr → 40-byte match-stats block | `DAT_1038_1ce9`: getmem(0x28) seg_1010:5570; +0x00 matches, +0x04 match wins, +0x08 rounds, +0x10 treasures, +0x14 money earned |
| +0x104 | 4 | money-bomb (super drill) counter | `DAT_1038_1cee`: set to 10 on use (seg_1000:2678), ticks per 18 frames (seg_1010:7650). The old "+0xE9/+0xEB speed" label was a 0x1C05-base artifact |

### Dig Stats (confirmed via pointer-indirect access)

| Offset | Size | Field | Evidence |
|--------|------|-------|----------|
| +0xA8 | 2+2 | digging_power (lo+hi) | `DAT_1038_1c92/1c94`, reset to 1 per round (seg_1010:7073), accumulates stat gems |
| +0xAC | 2+2 | bonus_stat (lo+hi) | `DAT_1038_1c96/1c98`, = `lg_rockpick*3 + rockpick + powerdrill*5` (seg_1010:7065) |
| +0xD2 | 2 | rockpick count | `DAT_1038_1cbc`, read by game_state_update |
| +0xD4 | 2 | lg_rockpick count | `DAT_1038_1cbe`, read by game_state_update |
| +0xD6 | 2 | powerdrill count | `DAT_1038_1cc0`, read by game_state_update |
| +0xE0 | 2 | steel_plates | `DAT_1038_1cca`, used in FUN_1010_c15c health formula |

### Position & Movement

Player pixel positions are at offset +0xEE/+0xF0, accessed only via pointer arithmetic:

| Offset | Size | Field | Evidence |
|--------|------|-------|----------|
| +0xEE | 2 | pixel_x | `*(int*)(base + 0xEE)` used in move_player. **pixel_x = tile_col * 10 + 5** (FUN_1000_3b40:2530). tile_col = pixel_x / 10. **In the original, X = rows (horizontal), Y = columns (vertical).** |
| +0xF0 | 2 | pixel_y | `*(int*)(base + 0xF0)` used in move_player. **pixel_y = tile_row * 10 + 35** (FUN_1000_3b40:2531). tile_row = (pixel_y - 30) / 10 |

Round-start positions (the four map corners, randomly swapped within the P1/P2 and P3/P4 pairs) are written directly into +0xEE/+0xF0 by `FUN_1010_c4f2` (seg_1010:7232-7288). An earlier revision of this page listed separate `start_pixel` fields at +0xD3/+0xD5 — those offsets sit inside the dig-tool counters (+0xD2/+0xD4) and the fields do not exist.

**Coordinate system**: The original game maps rows→X (horizontal, 0-639) and columns→Y (vertical, 30-479); the port follows the same convention. The entity spawn function FUN_1000_722b (seg_1000:4602-4603) confirms: `pixel_x = loop_row * 10 + 5`, `pixel_y = loop_col * 10 + 35`.

Evidence: In the frame loop (seg_1000:7186-7238), after `move_player()` the code checks values at fixed offsets from the player base (e.g., `DAT_1038_1cf0` for player 1, `DAT_1038_1dfa` for player 2 — difference = 0x10A). If the velocity value is positive, `move_player()` is called again (double movement for speed boost).

Player color is stored in config globals, not in the struct.

### Parallel Global Patterns

These are confirmed to have stride 0x10A between player instances:

| Player 1 | Player 2 | Player 3 | Player 4 | Meaning |
|----------|----------|----------|----------|---------|
| `g_player1_data` | `g_player2_data` | `g_player3_data` | `g_player4_data` | Struct base |
| `g_player1_dead` | `g_player2_dead` | `g_player3_dead` | `g_player4_dead` | Death flag (+0x21) |
| `g_player1_ready` | `g_player2_ready` | `g_player3_ready` | `g_player4_ready` | Slot active |
| `DAT_1038_1cf0` | `DAT_1038_1dfa` | `DAT_1038_1f04` | `DAT_1038_200e` | money-bomb counter HI word (+0x106) — read by the never-firing double-move gate (seg_1000:7190) |
| `DAT_1038_1cee` | `DAT_1038_1df8` | `DAT_1038_1f02` | `DAT_1038_200c` | money-bomb counter LO word (+0x104) |
| `DAT_1038_1c8e` | `DAT_1038_1d98` | `DAT_1038_1ea2` | — | Value zeroed on death (+0xA4) |

### Computed Offsets (base 0x1BEA)

```
Player 1 base = 0x1BEA (address of g_player1_data)
DAT_1038_1cee - 0x1BEA = 0x104 → money-bomb counter low word
DAT_1038_1cf0 - 0x1BEA = 0x106 → money-bomb counter high word
DAT_1038_1c8e - 0x1BEA = 0xA4  → zeroed-on-death field
```
Note: the "speed bonus" double-move gate (seg_1000:7190) reads the
money-bomb counter's high word, which never exceeds 0 (the counter maxes
at 10) — so the double-move path never fires in the original; the field
is not a speed value.

### Weapon & Tool Inventory (offsets +0xB0 through +0xE5)

This region is the weapon/tool inventory: 16-bit counts at even offsets (+0xB0 small bomb through +0xE4 super drill), interleaved with the dig-tool counters (+0xD2/+0xD4/+0xD6) and steel plates (+0xE0). See the weapon table in [Game Mechanics](game-mechanics.md) for the per-weapon offsets. An earlier revision of this page described +0xD9-0xE6 as byte-sized boolean state flags — a misread of the decompiler's `undefined1` placeholders.

## Key Binding Storage (per player)

Stored at fixed addresses, 8 values per player (see [Input System](../input/input-system.md) for scancodes):

| Player | Base Address | Stride |
|--------|-------------|--------|
| Player 1 | 0x1CDD | +0x10A = Player 2 |
| Player 2 | 0x1DE7 | +0x10A = Player 3 |
| Player 3 | 0x1EF1 | +0x10A = Player 4 |
| Player 4 | 0x1FFB | — |

Per-player key fields (1 byte each; verified against the default-scancode assignments in `FUN_1010_9fbb`):
```
+0x00: Left scancode
+0x01: Right scancode
+0x02: Up scancode
+0x03: Down scancode
+0x04: Stop scancode
+0x05: Bomb/Buy scancode
+0x06: Remote scancode
+0x07: Choose/Sell scancode
```

The on-disk keys file stores the same 8 values in a different order (Up, Down, Left, Right, Bomb, Remote, Choose/Sell, Stop — see [File Formats](../formats/file-formats.md)). An earlier revision had Remote and Choose/Sell swapped and listed a phantom "Extra" action; there are exactly 8 actions, none spare.

Note: Key data is embedded within the player struct (addresses 0x1CDD-0x1CE4 fall within player 1's block).

## Port Mapping

The port replaces the original's 4 parallel global blocks with the
`Player` struct array in `src/game/player.h` (`g_players[MAX_PLAYERS]`).
The 0x10A stride is the key to decoding further fields — any access
pattern repeating at +0x10A intervals is a field within the struct.

## Players.dat Record (101 bytes)

The on-disk player database stores 32 records of 101 bytes each. This is a separate (smaller) record from the in-memory 266-byte runtime struct. Decoded layout (details in [File Formats](../formats/file-formats.md)):
- Byte 0 = exists flag (**0 = record exists**, 1 = empty slot)
- Bytes 1-25 = Pascal String[24] name
- Bytes 26-65 = 10 x uint32 cumulative career stats
- Bytes 66-100 = weapon inventory bar data
- Does NOT contain runtime state (position, velocity, etc.)

The 101-byte record is loaded/saved via `FUN_1000_3276` and individual records via `FUN_1000_15c7`.
