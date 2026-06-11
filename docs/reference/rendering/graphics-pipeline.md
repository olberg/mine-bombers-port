# Graphics Rendering Pipeline

Reverse-engineered from `seg_1010_graphics.c`, `seg_1028_gfx_primitives.c`, and `seg_1018_system.c`.

## VGA Mode 12h (planar)

The game uses VGA Mode 12h — the planar 640x480, 16-color mode with 4
bitplanes and an 80-byte row stride. Confirmed by the BGI driver string
`"640 x 480 VGA"` (seg_1020) and the SPY plane size (38,400 bytes =
80 x 480). Key VGA register operations:

### Plane Selection
- **Write plane**: port 0x3C4 (Sequencer Index) = 2, port 0x3C5 = `1 << plane` (0-3)
- **Read plane**: port 0x3CE (Graphics Controller Index) = 4, port 0x3CF = plane (0-3)
- **All planes**: port 0x3C5 = 0x0F

### Page Flipping
- `set_draw_page(param_1, param_2)` — sets current draw target (up to 12 pages, stored in `DAT_1038_7FFE/8000`)
- `swap_display_pages()` (seg_1010:3295) — swaps visible/draw pages
- Display page set via CRTC Start Address registers (0x3D4/0x3D5, registers 0x0C/0x0D)

### Vertical Retrace Sync
```
wait for retrace end:  while (in(0x3DA) & 8) != 0
wait for retrace start: while (in(0x3DA) & 8) == 0
```

### Screen Shake
Offsets the CRTC Start Address by `shake_value * 0x50` bytes on odd frames. Resets to 0 when complete. Uses `g_screen_shake` global.

## Screen Layout

**Resolution**: 640x480 visible, 16 colors.

**Game field**: each tile is 10x10 pixels. The map's 64-tile axis runs
horizontally (`screen X = row * 10`, 0–630) and the 45-tile axis
vertically (`screen Y = col * 10 + 30`, 30–470) below a 30-pixel HUD
strip. The whole map is visible at once — there is no scrolling (see
[Multiplayer Camera](multiplayer-camera.md)).

**Player HUD Panel Positions**:
| Player | X offset | Y offset |
|--------|----------|----------|
| Player 1 | 12 (0x0C) | 0 |
| Player 2 | 174 (0xAE) | 0 |
| Player 3 | 337 (0x151) | 0 |
| Player 4 | 500 (0x1F4) | 0 |

**Player Name Positions**: Y=1, X= 32, 194, 357, 520 for players 1-4.

## Sprite System

### Sprite Data Format
```
offset 0: uint16 width
offset 2: uint16 height
offset 4: pixel data (width * height bytes, planar)
```

### Sprite Blitting
`blit_sprite(mode, sprite_ptr, y, x)` (seg_1028:1298):

1. Saves original height
2. **Clips to viewport**: height clipped to `viewport_bottom - (y + viewport_y_offset)`
3. **Bounds check**: x + viewport_x_offset must be >= 0, x + width must not exceed viewport right edge
4. Calls `FUN_1028_1fac` for actual pixel copy if within bounds
5. Restores original height

Viewport defined by: `DAT_1038_7fee` (X offset), `DAT_1038_7ff0` (Y offset), `DAT_1038_7f5e` (right edge), `DAT_1038_7f60` (bottom edge).

### Screen Capture
`capture_screen_region(dest, x2, y2, x1, y1)` (seg_1028:2110) — copies VRAM rectangle to memory buffer. First 2 words of dest = width, height.

## Rendering Order

Per frame:
1. **Tile map** — `draw_map_tile()` per visible tile at `(col * 10 + 30, row * 10)`
2. **Player/entity sprites** — blitted on top of tiles
3. **HUD panels** — `draw_player_status_panels()` draws weapon/stat panels
4. **HUD text** — `draw_game_hud()` draws text overlays (weapon names, cash, etc.)
5. **Minimap** (if enabled) — `g_minimap_enabled` controls visibility
6. **Page flip** — `swap_display_pages()`
7. **Screen shake** — CRTC start address offset

## Palette System

### Palette Buffer
768 bytes at `DAT_1038_0688` — 256 colors x 3 bytes (R, G, B).

### Setting Palette
`set_palette(0x2FF, palette_ptr)`:
- Writes to VGA DAC: `out(0x3C8, 0)` then loop `out(0x3C9, byte >> 2)` for all 768 bytes
- The `>> 2` converts from 8-bit (0-255) to 6-bit VGA DAC values (0-63)

### Palette Transitions
- `palette_fade_in(steps, palette_ptr)` — linear interpolation from black to target over N steps (typically 7). Calls `wait_vertical_retrace()` between steps.
- `palette_fade_out(steps, palette_ptr)` — reverse: target to black. Ends with `clear_palette_to_black()`.
- `clear_palette_to_black()` — fills all 768 bytes with 0.
- `set_palette_to_white()` — fills all 768 bytes with 0xFF.

Fade formula per step `i` of `steps`:
```
fade_in:  color[j] = (target[j] / steps) * i
fade_out: color[j] = (target[j] / steps) * (steps - i)
```

## Sprite Sheet & Tile Extraction

### Process (load_tile_resources, seg_1010:4599)
1. Load `SIKA.SPY` (in-game sprite sheet) → VRAM + palette buffer
2. Capture full screen as backup sprite
3. Extract 100+ individual 10x10 sprites from known grid positions
4. Extract larger composite sprites for HUD elements, menu cursor (shovel)
5. Extract 12 player color variant sprite sets

**Note:** The sprite sheet is `SIKA.SPY`, NOT `SHAPET.SPY`. SHAPET.SPY is a tile-type help/reference chart with text descriptions. The string at `0x1030:0x7972` resolves to `sika.spy` (MB.EXE offset 0x01AA73).

### Tile Grid
10x10 pixel tiles extracted via `load_sprite_from_sheet(stack, x, y)` → calls `calc_sprite_size` + `rtl_getmem` + `capture_screen_region`.

Row 0 (y=0): 16 basic terrain tiles (x = 0, 10, 20, ... 150)
Row 1 (y=10): Special tiles + items
Row 2 (y=20): Player sprites start
Row 3+ (y=30+): Mixed special sprites, effects

### Player Sprite Sets
`load_player_sprite_set(stack, dest, base_y, base_x)`:
- 4 directions x 4 animation frames = 16 sprites per set
- Vertical stride between directions: 40 pixels (0x28)
- Horizontal stride between frames: 10 pixels
- Each sprite: 10x10 pixels

**12 color variants** loaded from different sprite sheet positions:

| Variant | Sheet Y | Sheet X | Destination |
|---------|---------|---------|-------------|
| 1 | 10 | 0xA0 (160) | 0x1C0C |
| 2 | 0 | 0xA0 | 0x1D16 |
| 3 | 30 | 0xA0 | 0x1E20 |
| 4 | 40 | 0xA0 | 0x1F2A |
| 5 | 200 | 0xA0 | 0x1C4C |
| 6 | 200 | 0 | 0x1D56 |
| 7 | 210 | 0 | 0x1E60 |
| 8 | 210 | 0xA0 | 0x1F6A |
| 9 | 50 | 0xA0 | 0x2034 |
| 10 | 60 | 0xA0 | 0x213E |
| 11 | 70 | 0xA0 | 0x2352 |
| 12 | 80 | 0 | 0x245C |

Variants 9-12 have additional mirrored copies at +0x40 offset.

### Larger HUD/Menu Sprites
Captured as arbitrary rectangles from the sprite sheet (SIKA.SPY):
- `DAT_1038_0678` — menu cursor (shovel), 65x20 pixels at (150, 140). Drawn at X=222, Y=136/184/232/280
- `g_spr_life_full` / `g_spr_life_empty` — life bar indicators (~20x11 pixels)
- Status panel sprites — ~30x90 pixel regions
- Wall edge overlays — 4x4, 4x10, 10x4 pixel pieces

## Image Format Support

Two image loaders:

| Format | Function | Use |
|--------|----------|-----|
| `.SPY` (custom RLE) | `load_and_display_image` (seg_1010:4280) | Sprite sheets, title screen, backgrounds |
| PCX | `FUN_1010_73bd` (seg_1010:4367) | `.PPM` portrait files |

See [File Formats](../formats/file-formats.md) for detailed format specs.
