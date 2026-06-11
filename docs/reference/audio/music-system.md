# Music system

How Mine Bombers 3.11 schedules music. All facts below are verified
against the decompile plus binary/asset ground truth.

## Modules

| Module | Title | Orders | Role |
|---|---|---|---|
| `HUIPPE.S3M` | "Huipentuja" | 24 (22 real) | Menu music. Loaded by `init_music_playback` (seg_1010:3255; filename at MB.EXE offset 102243) at startup and on every return to the main menu (seg_1000:7044–7050). |
| `OEKU.S3M` | "koira no. 14" | 92 (90 real) | The entire match: shop **and** gameplay, via order jumps. Loaded once at PLAY together with the 12 game VOCs (`FUN_1010_5c83`, seg_1010:3139–3157). |

OEKU's order list is two songs in one module:

- **Positions 0–82** (0-based): the main song — gameplay music.
- **Positions 83–89** (patterns 59 56 57 58 60 61 62): a distinct tail
  section — the **shop music**.

## The order-jump primitive

`FUN_1018_0855(n)` (seg_1018:418–430) requests "jump to song order n at the
next pattern boundary". Orders are **1-based** (the player indexes
`order_list[order-1]` and wraps to order 1 at end-of-list/`0xFF`,
seg_1018:610–614). The jump is ignored unless `0 < n <= OrdNum`.

Port equivalent: `music_jump_to_order` / `music_play_from_order`
(`src/audio/music.c`, via `xmp_set_position(ctx, n-1)`).

## Match timeline (GUS path, mode 0)

| Moment | Original | Effect |
|---|---|---|
| Menu | `init_music_playback` | HUIPPE plays |
| PLAY | mute (seg_1000:7054); load OEKU+SFX (7057) | **player select is silent** |
| Round start, pre-shop | jump to order **0x54** (84) if music active (7098–7101); unmute (7102) | shop plays the tail section |
| Post-shop | mute (7121); jump to `table[Random(14)]` (7122–7126); unmute (7127) | gameplay starts at a random song position |
| Round end | mute (7299) | results/finalizers run silent |
| Match end | screens run muted; `FUN_1010_5db7` stops the module (7341–7343) | silence until menu |
| Back at menu | `init_music_playback` (7046) | HUIPPE restarts |

The "mute"/"unmute" functions are misleadingly named in the decompile:
`swap_display_pages` = `disable_music`, `enable_vga_display` =
`enable_music` (mode 0). The music-active gate `FUN_1010_505a`
returns 1 unconditionally in mode 0 and the config music bit
(`DAT_1038_1bde & 1`) in SB modes.

## The per-round random position table

14 bytes at `0x1038:0x0010` (MB.EXE data segment base = file offset 195840,
located via the BGI driver-name string anchors):

```
1 5 15 22 32 39 43 53 56 62 68 76 80 83   (1-based OEKU orders)
```

Strictly ascending; entry 1 is the song start; entry 83 is the last order
before the shop section. Pick: `table[Random(14)]` with the Borland LCG.

Port: `src/game/music_schedule.{c,h}`, characterized by
`tests/test_music_schedule.c`.

## Single player

Identical model — the round loop's jumps are not gated on player count.
SP shop also plays order 84; SP gameplay also starts at a random table
position.
