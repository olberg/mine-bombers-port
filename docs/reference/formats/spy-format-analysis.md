# SPY Format Analysis

## File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 768 | Palette: 256 entries x 3 bytes (R, G, B), 8-bit values (0-255) |
| 768 | variable | 4 RLE-compressed bitplanes, decoded sequentially |

## Confirmed Format: VGA Mode 12h (640x480, 16 colors, 4 bitplanes)

- **Resolution**: 640x480
- **Colors**: 16 (4-bit index from 4 bitplanes)
- **Plane size**: 38400 bytes = 80 bytes/row x 480 rows
- **Pixel encoding**: Each byte in a plane = 8 pixels (1 bit per pixel, MSB first)
- **Color index**: 4 planes combine — plane N contributes bit N of the 4-bit color index
- **Palette**: Only first 16 entries used; entries 16-255 are magenta filler (255, 31, 255)
- **Palette values**: Already 8-bit (0-255), NOT 6-bit VGA DAC — no `vga6to8` conversion needed

## RLE Compression

Each plane is independently RLE-compressed in the file:
- Byte `0x01` = RLE marker → next byte = value, next byte = count
- Any other byte = literal pixel data
- Planes are stored sequentially: plane 0, then 1, 2, 3

## Bitplane-to-Pixel Conversion

```c
for (int y = 0; y < 480; y++) {
    for (int x = 0; x < 640; x++) {
        int byte_off = y * 80 + x / 8;
        int bit = 7 - (x % 8);  // MSB first
        uint8_t color = 0;
        for (int p = 0; p < 4; p++) {
            color |= ((planes[p][byte_off] >> bit) & 1) << p;
        }
        pixels[y * 640 + x] = color;  // 0-15
    }
}
```

## Format notes

- The palette is **already 8-bit**: in TITLEBE.SPY, 516 of 768 palette
  values exceed 63, so no VGA 6-bit→8-bit DAC conversion is applied.
- The pixel data is **bitplanes, not chunky bytes**: interpreting plane
  bytes as 256-color indices yields bit-pattern garbage (0xFF, 0xAA,
  0x55) and a mostly-magenta image, since entries 16-255 are filler.

## SIKA.SPY Sprite Extraction

The original game loads SIKA.SPY (NOT SHAPET.SPY) to VGA VRAM via `load_tile_resources()`, then uses `capture_screen_region()` to extract individual sprites from it. SHAPET.SPY is a help/reference chart, not the gameplay sprite sheet.

### Grid-based tiles (via `load_sprite_from_sheet`)
- 10x10 pixel grid, each tile is 9x9 pixels (1px gap)
- `load_sprite_from_sheet(_, row_y, col_x)` → `capture_screen_region(buf, row_y+9, col_x+9, row_y, col_x)`
- First row (Y=0): 16 tiles at X=0,10,20,...,150
- Second row (Y=10): tiles at various X positions
- Rows go up to Y=0x46 (70)

### `capture_screen_region` parameter order
```c
capture_screen_region(buffer, Y_end, X_end, Y_start, X_start)
```
Stores: `buffer[0] = X_end - X_start` (width), `buffer[1] = Y_end - Y_start` (height)

### Larger sprites (via direct `capture_screen_region`)
Many sprites are captured after the grid tiles, using explicit coordinate rectangles.

## Menu Cursor (DAT_1038_0678)

The shovel cursor is a 65x20 sprite captured from **SIKA.SPY** (the
in-game sprite sheet, not SHAPET.SPY):

```c
// seg_1010_graphics.c (inside load_tile_resources)
capture_screen_region(DAT_1038_0678, 0xa0, 0xd7, 0x8c, 0x96);
// Params: Y_end=160, X_end=215, Y_start=140, X_start=150
// Result: 65x20 sprite at (150, 140)
```

- Gray horizontal bar with a shovel/spade icon (handle left, blade right)
- Drawn at menu positions X=222, Y=136/184/232/280 (4 menu items)
- Color index 0 = transparent (menu background shows through)
- The port extracts it the same way from the SIKA.SPY indexed buffer

For the full list of SPY files and their contents, see
[SPY Files Summary](spy-files-summary.md). All filenames are confirmed as
strings in the MB.EXE binary (e.g. `main3.spy` at EXE offset 0x23C7).
