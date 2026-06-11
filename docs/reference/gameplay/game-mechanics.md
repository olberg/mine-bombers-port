# Game Mechanics

Reverse-engineered from `seg_1000_game_logic.c` and cross-referenced with other segments.

## Map System

### Dimensions
- **45 columns (0x2D) x 64 rows (0x40)**
- Each tile = 10x10 pixels
- On screen the 64-valued index runs horizontally (640 px) and the 45-valued index vertically (450 px); the playfield starts 30 pixels (0x1E) down, below the HUD strip (450 + 30 = 480)
- Total tiles: 2,880

### Map Layers
Three parallel arrays in the data segment:

| Layer | Type | Row Stride | Purpose |
|-------|------|------------|---------|
| `g_tile_map` | byte | 0x2D (45) | Tile type ID |
| `g_collision_map` | word (2B) | 0x5A (90) | Hit points / collision value |
| `g_overlay_map` | word (2B) | 0x5A (90) | Timer / fuse countdown |

Access patterns:
```
g_tile_map[row * 0x2D + col]           // tile type (byte)
g_collision_map[row * 0x5A + col * 2]  // collision HP (word)
g_overlay_map[row * 0x5A + col * 2]    // timer/fuse (word)
```

Bounds check: if row >= 64 or col >= 45, returns 0x30 (empty floor).

## Tile Types

### Passable Tiles (allow movement)
| Value | Hex | Description |
|-------|-----|-------------|
| '0' | 0x30 | Empty floor |
| 'f' | 0x66 | Death marker / corpse |
| — | 0xAF | Floor variant |

### Indestructible Walls
| Value | Hex | Description |
|-------|-----|-------------|
| '1' | 0x31 | Solid wall type 1 |
| '2'-'4' | 0x32-0x34 | Solid wall variants |
| '5' | 0x35 | Damaged wall visual (indestructible remnant) |

### Destructible Walls (collision HP applies)
| Value | Hex | HP Thresholds |
|-------|-----|---------------|
| '7'-'9' | 0x37-0x39 | < 500 → '5' (damaged), < 1000 → '6' (cracked) |
| 'A'-'F' | 0x41-0x46 | Same degradation as above |
| 'p' | 0x70 | < 500 → '5', < 1000 → 'q' |
| 'q' | 0x71 | Damaged alt-wall |

### Reinforced Walls
| Value | Hex | HP Thresholds |
|-------|-----|---------------|
| 0xAC | — | Full reinforced |
| 0xAD | — | < 4001 HP stage |
| 0xAE | — | < 2001 HP stage |

### Treasures & Pickups
| Value | Hex | Effect |
|-------|-----|--------|
| 0x8F | — | +1 stat gem |
| 0x90 | — | +3 stat gem |
| 0x91 | — | +5 stat gem |
| 0x92 | — | 15 cash |
| 0x93 | — | 25 cash |
| 0x94 | — | 15 cash |
| 0x95 | — | 10 cash |
| 0x96 | — | 30 cash |
| 0x97 | — | 35 cash |
| 0x98 | — | 50 cash |
| 0x99 | — | 65 cash |
| 0x9A | — | 100 cash |
| 's' (0x73) | — | **1000 cash** (special treasure) |
| 'm' (0x6D) | — | Full health restore |
| 0xB3 | — | +1 extra life (single-player) |
| 'y' (0x79) | — | Mystery box (random weapon drop) |

### Interaction Tiles
| Value | Hex | Effect |
|-------|-----|--------|
| 'k' (0x6B) | — | Exit door (single-player win) |
| 0x9C | — | Teleporter (random warp between all 0x9C tiles) |
| 0xB4, 0xB5 | — | Shop tiles |

### Monster Spawn Tiles
| Value Range | Hex | Description |
|-------------|-----|-------------|
| 'G'-'V' | 0x47-0x56 | Monster spawn points, 4 types x 4 directions |

## Weapons & Bombs

### Weapon Types

| ID | Hex | Inventory Offset | Fuse (frames) | Notes |
|----|-----|-----------------|---------------|-------|
| 'W' | 0x57 | 0xB0 | 100 | Small bomb. Stages: → 'w' (0x77) at <60 → 'x' (0x78) at <30 |
| 'X' | 0x58 | 0xB2 | 100 | Medium bomb. Stages: → 0x8B at <60 → 0x8C at <30 |
| 'Y' | 0x59 | 0xB4 | 80 | Large bomb. Stages: → 0x8D at <40 → 0x8E at <20 |
| 'Z' | 0x5A | 0xC0 | — | Directional rocket (calls FUN_1010_45cd) |
| 0x7F | — | 0xC2 | 260 | Mega bomb |
| 0x80 | — | 0xC8 | — | Weapon variant |
| 0x81 | — | 0xCA | — | Super bomb → urethane flood-fill (max 45 iters) |
| 'e' (0x65) | — | 0xBE | — | Explosive type |
| 0x8A | — | 0xC6 | — | Napalm (fire bomb, spreading fire) |
| 0x9D | — | 0xB6 | 280 | Mine (stages: → 0x9E → 0x9F → 0x9D) |
| 'r' (0x72) | — | 0xDE | — | Rocket launcher (calls FUN_1010_4dff) |
| 0xA1 | — | 0xCC | 90 | Timer bomb → urethane flood-fill on detonation |
| 0xB0 | — | 0xE4 | — | **Super drill** ("SUPERpora", HISTORIA v3.1): +300 digging-power loan, no tile placed |
| 0xA2 | — | 0xCE | — | Weapon variant |
| 0xA4 | — | 0xC4 | — | Weapon variant |
| 0xA5 | — | 0xBC | 1 | Directional arrow (immediate, direction-encoded) |
| 0xA9 | — | 0xD0 | 1 | Teleporter bomb (immediate) |
| 0x9C | — | 0xD8 | — | Weapon variant |
| 'n' (0x6E) | — | 0xDA | — | Creature spawner |
| 'o' (0x6F) | — | 0xDC | random 0-80 | Proximity mine |
| 0xAB | — | 0xE2 | random 80-160 | Random-fuse bomb |

### Weapon Cycling Order
W → X → Y → Z → 0x7F → 0x80 → 0x81 → ... → 0xAB → W (loops)

### Directional Arrow Encoding
Direction when placed determines tile value:
- Right: 0xA5
- Left: 0xA6
- Down: 0xA7
- Up: 0xA8

### Collision Values for Placed Items
- Directional arrows (0xA5-0xA8): collision = **0** (passable)
- Proximity mine 'o': collision = **400**
- Random mine (0xAB): collision = **random(20) + 7**
- All other weapons: collision = **20**

### Detonation
- **Timer bombs**: overlay value decrements every frame. At 0, explosion triggers.
- **Remote bombs**: player presses remote key → scans entire 45x64 map for matching bomb tiles → sets their overlay to 1 (instant detonation next frame).
- **Treasure drop**: 1.1% chance (11/1000) per explosion to spawn a random treasure.
- **Directional arrows** (0xA5-0xA8, seg_1010:1036-1102): On detonation, arrows **move** one tile in their direction instead of exploding. If destination is passable ('0', 'f', 0xAF, or same arrow tile) and no player present (FUN_1010_98dd returns 0), the arrow tile moves to the destination with overlay=2 (DOWN/RIGHT) or overlay=1 (UP/LEFT) to continue movement next tick. The asymmetric overlay values compensate for the iteration order (row 0→63, col 0→44): DOWN/RIGHT move into not-yet-processed tiles that get decremented same frame, UP/LEFT move into already-processed tiles that keep their overlay until next frame. When blocked, the arrow detonates as a small bomb (0x57) via recursive FUN_1010_165b call.
- **Mine cross-pattern** (0x9D-0x9F, seg_1010:1212-1246): Mines create a large diamond/cross explosion. Algorithm: (1) set_palette_to_white() flash, (2) FUN_1010_1326 at center with damage=0xFF, (3) iterate row offsets -12 to +12, at each row call random for horizontal extent, apply FUN_1010_1326 to all tiles in range, (4) screen shake += 10, (5) 3 explosion sounds (trigger #4). The random extent creates a diamond shape narrowing toward top/bottom. (exact random distribution not fully verified).
- **Proximity mine movement** ('o'/0x6F, seg_1010:2131-2166): On "detonation", mines do NOT explode. Instead they **move/spread** to an adjacent passable tile. Algorithm: (1) new fuse = random(140)+1, (2) direction = fuse%4 (0=row+1, 1=row-1, 2=col+1, 3=col-1), (3) if adjacent tile is passable ('0','f',0xAF), place a new mine copy with same fuse and collision=400, (4) current mine stays alive with the new fuse. This creates a creeping mine field that spreads across open floor.
- **Creature spawner flood-fill** (0xA1 seg_1010:1104-1211, 0x81 seg_1010:1849-1950): Timer bomb (0xA1) and super bomb (0x81) create an expanding area of creature spawner tiles via layered flood-fill. Algorithm: (1) mark center as 0x88, (2) iterate up to 50 (0xA1) or 45 (0x81) times scanning entire map for 0x88 tiles and placing 0x89 on adjacent passable tiles, (3) convert all 0x89→0x88 each iteration, (4) stop when no new tiles added, (5) final pass: all 0x88→0xA0 with collision=400 and overlay=0xFA (250), except center→'0'. Sound: trigger #10 (urethan).

### Explosion vs Walls (FUN_1010_1326, verified 2026-06-11)

Wall classification at seg_1010:772 covers **only intact walls** `'7'-'9'`
and `'A'-'F'`:

- **Weak hit on an intact wall** (seg_1010:799-809): collision SET to 500
  and tile → `'q'`, or (50%, `Random(2)`) collision 1000 and tile → `'p'`.
  Prior dig damage is overwritten, not accumulated.
- **Weak hit on a degraded wall** (`'p'`, `'q'`, `'5'`, `'6'`): NOT in the
  wall set — falls through to the fire branch (seg_1010:775-778) and is
  **destroyed**. This is the classic two-bomb wall kill.
- **Weak hit on reinforced** (0xAC/0xAD/0xAE, seg_1010:780-792): degrade
  one stage (collision 4000 → 2000 → destroyed).
- **Strong hit** (mines, mega/rocket expansion, param_2=1): any wall →
  fire immediately.

All standard bombs (small/medium/large/signature) are weak mode; the
BOMB DAMAGE % option plays no part in wall damage.

### BOMB DAMAGE % Option (seg_1010:5378-5386, verified 2026-06-11)

The options-screen BOMB DAMAGE % value (config offset 0x10) scales
**player** explosion damage in **multiplayer only**: with `num_players < 2`
the raw per-bomb damage is subtracted; otherwise the subtraction routes
through the Pascal real RTL (FUN_1030_1545/1537/1549 = load → multiply →
Trunc), i.e. `health -= Trunc(damage * pct/100)`. Entities/monsters always
take raw damage (seg_1010:5442). Walls are unaffected (stage-based, above).

### Dig Damage Formula (seg_1000:3722-3724)
```
collision_value -= (digging_power + bonus_stat)
```
Where (using player pointer-indirect offsets):
- **digging_power** (+0xA8): Starts at 1 per round (seg_1010:7073), accumulates stat gem pickups (+1/+3/+5 from 0x8F/0x90/0x91).
- **bonus_stat** (+0xAC): Computed by game_state_update (seg_1010:7065):
  `bonus_stat = lg_rockpick * 3 + rockpick + powerdrill * 5`
  where rockpick/lg_rockpick/powerdrill are shop purchase counts at offsets +0xD2/+0xD4/+0xD6.

### Shop Stat Upgrades
| Shop Item | Offset | Price | Contribution to bonus_stat |
|-----------|--------|-------|---------------------------|
| Rockpick  | +0xD2  | $400  | +1 per purchase           |
| Lg rockpick | +0xD4 | $1100 | +3 per purchase          |
| Powerdrill | +0xD6 | $1600 | +5 per purchase           |

The HUD displays `bonus_stat + digging_power` as a combined dig stat (seg_1010:3458).

Wall HP thresholds determine visual degradation stages.

## Scoring & Economy

### Treasure Values
See tile types table above. Range: 10 cash (0x95) to 1000 cash ('s').

### Mystery Box ('y' / 0x79) Drop Tiers
| Tier | Probability | Weapon Count | Quantity Range |
|------|------------|--------------|----------------|
| 1 | 1/6 | 5 types | 1-3 |
| 2 | 1/6 | 8 types | 1-6 |
| 3 | 4/6 | 13 types | 3-13 |

### Kill Reward
Killing another player increments the killer's kill counter (player struct +0x20/+0x22). There is no direct cash transfer to the killer — a dead player's round earnings go into the round-end pool shared by all survivors (see below). An earlier revision of this page claimed the killer receives the victim's points; that was a misread.

### Super Drill ("Money Bomb", 0xB0)
Despite the decompiled nickname, this grants no cash. It **loans +300 digging power**: a counter starts at 10 and ticks down on the 18-frame tick; when it reaches 1, the 300 digging power is deducted back. Using it consumes no ammo — the original's code path never reaches the inventory decrement.

### Multiplayer Round-End Scoring (`FUN_1000_a17c`)
1. Dead players forfeit this round's earnings into a pool (their banked wallet is untouched)
2. If **exactly one** player survives, the pool also gets `Trunc(value of remaining on-map treasure / 2.5)`
3. Every survivor receives `pool / survivors` plus their own round earnings into the wallet
4. Survivors get a round win counted — but only when at least one player died this round
5. **Welfare floor**: any wallet below 100 gets **+150 added** (not set to 150)

Between rounds, banked savings earn **7% interest**: `wallet := Round(wallet * 1.07)` (`FUN_1010_ceb3`).

### Win Determination
Configurable: by **most cash** or by **most individual round wins**.

## Player Data Structure

See [Player Struct](player-struct.md) for the authoritative field layout
(266-byte struct, stride 0x10A, base 0x1BEA), including the money and
digging-power fields.

## Game Timing

### Update Frequencies

| System | Frequency | Condition |
|--------|-----------|-----------|
| Player movement | Every frame | Always |
| Double-speed movement | Every frame | If player has speed bonus |
| Input, weapons & death check | Every 2 frames | `frame_counter % 2 == 0` |
| Monster contact damage | Every frame | Same-tile check, no modulo gate |
| Monster activation + round-end checks | Every 5 frames | `frame_counter % 5 == 0` |
| Super-drill loan counter | Every 18 frames | `frame_counter % 18 == 0` |
| Treasures-gone check | Every 20 frames | `frame_counter % 20 == 0` |
| Monster pathfinding | Every 26 frames | `frame_counter % 26 == 0` |
| Monster random direction | Every 33 or 121 frames | `frame_counter % 33 == 0` or `% 121 == 0` |
| Overlay/fuse timers | Every frame | All overlays > 1 decremented |
| Time limit countdown | 18.2065 Hz PIT ticks | Wall-clock INT8 ISR, not frame-locked |

### Frame Delay
- `g_frame_delay` controls `delay_wait()` per frame
- Display formula: `speed = (33 - frame_delay) * 3 + 1`
- Higher delay = slower game

## Win/Lose Conditions

### Single-Player
- **Win**: reach exit tile 'k' (0x6B). Only one 'k' tile exists per level (extras randomly cleared at level load).
- **Lose**: health ≤ 0 → lose 1 life, retry level. 0 lives → game over.
- **15 levels** total. Complete all = game complete screen + hall of fame.

### Multiplayer Round End
Any of:
1. **< 2 players alive** → inactivity counter ramps (+3 per 5-frame check), round ends when it exceeds 100
2. **All treasures collected** → inactivity ramps fast (+20 per 20-frame check)
3. **Time runs out** → round ends (multiplayer only; in single-player the timer resets to full instead)
4. **ESC** → ends the current round (multiplayer only; does nothing in single-player)
5. **F10** → aborts the whole match, both modes

The decompiler's variable names had ESC and the abort key swapped ("g_key_mp_sync" is actually ESC); there is no separate "sync key". Other in-round keys: P pauses (any key resumes), F5 toggles music.

### Collision Detection
- **Proximity**: absolute distance < 20 pixels in both X and Y
- **Line-of-sight**: checks all tiles between monster and player; if all passable → monster kills
- **Directional fan**: checks fan-shaped area up to 7 tiles with increasing width
