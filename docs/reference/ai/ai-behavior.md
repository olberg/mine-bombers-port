# AI & Monster Behavior

Reverse-engineered from `seg_1000_game_logic.c` and `seg_1010_graphics.c`.

## Architecture Overview

The game has exactly one AI system: **monster AI** — autonomous entity pathfinding and combat, used for both map-spawned monsters and player-spawned creatures. It runs inside `monster_player_collision()` (seg_1000:5803).

Two corrections to earlier AI-assisted analysis of the decompiled sources (an older model misread these, and the errors propagated through earlier revisions of this page):

1. The named player profiles (Lottery, Skitso, Rambo, Invis, Pyroman, Mutation), previously described here as "bot AI personalities", are **cheat codes** — special player names that grant stat or visual bonuses at round start. No AI involved.
2. `apply_bot_ai()` (seg_1010:6987) is not an AI routine despite its decompiled name — it's the pre-round **shop screen** (it renders up to two player purchase panels per page; the first parameter selects solo vs paired layout).

## Cheat-Code Player Names

Applied by `FUN_1000_318d()` (seg_1000:2042) when a player's name (case-insensitive) matches one of six built-in cheat codes. They apply to **any** player — human or not — whose name matches:

| Name | Effect |
|------|--------|
| **Lottery** | Sets cash to 50,000. |
| **Skitso** | Sets all 27 weapon slots to 50 each, plus one of each dig tool (rock pick, large rock pick, power drill). |
| **Rambo** | Sets health and max health to 32,000. |
| **Invis** | Copies the floor sprite into all 16 player sprite slots — the player is invisible. |
| **Pyroman** | Sets directional rockets (offset 0xC0) to 1000. |
| **Mutation** | Copies a monster sprite set ("Mon 3") over the player's sprites — the player looks like a monster. |

These are one-shot stat/visual modifications applied at round init, not behavioral AI. The in-game info screens document them on a hidden page (`CODES.SPY`, shown when Tab is pressed on the fourth info image).

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
| +0xA4 | 2 | Current direction (1=right, 2=left, 3=up, 4=down) |
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

Spawned from tile map characters 'G'-'V' (0x47-0x56). The speed divisor throttles movement: the entity moves on every frame where `frame % divisor != 0`, so a **higher divisor means a faster monster** (divisor 100 → moves 99 of every 100 frames; divisor 2 → moves every other frame).

| Tiles | Template | Speed Divisor | Health | Attack | Notes |
|-------|----------|---------------|--------|--------|-------|
| G-J (0x47-0x4A) | KarvaMies | 6 | 29 | 2 | Moves 5 of 6 frames |
| K-N (0x4B-0x4E) | Creature | 3 | 29 | 3 | Moves 2 of 3 frames |
| O-R (0x4F-0x52) | Creature | 2 | 10 | 1 | Slowest, weakest |
| S-V (0x53-0x56) | Alien | 100 | 66 | 5 | Fastest, strongest |

Initial facing direction is encoded in the letter within each group of 4: first=right, second=left, third=up, fourth=down (e.g., G=right, H=left, I=up, J=down).

## Monster AI Decision Loop

Runs in `monster_player_collision()` (seg_1000:5803), called every frame. For each active, alive monster:

### Movement (every frame, throttled by speed divisor)
```
if (frame_counter % speed_divisor != 0): move_player(monster)
```

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
- An enemy player shares its row or column but is not on the exact same tile

Arrow tile matches movement direction: right→0xA5, left→0xA6, down→0xA7, up→0xA8.

## Monster Activation

Monsters start **dormant** (+0x103 == 0). Activation checks run every 5 frames, with three detection methods:

1. **Proximity**: player within 20 pixels (absolute) in both X and Y
2. **Line-of-sight**: player on same row/column with no walls between them
3. **Directional fan**: fan-shaped area ahead of the monster, up to 7 tiles wide

A roar sound plays on activation. Once active, a monster never goes dormant again.

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

- **Same-tile collision**: checked every frame with exact tile matching (not pixel proximity); monster deals damage = `monster.health` to player's health
- **Owner immunity**: no damage if `monster.owner == player_number`

## Key Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| Pathfind item radius | 5 tiles | Max search for collectibles |
| Pathfind player radius | 10 tiles | Max search for players |
| Pathfind bomb radius | 63 tiles | Max search for hazards (full map) |
| Monster AI tick | 26 frames | Decision frequency |
| Random redirect | 33 / 121 frames | Direction change frequency |
| Bomb placement threshold | 5 clear tiles | Min clear line for bomb |
| Activation check | every 5 frames | Dormant monster detection tick |
| Proximity activation | 20 pixels | Dormant → active distance |
| Random move chance | 3% | Prevents deterministic loops |
| Direction encoding | 1=right, 2=left, 3=up, 4=down | Entity movement |
