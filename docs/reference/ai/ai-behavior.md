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

A monster is a player record: a **265-byte (0x109) struct** with the layout of the [player struct](../gameplay/player-struct.md), kept in a linked list (head at `DAT_1038_2544/2546`) and moved by the same `move_player` routine as a player (seg_1000:3860, called per monster at seg_1000:5901-5903). Some fields mean something else for a monster:

| Offset | Size | Field |
|--------|------|-------|
| +0x00 | 0x1A | Name string (Pascal-style) |
| +0x1B | 2 | Contact damage per frame (a player's health) |
| +0x1D | 2 | Hit points (a player's max health). Explosion damage is subtracted from this; the monster dies below 1 |
| +0x21 | 1 | Dead flag |
| +0x22 | 64 | Sprite table 1 |
| +0x62 | 64 | Sprite table 2 |
| +0xA2 | 2 | Animation counter (starts at 1 in every template) |
| +0xA4 | 2 | Current direction (1=right, 2=left, 3=up, 4=down) |
| +0xA6 | 2 | Facing (the last direction moved in) |
| +0xA8 | 2 | Dig power, as for a player. 0 in the four map-monster templates |
| +0xAC | 2 | Dig strength (see the type table) |
| +0xEE | 2 | X position (pixels, tile centre) |
| +0xF0 | 2 | Y position (pixels, tile centre, offset by 30) |
| +0xFD | 2 | Owner player number 1-4. Set for a Robot only |
| +0xFF | 4 | Match-stats pointer: null for a monster |
| +0x103 | 1 | Awake flag (0=dormant, 1=hostile). Always 0 for a player |
| +0x104 | 4 | Next pointer (linked list) |
| +0x108 | 1 | Speed divisor |

## Monster Types

Spawned from tile map characters 'G'-'V' (0x47-0x56). The speed divisor throttles movement: the entity moves on every frame where `frame % divisor != 0`, so a **higher divisor means a faster monster** (divisor 100 → moves 99 of every 100 frames; divisor 2 → moves every other frame).

| Tiles | Template | Speed Divisor | Hit points | Contact damage | Dig strength | Notes |
|-------|----------|---------------|------------|----------------|--------------|-------|
| G-J (0x47-0x4A) | KarvaMies | 6 | 29 | 2 | 5 | Moves 5 of 6 frames |
| K-N (0x4B-0x4E) | HarmaaPeikko | 3 | 29 | 3 | 12 | Moves 2 of 3 frames |
| O-R (0x4F-0x52) | LimaPeikko | 2 | 10 | 1 | 12 | Slowest, weakest |
| S-V (0x53-0x56) | Alien | 100 | 66 | 5 | 52 | Fastest, strongest |

The template names are the strings at seg_1010:9D76, 9D80, 9D8D and 9D98 (templates filled at seg_1010:5578-5621). A fifth template, "Robot" (9D9E), belongs to the Robot weapon; see [The Robot](#the-robot).

Initial facing direction is encoded in the letter within each group of 4: first=right, second=left, third=up, fourth=down (e.g., G=right, H=left, I=up, J=down).

Both spawners append to the end of the list (seg_1000:4585-4599, 2506-2523), so monsters act in spawn order.

## Monster AI Decision Loop

Runs in `monster_player_collision()` (seg_1000:5803), called every frame (seg_1000:7261). Monsters are handled one at a time in list order. For each awake, alive monster, in this order (seg_1000:5836-5964):

1. Contact damage to the players on its tile (see [Collision & Damage](#collision-damage))
2. Movement, throttled by the speed divisor
3. Facing (+0xA6) takes the direction, unless the direction is 0
4. The AI decision, every 26 frames
5. The random turn, every 33 frames when blocked and every 121 frames regardless

Steps 4 and 5 are tested separately, so both can run on the same frame. The treasure count the decision uses is taken once per frame, before any monster acts (seg_1000:5830).

### Movement (every frame, throttled by speed divisor)
```
if (frame_counter % speed_divisor != 0): move_player(monster)
```

`move_player` is the routine players use, so a monster follows the same rules. It steps only into '0', 'f' and 0xAF, and at a tile centre it digs at, pushes or takes whatever is ahead:

- It digs with its own dig strength (+0xA8 + +0xAC). Seen in the original at the start of a round on the ANZULABY map: a boulder goes from 24 to 12 (dig strength 12) and a reinforced wall from 8000 to 7948 (an Alien's 52).
- It pushes bombs.
- It takes treasure, crates, medkits, teleporters and switches. Treasure taken by a map monster is credited to no player.
- A medkit sets +0x1B to +0x1D unless +0x103 is set (seg_1000:3646-3648). For a player that is health = max health. A monster still asleep ends up dealing as much contact damage as it has hit points; an awake one uses the medkit up and nothing else changes.
- Its match-stats pointer is null, so it counts no tiles walked or treasures and never reveals tiles in the dark.

With darkness off, every monster, asleep or not, gets one `move_player` call before the round's fade-in (`FUN_1000_758a`, seg_1000:7131-7134). That is how a sleeping monster comes to dig at or take what it faces.

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

These run on their own frame counters, whether or not the frame is also a decision frame. "Blocked" (`FUN_1000_83a2`) means the tile ahead is something the monster will neither walk into, dig through nor take: not '0', 'f' or 0xAF, not sand '2'-'4', and not treasure (0x73, 0x92-0x9A). A monster with direction 0 is not blocked.

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
| `FUN_1000_83a2` (seg_1000:5272) | Blocked test for the 33-frame random turn. Returns 1 unless the tile ahead is '0', 'f', 0xAF, sand '2'-'4' or treasure (0x73, 0x92-0x9A). |

## Collision & Damage

- **Same-tile collision** (seg_1000:5836-5899): checked every frame with exact tile matching (not pixel proximity), both tiles taken from the centre coordinates (+0xEE / 10, (+0xF0 − 30) / 10). The monster subtracts its +0x1B from the player's health.
- **Owner immunity**: a Robot does not hurt its owner. The owner test runs for a monster named "Robot" only (name compare at seg_1000:5842); +0xFD of a map monster is never set.

## The Robot

The Robot weapon ('n', 0x6E) spawns a monster through `FUN_1000_3b40` (seg_1000:2494), from the template at 0x1038:0x2226:

- Contact damage 1, 100 hit points, speed divisor 100 (`DAT_1038_2241/2243/232e`).
- It starts awake (+0x103 = 1, `DAT_1038_2329`).
- The spawner copies the owner's sprite table (seg_1000:2549-2550), so it is drawn with its owner's walk and dig frames.
- It takes the owner's dig strength, dig power, direction and facing (seg_1000:2551-2562, 2574-2575). While the owner's super-drill loan is running, the owner's +0xA8 carries the loan's 300; the Robot gets the value without it.
- What an awake Robot picks up is added to its owner's round earnings and dig power as well as to its own (seg_1000:3463-3496).

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
