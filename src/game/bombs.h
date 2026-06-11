#ifndef BOMBS_H
#define BOMBS_H

#include "game/player.h"
#include "game/map.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Bomb visual stage transitions (from decompiled process_player_input).
 * Each bomb type progresses through visual stages as its fuse counts down.
 */

/* Small bomb stages: W(100) -> w(<60) -> x(<30) -> detonate(0) */
#define BOMB_SMALL_1     0x57  /* 'W' */
#define BOMB_SMALL_2     0x77  /* 'w' */
#define BOMB_SMALL_3     0x78  /* 'x' */
#define BOMB_SMALL_DET   0x7D  /* detonated sprite */

/* Medium bomb stages: X(100) -> 0x8B(<60) -> 0x8C(<30) -> detonate(0) */
#define BOMB_MEDIUM_1    0x58  /* 'X' */
#define BOMB_MEDIUM_2    0x8B
#define BOMB_MEDIUM_3    0x8C
#define BOMB_MEDIUM_DET  0x7E  /* detonated sprite */

/* Large bomb stages: Y(80) -> 0x8D(<40) -> 0x8E(<20) -> detonate(0) */
#define BOMB_LARGE_1     0x59  /* 'Y' */
#define BOMB_LARGE_2     0x8D
#define BOMB_LARGE_3     0x8E
#define BOMB_LARGE_DET   0x7C  /* detonated sprite */

/* Mega bomb: 0x7F(260) -> 0xA3 -> detonate */
#define BOMB_MEGA_1      0x7F
#define BOMB_MEGA_2      0xA3

/* Mine stages: 0x9D(280) -> 0x9E -> 0x9F -> 0x9D (cycling) */
#define BOMB_MINE_1      0x9D
#define BOMB_MINE_2      0x9E
#define BOMB_MINE_3      0x9F

/* Timer bomb: 0xA1(90) -> 0x9E */
#define BOMB_TIMER_1     0xA1

/* Directional arrow tiles. True mapping from placement
 * (seg_1000:2744-2773: dir 1→0xA5, 2→0xA6, 3→0xA8, 4→0xA7) and
 * propagation (FUN_1010_165b, seg_1010:1036-1102: 0xA5 = row+1 = screen
 * RIGHT, 0xA6 = row-1 = LEFT, 0xA7 = col+1 = DOWN, 0xA8 = col-1 = UP).
 * Note DOWN/UP are NOT in value order. */
#define ARROW_RIGHT      0xA5
#define ARROW_LEFT       0xA6
#define ARROW_DOWN       0xA7
#define ARROW_UP         0xA8

/* Random bomb detonated sprite */
#define BOMB_RANDOM_DET  0xAA

/* Explosion tiles (fire/scorch marks, passable) */
#define TILE_EXPLOSION   0x84
#define TILE_EXPLOSION2  0x85

/* Rocket launcher fire beam tile (FUN_1010_4c31, seg_1010:2502) */
#define TILE_ROCKET_FIRE 0x61  /* 'a' — launcher beam fire */

/* Directional rocket expansion marker (FUN_1010_45cd, seg_1010:2289) */
#define TILE_ROCKET_EXPAND 0x7A  /* 'z' — temporary during expansion, → 0x84 */

/*
 * Per-player bomb signature tiles (decompiled seg_1010:5562-5567).
 * Two weapon types at offsets 0xB8 and 0xBA use player-specific tiles
 * so remote detonation can identify bomb ownership:
 *   Player 1: 'c' (0x63), 'd' (0x64)
 *   Player 2: 'g' (0x67), 'h' (0x68)
 *   Player 3: 0x82, 0x83
 *   Player 4: 'i' (0x69), 'j' (0x6A)
 * Overlay = 0 (no auto-detonation; triggered by remote key).
 * Collision = 20 (non-passable). Not targeted by AI pathfinding.
 */
#define BOMB_SIG_P1_A    0x63  /* 'c' */
#define BOMB_SIG_P1_B    0x64  /* 'd' */
#define BOMB_SIG_P2_A    0x67  /* 'g' */
#define BOMB_SIG_P2_B    0x68  /* 'h' */
#define BOMB_SIG_P3_A    0x82
#define BOMB_SIG_P3_B    0x83
#define BOMB_SIG_P4_A    0x69  /* 'i' */
#define BOMB_SIG_P4_B    0x6A  /* 'j' */

/* Urethane flood-fill tiles (decompiled seg_1010:1104-1211).
 * "Urethane" is the expanding blocking substance produced by timer bomb (0xA1)
 * and super bomb (0x81). Manual: "First trap your opponent in urethane."
 * Players colloquially call it "slime". NOT to be confused with the creature
 * spawner weapon (0x6E/'n') which spawns enemy entities. */
#define TILE_URETHANE_ORIGIN  0x88  /* flood-fill origin marker during expansion */
#define TILE_URETHANE_EXPAND  0x89  /* temporary expansion marker during flood-fill */
#define TILE_URETHANE_FINAL   0xA0  /* final urethane tile for timer bomb (0xA1) */
#define TILE_URETHANE_PERM    0x9B  /* final urethane tile for super bomb (0x81) — permanent */
#define URETHANE_FINAL_OVERLAY 0xFA /* 250 frames: timer bomb urethane expiry countdown */
#define URETHANE_MAX_ITER_A1   50   /* max flood-fill iterations for 0xA1 timer bomb */
#define URETHANE_MAX_ITER_81   45   /* max flood-fill iterations for 0x81 super bomb */

/* Remote bomb expansion (decompiled seg_1010:1581-1701).
 * Flood-fill that spreads THROUGH walls ('7'-'9','A'-'F'), chain-detonates
 * non-wall tiles. Uses same 0x88/0x89 markers as urethane. Final pass applies
 * strong damage (FUN_1010_1326 param_2=1, param_3=10) at all expanded positions. */
#define REMOTE_BOMB_MAX_ITER   75   /* max expansion iterations (0x4B) */

/* Mega bomb expansion (decompiled seg_1010:1704-1847).
 * Uses 'z' (0x7A) marker tiles with random probability gates,
 * expanding omnidirectionally up to 75 tiles, then applies strong damage. */
#define MEGA_BOMB_MAX_ITER     75   /* max expansion iterations (0x4B) */

/* Default collision value for placed bombs */
#define BOMB_COLLISION_DEFAULT  20
#define BOMB_COLLISION_PROX    400
/* Collision a pushed bomb gets at its destination (seg_1000:3697, 0x18) */
#define BOMB_COLLISION_PUSHED   24

/* Treasure drop chance: 11 out of 1000 = 1.1% */
#define TREASURE_DROP_CHANCE   11
#define TREASURE_DROP_RANGE  1000

/* Money bomb ("SUPERpora" / super drill, HISTORIA v3.1): a +300 DIGGING
 * POWER loan (+0xA8), repaid −300 when the counter ticks down to 1 —
 * NOT cash. */
#define MONEY_BOMB_DIG_LOAN   300
#define MONEY_BOMB_COOLDOWN    10

/*
 * Place a bomb at the player's current tile position.
 * Checks inventory, tile availability, sets tile/collision/overlay.
 * Returns true if bomb was placed (or money bomb used).
 */
bool bomb_place(Player *p, TileMap *map);

/*
 * Update all bomb fuses and visual stages on the map.
 * Called every frame. Decrements overlays, triggers detonations,
 * and transitions bomb visual stages.
 */
void bombs_update(TileMap *map);

/*
 * Check for treasure drop at a detonating bomb position.
 * Called when overlay reaches 1 (just before detonation).
 * Returns true if treasure was spawned (bomb replaced with treasure tile).
 * If false, normal detonation should proceed.
 *
 * Only late-stage bomb tiles can drop treasure:
 *   'x' (0x78) -> 0x7D, 0x8B -> 0x7E, 0x8C -> 0xAA, 0x8D -> 0x7C
 */
bool bomb_check_treasure_drop(TileMap *map, int col, int row);

/*
 * Detonate a bomb at the given tile position.
 * Applies explosion damage to surrounding collision map,
 * handles wall degradation, and marks tiles for redraw.
 */
void bomb_detonate(TileMap *map, int col, int row);

/*
 * Remote detonation: scan entire map for tiles matching the player's
 * bomb signature (bomb_sig[0] or bomb_sig[1]), and set their overlay
 * to 1 (triggering detonation on next bombs_update call).
 */
void bombs_remote_detonate(TileMap *map, const Player *p);

/*
 * Player bomb push (FUN_1000_5073 non-wall branch, seg_1000:3656-3706).
 * Attempt to slide the tile at (col,row) one tile in the player's facing
 * direction (1=RIGHT, 2=LEFT, 3=UP, 4=DOWN). The caller is responsible for
 * the eligibility gate: collision already drained below 2 by digging.
 * Succeeds only if the destination is open floor ('0'/'f'/0xAF) with no
 * live player or entity on it. The fuse overlay travels with the tile and
 * collision at the destination resets to BOMB_COLLISION_PUSHED.
 * Returns true if the tile moved.
 */
bool bomb_try_push(TileMap *map, int col, int row, uint8_t dir);

/*
 * Rocket launcher (0x72/'r'): fire a beam of up to 6 tiles in facing direction.
 * Each tile in the beam becomes fire tile 'a' (0x61) with overlay=3.
 * If a bomb is hit, it chain-detonates. Stops at walls/bounds.
 * Decompiled ref: FUN_1010_4dff (seg_1010:2542-2578),
 *                 FUN_1010_4c31 (seg_1010:2475-2538).
 */
void rocket_launcher_fire(TileMap *map, int col, int row, uint8_t direction);

/*
 * Directional rocket (0x5A/'Z'): expanding cross pattern.
 * Places 'z' (0x7A) tiles that expand outward from the origin, biased
 * toward the player's facing direction. Expands up to 29 tiles then
 * converts all 'z' to fire (0x84) with overlay=3.
 * Decompiled ref: FUN_1010_45cd (seg_1010:2249-2471).
 */
void directional_rocket_fire(TileMap *map, int col, int row, uint8_t direction);

/*
 * Set the entity list pointer for explosion damage checks.
 * Must be called before bombs_update() each frame so that
 * bombs_check_player_damage can iterate entities.
 */
struct Entity;  /* forward declaration */
void bombs_set_entity_list(struct Entity *head);

/*
 * Check if any player or entity is on the given tile and apply explosion damage.
 * Faithfully implements FUN_1010_98dd (seg_1010:5337-5473).
 *
 * Uses g_players/g_num_active_players globals and the entity list set via
 * bombs_set_entity_list().
 *
 * damage: HP to subtract from player health. 0 means "presence check only"
 *   (used by arrow bombs to detect blocking; kills result in corpse 'f' tile).
 *   Non-zero damage kills result in explosion death tile (0x85).
 *
 * Per-bomb-type damage values (from FUN_1010_1326 calls):
 *   Small bomb:         0x3C (60)
 *   Medium/sig-A:       0x54 (84)
 *   Large/sig-B:        100
 *   Type 0x80:          100 or 200
 *   Mines (strong):     0xFF (255)
 *   Mega (strong):      0xDC (220)
 *   Timer/Super:        0xDC (220)
 *   Power bomb scatter: 0x54 (84)
 *
 * Returns true if any live player/entity was found on the tile.
 */
bool bombs_check_player_damage(TileMap *map, int col, int row, uint8_t damage);

#endif
