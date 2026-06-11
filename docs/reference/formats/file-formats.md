# File Formats

Reverse-engineered from decompiled sources. All multi-byte values are little-endian (x86).

## Game Configuration — `OPTIONS.CFG` (17 bytes)

Written by `FUN_1000_000e` (seg_1000:6), read by `load_game_config` (seg_1010:5499).
Filename verified empirically: driving the original's options menu in
DOSBox-X writes `OPTIONS.CFG` next to MB.EXE. No config file ships with
the game; it appears on first options save. A game-written defaults file
is byte-identical to the port's `config_save` output (round-trip
compatibility verified for defaults).

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0x00 | 1 | num_players | 1-4 |
| 0x01 | 1 | config_byte | TREASURES count (options screen row 1; "bitfield" was a guess) |
| 0x02 | 2 | total_rounds | int16, forced to min 1 on load |
| 0x04 | 2 | starting_cash | int16 — "speed_setting" was a decompiler mislabel |
| 0x06 | 2 | time_limit_lo | int16, **PIT ticks** (18.2065 Hz, 54.9 ms each; NOT seconds) |
| 0x08 | 2 | time_limit_hi | int16 |
| 0x0A | 2 | frame_delay | int16 |
| 0x0C | 1 | option_darkness | "player colors" was a mislabel: option toggles |
| 0x0D | 1 | option_free_market | |
| 0x0E | 1 | option_selling | |
| 0x0F | 1 | option_winner_by | |
| 0x10 | 1 | bomb_damage_pct | BOMB DAMAGE % (options screen row 6, range 0-100; "starting_lives" was a decompiler mislabel) |

**Defaults (if file missing):** num_players=2, treasures=0x2D (45), total_rounds=15, starting_cash=750, time_limit=0x1DEE (7662 ticks = 7 min 01 s — the options screen displays it as real time "7:00 Min"), frame_delay=8, bomb_damage=100%, all option toggles=0. A game-written defaults file is `02 2D 0F 00 EE 02 EE 1D 00 00 08 00 00 00 00 00 64` (captured from the original).

## Player Database — `players.dat` (3,232 bytes)

Read/written by `FUN_1000_3276` (seg_1000:2096). Individual records accessed by `FUN_1000_15c7` (seg_1000:867).

- 32 records x 101 bytes each = 3,232 bytes (0xCA0)
- Record size = 0x65 (101 bytes)
- New file: each record initialized with first byte = 1, rest = 0

Per-record layout (decoded from `FUN_1000_19e8`, seg_1000:1053, and `FUN_1000_15c7`, seg_1000:867):

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0x00 | 1 | exists flag | **0 = record exists**, nonzero (1) = empty slot. Verified against the shipped `PLAYERS.DAT`: "Plr 1"/"Plr 2" have 0x00, the 30 empty slots 0x01. |
| 0x01 | 25 | name | Pascal String[24]: length byte + up to 24 chars |
| 0x1A | 40 | stats | 10 x uint32 cumulative career counters, merged once per multiplayer match: matches played, matches won, rounds played, round wins, treasures collected, money earned, (unidentified), weapons placed, deaths, tiles walked |
| 0x42 | 35 | weapons | weapon inventory bar data |

## Player Slot Selections (4 bytes)

Read by `FUN_1000_2e4e` (seg_1000:1863), written by `FUN_1000_2f9f` (seg_1000:1916).

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 1 | player1_ready |
| 0x01 | 1 | player2_ready |
| 0x02 | 1 | player3_ready |
| 0x03 | 1 | player4_ready |

## High Score Table (260 bytes)

Read by `FUN_1000_a619` (seg_1000:6713), written by `FUN_1000_a6d7` (seg_1000:6748).
Filename: `HALLOFFA.DAT` (string at address `0xa60c` in seg_1030).

10 entries x 26 bytes each = 260 bytes (0x104).

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 21 | Player name (Pascal string: 1 byte length + up to 20 chars) |
| 0x15 | 1 | Level number reached |
| 0x16 | 2 | Score low word (int16) |
| 0x18 | 2 | Score high word (int16) |

**Default:** 10 entries with empty names and zero scores.

## Saved Game Data with Checksums (118 bytes)

Read by `FUN_1010_02fc` (seg_1010:100).

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 115 | Game data block (loaded to DAT_1038_03e0) |
| 0x73 | 1 | Checksum #1 (mod 255, via FUN_1010_0127) |
| 0x74 | 1 | Checksum #2 (mod 255, via FUN_1010_01b5) |
| 0x75 | 1 | Checksum #3 (mod 255, via FUN_1010_0245) |

## Sound Card Configuration (6 bytes)

Read by `FUN_1000_6e4c` (seg_1000:4315). Sound hardware parameters (usage is in seg_1010).

| Offset | Size | Field | Default | Notes |
|--------|------|-------|---------|-------|
| 0x00 | 1 | Sound card type | 10 | 0=GUS, 1=SB, 2=SBPro, 3=SB16, 4=none, 10=no sound |
| 0x01 | 1 | DMA channel | 1 | An earlier revision labeled bytes 1-2 as IRQ then DMA; the defaults (1 and 7) match the Sound Blaster convention DMA=1, IRQ=7 |
| 0x02 | 1 | IRQ number | 7 | Sound Blaster IRQ |
| 0x03 | 1 | Base port multiplier | 2 | Port = value * 0x10 + 0x200 (2 → 0x220) |
| 0x04 | 1 | Sound features | 1 | Bitmask: bit 0 = music, bit 1 = SFX |
| 0x05 | 1 | Mixing rate multiplier | 22 | Rate = value * 1000 (22 → 22000 Hz) |

## Joystick Configuration — `joystic1.cfg`, `joystic2.cfg` (16 bytes each)

Read by `FUN_1000_6fcd` (seg_1000:4374).

| Offset | Size | Field | Default |
|--------|------|-------|---------|
| 0x00 | 2 | X-axis min | 10 |
| 0x02 | 2 | X-axis param 2 | 10 |
| 0x04 | 2 | Y-axis max | 400 |
| 0x06 | 2 | Y-axis param 2 | 400 |
| 0x08 | 2 | Center X low | 40 |
| 0x0A | 2 | Center X high | 40 |
| 0x0C | 2 | Center Y low | 350 |
| 0x0E | 2 | Center Y high | 350 |

## Key Bindings (text format, 32 values)

Read by `FUN_1010_9fbb` (seg_1010:5641), written by `FUN_1010_c1c5` (seg_1010:7103).

Text file with 32 integer values (Pascal `read`/`write` format). 8 values per player x 4 players.

Per-player field order in the file (from the read sequence in `FUN_1010_9fbb`): **Up, Down, Left, Right, Bomb, Remote, Choose/Sell, Stop**. (In the player struct the same keys sit at offsets 0xF3-0xFA in a different order: Left, Right, Up, Down, Stop, Bomb, Remote, Choose/Sell.)

**Default scancodes** (key identities resolved against MB.EXE's own scancode-name table; an earlier revision of this page misassigned the action labels):

- Player 1 (numpad + nav cluster): 0x48 NP-Up, 0x50 NP-Down, 0x4B NP-Left, 0x4D NP-Right, 0xB5 PageDown (Bomb), 0x4F NP-1/End (Remote), 0xAD PageUp (Choose/Sell), 0x4C NP-5 (Stop)
- Player 2 (WASD area): 0x11 W, 0x2D X, 0x1E A, 0x20 D, 0x0F Tab (Bomb), 0x2C Z (Remote), 0x29 key left of '1' (Choose/Sell), 0x1F S (Stop)
- Player 3 (OKL area): 0x18 O, 0x34 period, 0x25 K, 0x27 semicolon, 0x17 I (Bomb), 0x33 comma (Remote), 0x09 8 (Choose/Sell), 0x26 L (Stop)
- Player 4 (arrow keys + right-hand modifiers, extended scancodes): 0xAC Up, 0xB4 Down, 0xAF Left, 0xB1 Right, 0x81 RCtrl (Bomb), 0x9C RAlt (Remote), 0x36 RShift (Choose/Sell), 0x29 (Stop — the original ships this conflict with Player 2's Choose key)

## Level Maps — `.MNL` / `.MNE` (text format)

Read by `load_level` (seg_1000:2963).

- **NOT binary** — read line-by-line via text file functions
- Map dimensions: 45 columns (0x2D) x 64 rows (0x40)
- Total tiles: 2,880
- Each tile = 1 byte value (tile type index)
- Read column-by-column: for each of 45 columns, read a text line, then store 64 row values

Note: The original `.MNE` and `.MNL` files on disk are 2,970 bytes. The text encoding adds overhead vs raw 2,880 bytes.

## Custom Compressed Image — `.SPY` format

Read by `load_and_display_image` (seg_1010:4280).

```
Offset 0x000:  768 bytes (0x300) — palette (256 x RGB, 8-bit per channel)
Offset 0x300:  Variable — RLE-compressed pixel data (4 planes)
```

**Palette:** 256 entries, 3 bytes each (R, G, B). Values are already 8-bit (0-255), not 6-bit VGA DAC — see [SPY Format Analysis](spy-format-analysis.md). Loaded to `DAT_1038_0688`.

**RLE compression** (decoded by `FUN_1010_7114`, seg_1010:4237):
- Byte `0x01`: RLE marker. Next byte = fill value, next byte = run length.
- Any other byte: literal byte, copy as-is.
- Decompressed output per plane: 38,400 bytes (0x9600) = 80 bytes/row x 480 rows
- 4 planes, decompressed sequentially

The planes are **1-bit-per-pixel bitplanes** of a 640x480 image (640 pixels / 8 = 80 bytes per row). The four planes combine to a 4-bit color index per pixel, so SPY images use 16 colors — only the first 16 palette entries matter. Total decompressed data: 4 x 38,400 = 153,600 bytes.

## PCX Image Format

Read by `FUN_1010_73bd` (seg_1010:4367). Used for `.PPM` portrait files.

Standard PCX format:
```
Offset 0x000:  128 bytes — PCX header
Offset 0x080:  Variable — RLE pixel data
End - 0x301:   1 byte — palette marker (0x0C if palette present)
End - 0x300:   768 bytes — VGA palette (256 x RGB)
```

**PCX RLE:** If `(byte & 0xC0) == 0xC0`: count = `byte & 0x3F`, next byte = color. Otherwise: literal pixel.

## Font File — `.FON` (2,048 bytes)

Read by `load_font_file` (seg_1018:2055).

- Raw bitmap: 256 characters x 8 bytes each = 2,048 bytes
- Each character: 8x8 pixels, 1 bit per pixel
- 8 consecutive bytes per character (one byte per row, MSB = leftmost pixel)
- Loaded to `DAT_1038_7616`

## Sound Effects — `.VOC`

Loaded by `load_sound_effect` (seg_1008:1020) and `FUN_1008_33af` (seg_1008:2677).

The game loads VOC files **raw** — reads the entire file byte-by-byte and converts unsigned 8-bit PCM to signed (`byte - 0x80`). The Creative VOC header is NOT parsed separately, suggesting these may be headerless raw PCM files renamed as `.VOC`, or the header bytes cause a brief noise artifact at playback start.

**Known VOC files (9 unique effects):**
- `explos1.voc` through `explos5.voc` — explosion variants
- `urethan.voc` — urethane/material sound
- `kili.voc` — kill/death sound
- `applause.voc` — victory applause
- `pikkupom.voc` — small bomb / pickup sound

Plus 4 additional VOC files at string addresses 0x580C, 0x5819, 0x583A, 0x5847 (filenames not in string table).

## Music Module — `.S3M`

The port does not implement this loader — playback goes through libxmp.
The format details below document what the original's own player parses.

Loaded by `FUN_1018_0350` (seg_1018:169) via `load_music_module` (seg_1008:882).

Standard S3M (ScreamTracker 3) format with packed patterns:

**Header (96 bytes):**

| Field | Offset (approx) | Global |
|-------|-----------------|--------|
| Num patterns | varies | DAT_1038_71fe |
| Num instruments | varies | DAT_1038_7200 |
| Num orders | varies | DAT_1038_7202 |
| Default speed | varies | DAT_1038_720f |
| Default tempo | varies | DAT_1038_7210 |
| Flags | varies | DAT_1038_7213 |

After header: pattern pointer table, instrument offset table (num_instruments x 2 bytes), order table (num_orders x 2 bytes).

**Instrument headers** (80 bytes each): contain sample length, loop start/end, default volume, C5 speed, flags.

**Packed pattern format:** Standard S3M compression — byte 0 = end of row, otherwise bits 5/6/7 indicate extra parameter bytes (note+instrument, volume, effect+param).

**Known S3M file:** `oeku.s3m`

**Playback:** Up to 16 music channels + 11 simultaneous sound effect channels.

## Complete File Inventory

| File | Format | Size | R/W | Loader |
|------|--------|------|-----|--------|
| Config .dat | Binary, 17B | 17 B | R/W | seg_1000:6 / seg_1010:5499 |
| `players.dat` | Binary, 32x101B | 3,232 B | R/W | seg_1000:2096 |
| Player slots | Binary, 4B | 4 B | R/W | seg_1000:1863/1916 |
| High scores | Binary, 10x26B | 260 B | R/W | seg_1000:6713/6748 |
| Saved game | Binary, 115B+3 checksums | 118 B | R | seg_1010:100 |
| Sound config | Binary, 6B | 6 B | R | seg_1000:4315 |
| `joystic1.cfg` | Binary, 8x int16 | 16 B | R | seg_1000:4374 |
| `joystic2.cfg` | Binary, 8x int16 | 16 B | R | seg_1000:4374 |
| Key bindings | Text, 32 values | ~100 B | R/W | seg_1010:5641/7103 |
| `.MNL` / `.MNE` maps | Text, 45x64 tiles | ~2,970 B | R | seg_1000:2963 |
| `.SPY` images | RLE + palette | varies | R | seg_1010:4280 |
| `.PPM` portraits | PCX format | varies | R | seg_1010:4367 |
| `.FON` font | Raw bitmap 8x8 | 2,048 B | R | seg_1018:2055 |
| `.VOC` sounds | Raw unsigned 8-bit PCM | varies | R | seg_1008:1020/2677 |
| `.S3M` music | ScreamTracker 3 module | varies | R | seg_1018:169 |
