# Multiplayer Camera / Viewport

How the original handles the view of the map — verified against the
decompile and the port's side-by-side DOSBox comparisons.

## Fixed view — no camera

The original has **no scrolling camera and no split-screen**. The entire
map is visible at once, and all players share the same fixed 640x480 view:

- Screen geometry (VGA Mode 12h, 640x480): the map's **64-tile axis runs
  horizontally** (`screen X = row * 10`, 0–630) and the **45-tile axis
  vertically** (`screen Y = col * 10 + 30`, 30–470), below a 30-pixel HUD
  strip at the top. 64x10 = 640 and 45x10 + 30 = 480 — the map fills the
  screen exactly.
- The BGI viewport is initialized once with offsets (0,0)
  (`FUN_1028_12d3`, seg_1028:1080, called from seg_1028:787) and never
  modified during gameplay. Sprites are drawn at absolute map coordinates;
  `blit_sprite` (seg_1028:1298) clips against the viewport edges.
- There is no camera update, scroll calculation, or viewport
  repositioning anywhere in the frame loop (seg_1000:7139–7289).

## Screen shake — the only CRTC manipulation

The only CRTC Start Address manipulation during gameplay is screen shake
(seg_1010:7705–7725): on odd shake values the start address is offset by
`shake_value * 0x50` bytes (one row per unit), and it resets to 0 when the
shake ends. It is never used for scrolling.

## HUD layout

| Element | Position |
|---------|----------|
| Status panels (P1–P4) | X = 12 / 174 / 337 / 500, Y = 0 |
| Player names | Y = 1, X = 32 / 194 / 357 / 520 |
| Timer bar (multiplayer only) | rows Y = 473–477, spanning X = 2–637 |

## Port mapping

The port renders the same fixed 640x480 view (`src/game/map_renderer.c`)
with no camera. Screen shake is implemented as a vertical draw offset
(`y_offset`) applied during map drawing instead of CRTC writes.
