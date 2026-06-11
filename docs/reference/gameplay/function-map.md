# Function Map

Named and key unnamed functions across all segments, grouped by module.

## Segment 1000 — Game Logic (seg_1000_game_logic.c)

### Entry & Main Loop
| Line | Function | Purpose |
|------|----------|---------|
| 6985 | `entry()` | Main entry point. Init sequence → main loop |
| — | `main_menu()` | Display and handle main menu |
| — | `process_menu_selection()` | Act on menu choice |
| — | `init_game_state()` | Initialize game state for new match |

### File I/O
| Line | Function | Purpose |
|------|----------|---------|
| 6 | `FUN_1000_000e` | Save game configuration (17 bytes) |
| 867 | `FUN_1000_15c7` | Read/update individual player record in players.dat |
| 1788 | `level_file_select` | Create new players.dat with 32 empty records |
| 1817 | `FUN_1000_2dad` | Check file existence, return file size |
| 1863 | `FUN_1000_2e4e` | Read player slot selections (4 bytes) |
| 1916 | `FUN_1000_2f9f` | Write player slot selections (4 bytes) |
| 2096 | `FUN_1000_3276` | Player select screen — bulk load/save players.dat (3232 bytes) |
| 2963 | `load_level` | Load level map from text file (.MNL/.MNE) |
| 4315 | `FUN_1000_6e4c` | Load sound card configuration (6 bytes) |
| 4374 | `FUN_1000_6fcd` | Load joystick config (joystic1.cfg, joystic2.cfg) |
| 6713 | `FUN_1000_a619` | Load high score table (260 bytes) |
| 6748 | `FUN_1000_a6d7` | Save high score table (260 bytes) |

### Player & Movement
| Line | Function | Purpose |
|------|----------|---------|
| — | `move_player` | Player physics and movement |
| — | `animate_player_sprite` | Character animation state machine |
| — | `player_collision_check` | Collision with map tiles |
| — | `monster_player_collision` | NPC/monster collision |
| — | `process_player_input` | Read keyboard state for all players |
| — | `process_all_key_inputs` | Process key inputs with configurable params |

### Combat & Weapons
| Line | Function | Purpose |
|------|----------|---------|
| — | `process_weapons` | Bomb placement, detonation, damage |
| — | `check_player_death` | Death condition checking |
| — | `game_tick_update` | Per-tick game state advancement |
| — | `game_state_update` | Broader game state update |

### AI
| Line | Function | Purpose |
|------|----------|---------|
| — | `move_entity_toward_target` | Pathfinding for monsters |

(`apply_bot_ai`, formerly listed here as the "bot AI entry point", is actually the shop screen in seg_1010 — see below. The decompiled name is a mislabel; the game has no bot players.)

### Scoring & Results
| Line | Function | Purpose |
|------|----------|---------|
| 6459 | `FUN_1000_a17c` | Multiplayer round-end scoring (pool, shares, round wins, welfare floor) |
| ~6087 | (around FUN_1000_9778) | Results screen with PPM portraits |
| — | `player_select_screen` | Player setup / character select |

### Rendering (calls into seg_1010/1028)
| Line | Function | Purpose |
|------|----------|---------|
| — | `redraw_game_screen` | Full game screen redraw |
| 7037 | (inline) | `load_and_display_image` for title screen |

## Segment 1008 — Sound & Keyboard (seg_1008_sound.c)

### Sound System Init
| Line | Function | Purpose |
|------|----------|---------|
| 882 | `load_music_module` | Load S3M module + init all instruments (GUS/OPL path) |
| 1660 | `detect_sound_blaster` | Auto-detect SB card (probe ports 0x210-0x260) |
| 1550 | `init_dma_irq` | Initialize SB DMA and IRQ for playback |
| 2791 | `FUN_1008_3574` | Init DMA double-buffer system (SB path) |

### Sound Loading
| Line | Function | Purpose |
|------|----------|---------|
| 811 | `load_instrument` | Load raw PCM sample, program into OPL3 |
| 1020 | `load_sound_effect` | Load .VOC file as instrument |
| 2053 | `FUN_1008_27f8` | Load S3M module for SB DMA playback |
| 2677 | `FUN_1008_33af` | Load file into memory (13-byte descriptor + data) |
| 2753 | `FUN_1008_34dc` | Free sample descriptor and data buffer |

### Sound Playback
| Line | Function | Purpose |
|------|----------|---------|
| 1966 | `FUN_1008_269a` | Load sound sample with descriptor struct |
| — | `FUN_1008_23f1` | Enable SB playback |
| — | `FUN_1008_2402` | Disable SB playback |

### Keyboard
| Line | Function | Purpose |
|------|----------|---------|
| — | `install_keyboard_handler` | Hook INT 0x09 |
| — | (interrupt handler) | Scancode processing, updates global key state |

## Segment 1010 — Graphics & Config (seg_1010_graphics.c)

### Image Loading
| Line | Function | Purpose |
|------|----------|---------|
| 4280 | `load_and_display_image` | Load custom .SPY image (RLE + palette) |
| 4367 | `FUN_1010_73bd` | Load PCX image, render to screen |
| 4500 | `FUN_1010_7785` | PCX loader wrapper with error handling |
| 4142 | `FUN_1010_6fda` | File existence check |
| 4207 | `FUN_1010_70a6` | Buffered file read helper |

### Sprite System
| Line | Function | Purpose |
|------|----------|---------|
| 4599 | `load_tile_resources` | Load SIKA.SPY sprite sheet, extract 100+ individual sprites |
| — | `load_player_sprite_set` | Extract player sprite grid (4 directions x 4 frames) |
| — | `load_sprite_from_sheet` | Capture 10x10 region from VRAM as sprite |
| — | `calc_sprite_size` | Calculate memory for sprite capture |
| — | `capture_screen_region` | Copy VRAM region to allocated sprite buffer |

### VGA / Palette
| Line | Function | Purpose |
|------|----------|---------|
| — | `init_music_playback` | Init music playback (SB/GUS/none) |
| — | `enable_vga_display` | **Mislabeled**: actually enables music (mode 0) — see [Music System](../audio/music-system.md) |
| — | `setup_graphics_pages` | Configure VGA page flipping |
| — | `swap_display_pages` | **Mislabeled**: actually disables music (mode 0) — see [Music System](../audio/music-system.md) |
| — | `set_palette` | Set VGA palette from buffer |
| — | `palette_fade_in` | Gradual palette fade in |
| — | `palette_fade_out` | Gradual palette fade out |
| — | `clear_palette_to_black` | Zero entire palette |

### Configuration
| Line | Function | Purpose |
|------|----------|---------|
| 5499 | `load_game_config` | Read 17-byte config file |
| 5476 | `FUN_1010_9c57` | Set config defaults |
| 5641 | `FUN_1010_9fbb` | Load key bindings (text, 32 values) |
| 7103 | `FUN_1010_c1c5` | Save key bindings |
| — | `key_config_screen` | Key binding UI |
| — | `per_player_update` | Per-round player state updates |

### Sound Init
| Line | Function | Purpose |
|------|----------|---------|
| 2940 | `FUN_1010_5860` | Init sound system, load VOC/S3M (SB path) |
| 3110 | `FUN_1010_5c83` | Load match audio at PLAY: OEKU.S3M + the 12 game VOCs |

### Game State
| Line | Function | Purpose |
|------|----------|---------|
| 100 | `FUN_1010_02fc` | Load/validate saved game with checksums |
| 6987 | `apply_bot_ai` | **Shop screen** — renders one page with up to two player purchase panels (first param: 1 = paired, 0 = solo). The decompiled name is a mislabel from an older analysis pass |
| — | `FUN_1010_c5f4` | Game state update (called from round loop) |
| — | `FUN_1010_c15c` | Pre-render game state update |
| — | `FUN_1010_12c4` | Post-init game state setup |

## Segment 1018 — System Utilities (seg_1018_system.c)

### Font & Text
| Line | Function | Purpose |
|------|----------|---------|
| 1927 | `draw_glyph` | Render single 8x8 character |
| 2055 | `load_font_file` | Load .FON file (2048 bytes) |
| 2095 | `load_font` | Wrapper — calls load_font_file with hardcoded name |
| — | `print_string_at` | Draw string at screen position |

### Music Module Loader
| Line | Function | Purpose |
|------|----------|---------|
| 169 | `FUN_1018_0350` | Core S3M file loader (header, patterns, instruments) |
| 92 | `FUN_1018_01fc` | S3M packed pattern data reader |
| — | `FUN_1018_0656` | Instrument period calculation |
| — | `FUN_1018_06e5` | Sample data offset handling |
| — | `FUN_1018_0afb` | Channel note processing (16 channels) |

### Timer & System
| Line | Function | Purpose |
|------|----------|---------|
| — | `init_timer` | Set up DOS timer interrupt |
| 418 | `FUN_1018_0855` | Music: request jump to song order n at next pattern boundary (see [Music System](../audio/music-system.md)) |
| — | `FUN_1018_2ede` | System tick handler |
| — | `FUN_1018_33b3` | Read DOS environment variable |
| — | `delay_wait` | Busy-wait delay |

## Segment 1020 — BGI Stub (seg_1020_bgi.c)

| Line | Function | Purpose |
|------|----------|---------|
| — | `FUN_1020_009b` | BGI EGAVGA driver initialization (stub, not used for rendering) |

## Segment 1028 — Graphics Primitives (seg_1028_gfx_primitives.c)

| Function | Purpose |
|----------|---------|
| Sprite blit routines | Copy sprite data to VRAM with clipping |
| `FUN_1028_1bfc` | Draw horizontal line (used in PCX decompression) |
| `FUN_1028_1f61` | Draw single pixel |
| `set_draw_color` | Set current drawing color index |
| Rectangle fill | Fill rectangular VRAM region |
| Screen capture | Copy VRAM region to memory buffer |

## Segment 1030 — Pascal RTL (seg_1030_pascal_rtl.c)

### File I/O
| Line | Function | Purpose |
|------|----------|---------|
| 881 | `rtl_assign` | Assign filename to file record |
| 919 | `rtl_reset` | Open file for reading |
| 956 | `rtl_rewrite` | Open/create file for writing |
| 993 | `rtl_close` | Close file |
| 1087 | `rtl_blockread` | Block read from file |
| 1141 | `rtl_blockwrite` | Block write to file |
| 1194 | `rtl_seek` | Seek to record position |
| 2888 | `rtl_filesize` | Get file size in records |
| 432 | `rtl_ioresult` | Get and clear last I/O error |
| 446 | `rtl_ioresult2` | Check I/O result, halt on error |

### Memory
| Function | Purpose |
|----------|---------|
| `rtl_getmem` | Allocate memory (Pascal GetMem) |
| `rtl_freemem` | Free memory (Pascal FreeMem) |

### Strings
| Function | Purpose |
|----------|---------|
| `rtl_strcopy` | String copy |
| `rtl_strop` | String concatenation |
| `FUN_1030_0dac` | String from address |

### Runtime
| Function | Purpose |
|----------|---------|
| `rtl_init` | Runtime initialization |
| `rtl_exit` | Runtime cleanup / program exit |
| `rtl_stack_check` | Stack overflow check (called at function entry) |
| `FUN_1030_0378` | Get free memory amount |

## Cross-Segment Call Patterns

Key call chains across segments:

```
entry() [1000]
  ├→ detect_sound_init() [1008]
  ├→ install_keyboard_handler() [1008]
  ├→ load_font() [1018]
  ├→ init_music_playback() [1010]
  ├→ load_and_display_image() [1010]
  ├→ load_tile_resources() [1010]
  │   ├→ load_and_display_image() [1010] — SIKA.SPY sprite sheet
  │   └→ capture_screen_region() [1028]
  ├→ main_menu() [1000]
  │   └→ process_menu_selection() [1000]
  ├→ FUN_1010_5860() [1010] — sound init
  │   ├→ FUN_1008_27f8() [1008] — S3M loader (SB)
  │   ├→ FUN_1008_33af() [1008] x12 — VOC loader
  │   └→ FUN_1008_3574() [1008] — DMA init
  ├→ load_level() [1000]
  │   └→ rtl_* file ops [1030]
  ├→ apply_bot_ai() [1010] — shop screen, before each round (mislabeled name)
  └→ game frame loop [1000]
      ├→ move_player() [1000]
      ├→ process_weapons() [1000]
      ├→ check_player_death() [1000]
      └→ redraw_game_screen() [1000]
          └→ sprite blit routines [1028]
```
