#include "game/weapons.h"
#include <stddef.h>

/*
 * Weapon table built from decompiled GAME_MECHANICS.md data.
 * Order matches the weapon cycling order in the original game.
 * Prices extracted from MB.EXE via FUN_1010_a2de (6-byte Real constants
 * loaded at runtime into DAT_1038_25b2..25e6).
 *
 * Ref: seg_1010_graphics.c:6320-6414 (FUN_1010_af5c, price lookup)
 */
const WeaponDef g_weapon_defs[WEAPON_COUNT] = {
    /* tile   offset  fuse  coll  price  name */
    { 0x57,   0xB0,   100,   20,    1,   "Small bomb" },
    { 0x58,   0xB2,   100,   20,    3,   "Medium bomb" },
    { 0x59,   0xB4,    80,   20,   10,   "Large bomb" },
    { 0x5A,   0xC0,    -1,   20,  500,   "Rocket" },
    { 0x7F,   0xC2,   260,   20,   80,   "Mega bomb" },
    { 0x80,   0xC8,    -1,   20,  145,   "Cluster bomb" },
    { 0x81,   0xCA,    -1,   20,   15,   "Super bomb" },
    { 0x65,   0xBE,    -1,   20,   25,   "Explosive" },
    { 0x8A,   0xC6,    -1,   20,   35,   "Napalm" },
    { 0x9D,   0xB6,   280,   20,  650,   "Mine" },
    { 0x72,   0xDE,    -1,   20,   80,   "Rocket launcher" },
    { 0xA1,   0xCC,    90,   20,   80,   "Timer bomb" },
    { 0xB0,   0xE4,    -1,    0,  575,   "Money bomb" },
    { 0xA2,   0xCE,    -1,   20,  120,   "Remote bomb" },
    { 0xA4,   0xC4,    -1,   20,   90,   "Power bomb" },
    { 0xA5,   0xBC,     1,    0,  300,   "Arrow" },
    { 0xA9,   0xD0,     1,   20,   50,   "Teleporter bomb" },
    { 0x9C,   0xD8,    -1,   20,   70,   "Warp bomb" },
    { 0x6E,   0xDA,    -1,   20,  400,   "Creature spawner" },
    { 0x6F,   0xDC,    -2,  400,   50,   "Proximity mine" },
    { 0xAB,   0xE2,    -2,   -1,   95,   "Random bomb" },
};

const WeaponDef *weapon_get_def(uint8_t tile_id)
{
    for (int i = 0; i < WEAPON_COUNT; i++) {
        if (g_weapon_defs[i].tile_id == tile_id) return &g_weapon_defs[i];
    }
    return NULL;
}

int weapon_inv_index(uint8_t tile_id)
{
    for (int i = 0; i < WEAPON_COUNT; i++) {
        if (g_weapon_defs[i].tile_id == tile_id) return i;
    }
    return -1;
}
