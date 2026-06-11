#ifndef WEAPONS_H
#define WEAPONS_H

#include <stdint.h>

/*
 * ===== WEAPON TILE ID MAP =====
 *
 * Every weapon-related tile in the game, organized by category.
 * Decompiled refs: FUN_1000_3974 (weapon cycling, seg_1000:2398-2488),
 *                  FUN_1010_165b (detonation, seg_1010:829-2265),
 *                  process_weapons (placement, seg_1000:2582-2930).
 *
 * --- Base weapon IDs (placed on map, in cycling order) ---
 *
 *   Tile  Hex   Inv.Ofs  Fuse  Name
 *   'W'   0x57  0xB0     100   Small bomb
 *   'X'   0x58  0xB2     100   Medium bomb
 *   'Y'   0x59  0xB4      80   Large bomb
 *   'Z'   0x5A  0xC0      --   Directional rocket
 *   ---   0x7F  0xC2     260   Mega bomb
 *   ---   0x80  0xC8      --   Cluster bomb
 *   ---   0x81  0xCA      --   Super bomb (urethane)
 *   [per-player bomb_sig[0] tile inserted here in cycle]
 *   [per-player bomb_sig[1] tile inserted here in cycle]
 *   'e'   0x65  0xBE      --   Explosive
 *   ---   0x8A  0xC6      --   Napalm (fire bomb)
 *   ---   0x9D  0xB6     280   Mine
 *   'r'   0x72  0xDE      --   Rocket launcher
 *   ---   0xA1  0xCC      90   Timer bomb (urethane)
 *   ---   0xB0  0xE4      --   Money bomb (+300 cash, no tile placed)
 *   ---   0xA2  0xCE      --   Remote bomb
 *   ---   0xA4  0xC4      --   Power bomb
 *   ---   0xA5  0xBC       1   Arrow (directional, immediate detonation)
 *   ---   0xA9  0xD0       1   Teleporter bomb (immediate)
 *   ---   0x9C  0xD8      --   Warp bomb
 *   'n'   0x6E  0xDA      --   Creature spawner
 *   'o'   0x6F  0xDC    rand   Proximity mine (fuse 0-80)
 *   ---   0xAB  0xE2    rand   Random bomb (fuse 80-160)
 *
 * --- Visual fuse-stage tiles (NOT base weapons; countdown appearances) ---
 *
 *   Tile  Hex   Parent    Stage
 *   'w'   0x77  0x57(W)   Small bomb fuse < 60
 *   'x'   0x78  0x57(W)   Small bomb fuse < 30
 *   ---   0x7D  0x57(W)   Small bomb detonated sprite
 *   ---   0x8B  0x58(X)   Medium bomb fuse < 60
 *   ---   0x8C  0x58(X)   Medium bomb fuse < 30
 *   ---   0x7E  0x58(X)   Medium bomb detonated sprite
 *   ---   0x8D  0x59(Y)   Large bomb fuse < 40
 *   ---   0x8E  0x59(Y)   Large bomb fuse < 20
 *   ---   0x7C  0x59(Y)   Large bomb detonated sprite
 *   ---   0xA3  0x7F      Mega bomb stage 2
 *   ---   0x9E  0x9D      Mine stage 2 (cycles: 0x9D->0x9E->0x9F->0x9D)
 *   ---   0x9F  0x9D      Mine stage 3
 *   ---   0xAA  0xAB      Random bomb detonated sprite
 *
 * --- Directional arrow variants (base = 0xA5, direction-encoded) ---
 *
 *   Tile  Hex   Direction (from FUN_1010_165b propagation axes)
 *   ---   0xA5  Right
 *   ---   0xA6  Left
 *   ---   0xA7  Down
 *   ---   0xA8  Up
 *
 * --- Per-player bomb signature tiles (remote detonation ownership) ---
 *
 *   Player  Sig0(0xB8)  Sig1(0xBA)
 *   P1      'c' 0x63    'd' 0x64
 *   P2      'g' 0x67    'h' 0x68
 *   P3      --- 0x82    --- 0x83
 *   P4      'i' 0x69    'j' 0x6A
 *
 *   Cycling: 0x81 -> bomb_sig[0] -> bomb_sig[1] -> 0x65
 *   The sig[0] transition checks for any of c/g/0x82/i (all Sig0 tiles).
 *   The sig[1] transition checks for any of d/h/0x83/j (all Sig1 tiles).
 *   Overlay = 0 (no auto-detonation). Collision = 20.
 *
 * --- Explosion/scorch tiles (result of detonation, passable) ---
 *
 *   Tile  Hex   Notes
 *   ---   0x84  Explosion fire (overlay = 3, fades to floor)
 *   ---   0x85  Explosion variant 2
 */

typedef struct {
    uint8_t     tile_id;        /* placement tile (e.g., 'W', 'X', 'Y') */
    uint8_t     inv_offset;     /* original struct offset (0xB0, 0xB2, ...) */
    int16_t     fuse_frames;    /* -1 = special handling, -2 = random */
    int16_t     collision_hp;   /* placed collision value */
    int16_t     price;          /* shop price */
    const char *name;           /* display name */
} WeaponDef;

#define WEAPON_COUNT 21

extern const WeaponDef g_weapon_defs[WEAPON_COUNT];

/* Look up weapon definition by tile ID. Returns NULL if not found. */
const WeaponDef *weapon_get_def(uint8_t tile_id);

/* Map weapon tile ID to inventory index (0..WEAPON_COUNT-1). Returns -1 if invalid. */
int weapon_inv_index(uint8_t tile_id);

#endif
