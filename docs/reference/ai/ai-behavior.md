# AI & Monster Behavior

Reverse-engineered from `seg_1000_game_logic.c` and `seg_1010_graphics.c`.

## Architecture Overview

The game has two separate AI systems:
1. **Bot AI personalities** — named player profiles that modify stats (not true AI decision-making)
2. **Monster AI** — autonomous entity pathfinding and combat, used for both single-player monsters and player-spawned creatures

`apply_bot_ai()` (seg_1010:6987) is **not** the AI brain — it's a pre-round betting/spectator screen. The actual monster/entity AI runs inside `monster_player_collision()` (seg_1000:5803).

## Bot AI Personalities

Activated by `FUN_1000_318d()` (seg_1000:2042) when a player's name matches a known AI name from `players.dat`:

| Name | Effect |
|------|--------|
| **Lottery** | Sets score to 50,000. High-risk gambler. |
| **Skitso** | Sets all 27 weapon slots to 50 each. Random arsenal. |
| **Rambo** | Sets health to 32,000. Extremely tanky. |
| **Invis** | Copies a transparent sprite into all 16 sprite slots. Invisible player. |
| **Pyroman** | Sets bomb capacity (offset 0xC0) to 1000. Unlimited bombs. |
| (6th name) | Custom visual skin (copies 64 bytes into sprite arrays). |

These are stat modifications applied at round start, not behavioral AI. The actual movement/combat decisions come from the monster AI system.

## Monster Entity Structure

Each monster is a **265-byte (0x109) struct** in a linked list (head at `DAT_1038_2544/2546`):

| Offset | Size | Field |
|--------|------|-------|
| +0x00 | 0x1A | Name string (Pascal-style) |
| +0x1B | 2 | Health |
| +0x1D | 2 | Max health |
| +0x21 | 1 | Dead flag |
| +0x22 | 64 | Sprite table 1 |
| +0x62 | 64 | Sprite table 2 |
| +0xA2 | 2 | Animation state |
| +0xA4 | 2 | Current direction (1=left, 2=right, 3=up, 4=down) |
| +0xA6 | 2 | Previous direction |
| +0xA8 | 2 | Base attack power |
| +0xAC | 2 | Bonus attack power |
| +0xEE | 2 | X position (pixels) |
| +0xF0 | 2 | Y position (pixels, offset by 30) |
| +0xFD | 2 | Owner player number (prevents friendly fire) |
| +0x103 | 1 | Active flag (0=dormant, 1=hostile) |
| +0x104 | 4 | Next pointer (linked list) |
| +0x108 | 1 | Speed divisor |

## Monster Types

Spawned from tile map characters 'G'-'V' (0x47-0x56):

| Tiles | Template | Speed Divisor | Notes |
|-------|----------|---------------|-------|
| G-J (0x47-0x4A) | KarvaMies | 6 | Fast, type 2 |
| K-N (0x4B-0x4E) | Creature 2 | 3 | Faster, type 3 |
| O-R (0x4F-0x52) | Creature 3 | 2 | Very fast, aggro range 10 |
| S-V (0x53-0x56) | Alien | 100 | Very slow but powerful, aggro range 66 |

Initial facing direction encoded in the letter: first=left, second=right, third=up, fourth=down (e.g., G=left, H=right, I=up, J=down).

## Monster AI Decision Loop

Runs in `monster_player_collision()` (seg_1000:5803), called every frame. For each active, alive monster:

### Movement (every frame, throttled by speed)
```
if (frame_counter % speed_divisor != 0): move_player(monster)
```
Speed 6 → moves 5 of every 6 frames. Speed 100 → moves 99 of every 100 frames.

### AI Decisions (every 26 frames)
```
frame_counter % 26 == 0
```

1. Search for collectible items (radius 5) → move toward them
2. If no items, search for players (radius 10) — includes owner
3. If found player is an **enemy** (not owner) → approach directly + try to place bomb (**no hazard check**)
4. If found player is **owner** or no player found → search for hazards (radius 63, only if treasures remain on map) → flee if found
5. If no hazard found → try to place bomb

**Key behavior:** Entities only flee from hazards when they have no enemy target. When an enemy is in range, entities attack regardless of nearby bombs. The hazard search is additionally gated by `FUN_1000_6ddc() > 0` (treasure count) — no treasures means no hazard avoidance.

### Random Direction Changes
- Every 33 frames: if blocked in current direction, pick random direction 1-4
- Every 121 frames: unconditionally pick random direction

### Bomb Placement
Monster places a directional arrow bomb when:
- It can "see" 5+ clear tiles in its current direction
- No other monsters are in the blast line
- Its own owner is not in the blast line

Arrow direction matches movement: left→0xA5, right→0xA6, up→0xA8, down→0xA7.

## Monster Activation

Monsters start **dormant** (+0x103 == 0). They activate when:

1. **Proximity**: player within 20 pixels (absolute) in both X and Y
2. **Line-of-sight**: player on same row/column with no walls between them

Once active, a monster never goes dormant again.

## Pathfinding System

Three functions using **expanding square spiral search**:

| Function | Purpose | Max Radius | Targets |
|----------|---------|------------|---------|
| `pathfind_target` (seg_1000:5021) | Find items | 5 tiles | Tile values 0x57-0x59, 0x77-0x78, 0x7F-0x81, 0x8A-0x8E, 0x9D-0xA9, 0xAB |
| `pathfind_alt` (seg_1000:5192) | Find players | 10 tiles | Checks player positions via `FUN_1000_8057` |
| `FUN_1000_8e28` (seg_1000:5707) | Find bombs/hazards | 63 tiles | Tile 0x73, 0x92-0x9A, 0x8F-0x91 |

### Movement Functions

| Function | Behavior |
|----------|----------|
| `move_entity_toward_target` (seg_1000:5348) | Move toward target. Picks axis with larger distance. 3% random chance to swap axes (prevents loops). |
| `move_entity_alt` (seg_1000:5404) | Move **away** from target (flee). If both directions blocked, random direction 1-4. |
| `FUN_1000_83a2` (seg_1000:5272) | Obstacle check. Returns 1 if direction blocked. Passable: '0', 'f', 0xAF. |

## Collision & Damage

- **Same-tile collision**: monster deals damage = `monster.health` to player's health
- **Owner immunity**: no damage if `monster.owner == player_number`
- **Directional fan check**: for each direction, checks fan-shaped area up to 7 tiles wide

## Player-Count Behavior

| Players | Bot Slots | Mode |
|---------|-----------|------|
| 1 | Player 2 automated (mode=0) | Betting screen skipped |
| 2 | None automated | Betting screen for player 2 (mode=1) |
| 3 | Player 4 automated (mode=0) | Betting for player 2 |
| 4 | None automated | Betting for player 2 and player 4 |

## Key Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| Pathfind item radius | 5 tiles | Max search for collectibles |
| Pathfind player radius | 10 tiles | Max search for players |
| Pathfind bomb radius | 63 tiles | Max search for hazards (full map) |
| Monster AI tick | 26 frames | Decision frequency |
| Random redirect | 33 / 121 frames | Direction change frequency |
| Bomb placement threshold | 5 clear tiles | Min clear line for bomb |
| Proximity activation | 20 pixels | Dormant → active distance |
| Random move chance | 3% | Prevents deterministic loops |
| Direction encoding | 1=left, 2=right, 3=up, 4=down | Entity movement |
