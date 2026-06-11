# Game Flow and State Machine

Traced from `entry()` in seg_1000:6985.

## Initialization Sequence

```
entry()
  INITTASK()                    — DOS task initialization
  rtl_init()                    — Pascal runtime library init
  init_filesystem()             — File I/O setup
  detect_sound_init()           — Sound hardware detection (SB/GUS)
  FUN_1020_009b()               — BGI graphics driver stub
  install_keyboard_handler()    — Hook INT 0x09 (keyboard interrupt)
  load_font()                   — Load .FON bitmap font (2048 bytes)
  keyboard_setup()              — Key binding configuration
  clear_palette_to_black()
  setup_graphics_pages()        — VGA page setup
  load_game_resources()         — Load assets from disk
  init_data_structures()        — Initialize game state arrays
  init_music_playback()              — Init music playback (SB/GUS/none)
  enable_vga_display()
  load_and_display_image()      — Show title screen (titlebe.spy)
  palette_fade_in()
  init_timer()                  — Set up DOS timer interrupt
  init_game_state()
  palette_fade_out()
  load_tile_resources()         — Load SIKA.SPY sprite sheet, extract all tile/player/menu sprites
```

## Main Game Loop

```
while (g_quit_flag == 0):
    init_music_playback()
    enable_vga_display()
    main_menu()                 — Display main menu, wait for selection
    process_menu_selection()    — Handle menu choice
    g_rounds_remaining = g_total_rounds
    setup_graphics_pages()
    swap_display_pages()
    FUN_1010_50b6()             — Pre-round setup
    FUN_1010_5c83()             — Load sound effects (if not loaded)
    FUN_1000_3276()             — Player select screen (players.dat)

    if quit: g_rounds_remaining = 0

    for each round (while g_rounds_remaining > 0):
        [ROUND LOOP - see below]

    [POST-MATCH - scoring/results]
```

## Round Loop (per round)

```
setup_graphics_pages()
set_palette()

if single_player (num_players == 1):
    load_level(level_filename)   — Load .MNL campaign level
    FUN_1000_a4af()              — Single-player level init
elif multiplayer with a picked map (this round's map-picker slot < 30000; 32000 = "Random" cell):
    load_level(map_filename)     — Load .MNE map file
else:
    FUN_1008_1263()              — Generate random level

clear_palette_to_black()
game_state_update()
enable_vga_display()

— Shop screens. The decompiled name "apply_bot_ai" is a mislabel (an older
— analysis pass misread it — there are no bots): the function renders one
— shop page with up to two player purchase panels. Param 1 = paired, 0 = solo.
if num_players == 1: shop page (P1 solo)
if num_players == 2: shop page (P1 + P2)
if num_players == 3: shop page (P1 + P2), then shop page (P3 solo)
if num_players == 4: shop page (P1 + P2), then shop page (P3 + P4)

swap_display_pages()
redraw_game_screen()
palette_fade_in()

— Per-frame game loop
while (g_round_over == 0):
    [FRAME LOOP - see below]

palette_fade_out()
g_rounds_remaining -= 1
```

## Frame Loop (per frame)

```
— Check special keys. The decompiled key names are misleading — re-derived
— from MB.EXE bytes: "g_key_mp_sync" is ESC, "g_key_esc" is F10, and
— "g_key_screen_toggle" is F5, which toggles MUSIC (the "swap_display_pages"
— call it gates is actually disable_music).
if ESC && num_players > 1:              — ends the current round
    palette_fade_out()
    g_round_over = 1

if P: FUN_1000_7194()                   — Pause handler
if F5: toggle music
if F10:                                  — aborts the whole match
    palette_fade_out()
    g_rounds_remaining = 0
    g_round_over = 1

— Process input
process_player_input()
game_tick_update()

— Every 5th frame: process AI and check win/lose conditions
if frame_counter % 5 == 0:
    process_all_key_inputs(0,0,1,1,0)
    check if fewer than 2 players alive → start inactivity counter
    single-player death: decrement lives, add retry round

— Per-player update (for each alive player 1-4):
    move_player()
    if speed bonus: move_player() again  — double movement for fast players
    every 2nd frame:
        process_weapons()
        check_player_death()

— Rendering
redraw_game_screen()
if minimap_enabled: draw minimap
```

## Game States

| State | Entry Condition | Exit Condition |
|-------|----------------|----------------|
| **Title Screen** | Program start | Any key → Main Menu |
| **Main Menu** | After title, after match ends | Menu selection or F10 quit |
| **Options Menu** | Selected from main menu | "Mainmenu" option → Main Menu |
| **Player Select** | Before match starts | All players selected → Shop/Round |
| **Shop** | Before each round, both modes (the single-player campaign starts in the shop too) | All players press Leave/ESC → Round |
| **Round (Gameplay)** | After shop / level load | Time runs out, <2 alive, all treasures collected, ESC |
| **Round Results** | After round ends | Auto-advance to next round/shop |
| **Match Results** | All rounds complete | Any key → Main Menu |
| **Hall of Fame** | Single-player game over | Any key → Main Menu |
| **Pause** | Pause key during gameplay | Pause key again → Resume |

### Options Menu (FUN_1000_1017 / FUN_1000_0140, DOSBox-verified 2026-06-11)

14 rows, cursor (27x11 arrow from SIKA.SPY at 205,99) at X=217,
Y=(item+4)·24+6, **starting on row 13 (MAINMENU)**. Background
`OPTIONS5.SPY` (all labels baked in); reference capture:
`docs/reference/corpus/options-defaults.png`.

| Row | Label | Value display | Bar fill offset (white, X 334..334+w incl.) |
|-----|-------|---------------|----------------------------------------|
| 0 | CASH | number | `Trunc(cash/16)` (max 2650) |
| 1 | TREASURES | number | `Trunc(v*2.2)` |
| 2 | ROUNDS | number | `v*3` (max 54: bound `v*3 < 0xA5`) |
| 3 | TIME | `M:SS min` of REAL seconds = `Trunc(ticks·65536/1193182)` | `Trunc(ticks/150)` (max 0x60AE) |
| 4 | PLAYERS | number | `(n-1)·0x37` |
| 5 | SPEED | `(33-frame_delay)·3+1` + `%` | `(33-fd)·5` |
| 6 | BOMB DAMAGE | number + `%` (0-100) | `Trunc(v·1.65)` |
| 7-10 | DARKNESS / FREE MARKET / SELLING / WINNER | coin sprites | dim coin (SIKA 90,40 15x13) at X=377 AND 443; bright coin (90,53) over the active side. Rows 7-9: on→377; row 10 INVERTED: by-money(0)→377 |
| 11 | REDEFINE KEYS | opens key config (FUN_1010_d4c4) | |
| 12 | LOAD LEVELS | opens map select (FUN_1010_e231) | |
| 13 | MAINMENU | exit (also ESC) | |

Bars/coins sit on 24px rows at Y=101+24i (13px tall); value text color 8 at
X=400 (rows 0-3) / 408 (rows 4-6), Y=103+24i; bar color 1 (white in the
OPTIONS5 palette). `D` resets defaults. Keys: arrows/numpad 2-4-6-8
navigate/adjust; Enter / numpad 3 / PgDn activate. There is NO sound-config
row — the original configured sound in SETUP.EXE.

## Single-Player Mode

- 15 levels, loaded from `.MNL` files
- 3 starting lives (g_sp_lives_remaining)
- Player starts at upper-left corner
- Goal: find the exit door (black square with grey borders)
- On death: lose 1 life, retry same level (g_rounds_remaining incremented)
- On 0 lives: game over → Hall of Fame check
- Opposition comes from map-spawned monsters — there is no bot player (the "bot AI" in older revisions of this page was a misread of the shop function)

## Multiplayer Mode

- 1-4 players on same keyboard/joystick
- Configurable number of rounds
- Each round: shop → gameplay → scoring
- Round ends when: time expires, <2 players alive, all treasures collected
- Winner determined by: most cash OR most individual round wins (configurable)
- All player slots are human-controlled — there are no AI-controlled player slots

## Sound System Initialization

Two code paths depending on detected hardware:

```
if SB detected (DAT_1038_1bda == 1/2/3):
    FUN_1008_27f8()             — Load S3M via SB module loader
    12x FUN_1008_33af()         — Load 12 VOC files to memory
    FUN_1008_3574()             — Init DMA double-buffer playback
elif GUS detected (DAT_1038_1bda == 0):
    load_music_module()         — Load S3M via GUS/OPL loader
    12x load_sound_effect()     — Load 12 VOC files as OPL instruments
elif no sound (DAT_1038_1bda == 4):
    delay_wait(1)               — No-op
```

## Exit Sequence

```
g_quit_flag = 1  (set by F10 or quit menu option)
→ breaks main loop
→ palette_fade_out()
→ restore keyboard interrupt (INT 0x09)
→ restore VGA text mode
→ rtl_exit()
```
