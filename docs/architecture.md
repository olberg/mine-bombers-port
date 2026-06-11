# Architecture overview

A short orientation for developers new to this codebase. The project is a
faithful recreation of **Mine Bombers 3.11** (DOS, Borland Pascal) in
**C99 + Raylib 5.5**, built with CMake/MinGW. The port reads the original,
unmodified game data files at runtime — nothing is pre-converted.

## Big picture

The game is one executable driven by a single state machine in
`src/main.c` (`STATE_INIT → TITLE → MENU → … → GAMEPLAY → RESULTS`, plus
side states for options, info, key/sound config, map picker, hall of fame,
and the single-player endings). Everything renders into a 640x480
`RenderTexture2D` — the original's VGA resolution — which is scaled 2x
onto a fixed 1280x960 window each frame.

A match runs as a loop of rounds: load map → shop → round loop
(input → movement → weapons/bombs → entities/AI → death checks → draw) →
results. The round loop lives in `src/game/round.c`; `main.c` owns the
between-screens flow.

## Directory map

| Path | What lives there |
|------|------------------|
| `src/main.c` | Entry point, window/render setup, game state machine, match flow, logging (`minebombers.log`) |
| `src/game/` | All gameplay: `round.c` (per-frame round loop), `map.c` + `map_renderer.c` (45x64 tile map, 3 layers), `movement.c` (player movement/digging), `bombs.c` + `weapons.c` (27 weapon types, fuses, detonation), `entity.c` + `ai.c` (monsters, spiral-search pathfinding), `player.c` (player struct + per-round resets), `shop.c`, `player_select.c`, `results.c`, `hall_of_fame.c`, `menu.c`, `title.c`, `options.c`, `hud.c`, `visibility.c` (fog of war), `player_db.c` (PLAYERS.DAT roster), `config.c` (OPTIONS.CFG), `music_schedule.c` (original song-position scheduling) |
| `src/loaders/` | Original binary format readers: `spy_loader.c` (.SPY sprites, RLE + 4 bitplanes), `font_loader.c` (.FON 8x8 bitmap font), `voc_loader.c` (.VOC sound effects → PCM), `pcx_loader.c` (.PPM portraits, PCX-encoded), `sprite_sheet.c` (tile/sprite extraction) |
| `src/gfx/` | `palette.c` — VGA palette handling and fades (7-step, as the original) |
| `src/audio/` | `music.c` (S3M playback via libxmp), `sfx.c` (VOC effects via Raylib) |
| `src/input/` | Two input layers: menu-level actions (`input_pressed`) and per-player 8-action key bindings matching the original (`player_input_*`), plus a scripted-injection mode used by the test harness |
| `src/util/` | `prng.c` — exact Borland Pascal 7 LCG (`x*0x08088405+1`), so random sequences can match the original |
| `src/autoplay.c` | Unattended bot-match harness (`MB_AUTOPLAY=1` env), used for soak testing and screenshots; writes a JSONL trace |
| `src/debug_overlay.c` | `--debug` flag: F3-toggled diagnostic overlay |
| `tests/` | Unity (ThrowTheSwitch) unit tests, one exe per area; `run_mp_soak.cmake` runs the real game in autoplay for determinism + completion checks |

## Key concepts

- **Fidelity tiers.** Game mechanics (movement, collision, weapons,
  scoring, AI, timing) are ported 1:1 from the original — no rebalancing.
  Scaffolding (menus, state flow) follows the original design with modern
  idioms. Utility code only needs identical observable results.
- **Evidence-based comments.** Comments cite the original executable as
  evidence: `seg_XXXX:YYYY` references are segment:offset locations in the
  decompiled MB.EXE; `FUN_XXXX_YYYY` names refer to functions there.
  When the port deliberately deviates (or replicates an original quirk),
  the comment says so.
- **Original data, loaded live.** The exe expects an `assets/` directory
  (the original game's files, user-supplied) next to it; see the README.
  Saves/config are written back in the original binary formats, so files
  stay interchangeable with the DOS game.
- **Timing.** The render loop targets 60 FPS; original mechanics tied to
  the DOS PIT timer (18.2065 Hz ticks) are converted with exact integer
  accumulators rather than floating-point per-frame drift.
- **Determinism.** Game randomness goes through `mb_random()` (seedable
  with `MB_SEED`), which the soak test uses to assert byte-identical
  round traces across runs.

## Building and testing

See the README for build steps and game-data setup. `ctest --test-dir
build` runs everything; unit tests load real original assets from
`build/tests/assets/`, and the soak test opens a real game window.
