#include "unity.h"
#include "game/bombs.h"
#include "game/config.h"
#include "game/player.h"
#include "game/map.h"
#include "game/map_renderer.h"
#include "game/entity.h"
#include <string.h>

/* Global player state defined in player.c, used by bombs.c */
extern Player g_players[MAX_PLAYERS];
extern int    g_num_active_players;

void setUp(void) {
    memset(g_players, 0, sizeof(g_players));
    g_num_active_players = 0;
    bombs_set_entity_list(NULL);
    /* bombs.c scales MP player damage by g_config.bomb_damage_pct;
     * default 100% = raw damage, so existing expectations hold. */
    config_set_defaults();
}

void tearDown(void) {}

/* Helper: place a player at the center of a tile */
static void place_player_at_tile(int idx, int row, int col, int16_t health)
{
    g_players[idx].player_num = idx;
    g_players[idx].x_pos = tile_to_pixel_x(col);
    g_players[idx].y_pos = tile_to_pixel_y(row);
    g_players[idx].health = health;
    g_players[idx].max_health = health;
    g_players[idx].active = 1;
    g_players[idx].dead = 0;
    g_players[idx].deaths = 0;
    g_players[idx].kills = 0;
}

/* Explosion damages player standing on explosion tile */
void test_explosion_damages_player(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 1;
    place_player_at_tile(0, 5, 5, 100);

    /* Apply damage=60 (small bomb) at player's tile */
    bool found = bombs_check_player_damage(&map, 5, 5, 60);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_INT(40, g_players[0].health);  /* 100 - 60 */
    TEST_ASSERT_EQUAL_INT(0, g_players[0].dead);     /* still alive */
}

/* Explosion kills player when damage >= health */
void test_explosion_kills_player(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 1;
    place_player_at_tile(0, 5, 5, 50);

    /* Apply damage=60 which exceeds health=50 */
    bool found = bombs_check_player_damage(&map, 5, 5, 60);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_INT(1, g_players[0].dead);
    TEST_ASSERT_EQUAL_INT(0, g_players[0].active);
    TEST_ASSERT_EQUAL_INT(1, g_players[0].deaths);
    /* Death tile should be TILE_EXPLOSION2 (0x85) for non-zero damage */
    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION2, map.tiles[5][5]);
}

/* Kill credit goes to bomb owner */
void test_kill_credit_to_bomb_owner(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 2;
    place_player_at_tile(0, 5, 5, 100);  /* victim */
    place_player_at_tile(1, 10, 10, 100); /* bomber */

    /* Player 1 placed bomb at (5,5) */
    map.bomb_owner[5][5] = 1;

    /* Lethal damage at victim's tile */
    bombs_check_player_damage(&map, 5, 5, 0xFF);
    TEST_ASSERT_EQUAL_INT(1, g_players[0].dead);      /* victim dead */
    TEST_ASSERT_EQUAL_INT(1, g_players[1].kills);      /* bomber gets credit */
    TEST_ASSERT_EQUAL_INT(0, g_players[0].kills);      /* victim no self-credit */
}

/* No kill credit for self-kill */
void test_no_self_kill_credit(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 1;
    place_player_at_tile(0, 5, 5, 50);

    /* Player 0 placed bomb at own position */
    map.bomb_owner[5][5] = 0;

    bombs_check_player_damage(&map, 5, 5, 0xFF);
    TEST_ASSERT_EQUAL_INT(1, g_players[0].dead);
    TEST_ASSERT_EQUAL_INT(0, g_players[0].kills);  /* no self-credit */
}

/* No damage if player is not on the tile */
void test_no_damage_if_player_elsewhere(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 1;
    place_player_at_tile(0, 5, 5, 100);

    /* Explosion at a different tile */
    bool found = bombs_check_player_damage(&map, 10, 10, 60);
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL_INT(100, g_players[0].health);  /* unchanged */
}

/* Damage=0 places corpse 'f' tile (arrow blocking mode) */
void test_zero_damage_corpse_tile(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 1;
    /* Player at health 0 should die on next damage check */
    place_player_at_tile(0, 5, 5, 0);

    bombs_check_player_damage(&map, 5, 5, 0);
    TEST_ASSERT_EQUAL_INT(1, g_players[0].dead);
    /* damage==0: corpse tile 'f' instead of 0x85 */
    TEST_ASSERT_EQUAL_HEX8('f', map.tiles[5][5]);
}

/* Entity on tile takes explosion damage */
void test_entity_takes_explosion_damage(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 0;

    /* Create an entity at tile (5, 5) */
    Entity e;
    memset(&e, 0, sizeof(e));
    e.type = ENTITY_TYPE_1;
    e.health = 100;
    e.max_health = 100;
    e.x_pos = tile_to_pixel_x(5);
    e.y_pos = tile_to_pixel_y(5);
    e.dead = 0;
    e.next = NULL;

    bombs_set_entity_list(&e);

    bool found = bombs_check_player_damage(&map, 5, 5, 60);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_INT(40, e.health);  /* 100 - 60 */
    TEST_ASSERT_EQUAL_INT(0, e.dead);     /* still alive */
}

/* Entity killed by explosion, death tile placed */
void test_entity_killed_by_explosion(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 0;

    Entity e;
    memset(&e, 0, sizeof(e));
    e.type = ENTITY_TYPE_1;
    e.health = 50;
    e.max_health = 100;
    e.x_pos = tile_to_pixel_x(5);
    e.y_pos = tile_to_pixel_y(5);
    e.dead = 0;
    e.next = NULL;

    bombs_set_entity_list(&e);

    bombs_check_player_damage(&map, 5, 5, 0xFF);
    TEST_ASSERT_EQUAL_INT(1, e.dead);
    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION2, map.tiles[5][5]);
}

/* Full detonation integration — small bomb kills player in radius */
void test_bomb_detonation_kills_nearby_player(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 1;
    /* Place player at (5, 6) — 1 tile right of bomb at (5, 5) */
    place_player_at_tile(0, 5, 6, 50);

    /* Place small bomb at (5, 5) with fuse=1 */
    map.tiles[5][5] = BOMB_SMALL_1;
    map.overlay[5][5] = 1;
    map.collision[5][5] = BOMB_COLLISION_DEFAULT;
    map.bomb_owner[5][5] = 0xFF;

    /* Surrounding tiles should be floor */
    for (int r = 4; r <= 6; r++)
        for (int c = 4; c <= 6; c++)
            if (r != 5 || c != 5)
                map.tiles[r][c] = '0';

    bombs_set_entity_list(NULL);
    bombs_update(&map);

    /* Player should be damaged or dead (small bomb = 60 HP damage, player had 50) */
    TEST_ASSERT_EQUAL_INT(1, g_players[0].dead);
    TEST_ASSERT_EQUAL_INT(1, g_players[0].deaths);
}

/* Weak explosions DESTROY already-degraded walls ('p','q','5','6'):
 * original FUN_1010_1326 (seg_1010:772) classifies only intact '7'-'9' /
 * 'A'-'F' as walls; degraded tiles take the fire branch (seg_1010:775-778).
 * Regression: the port used to put degraded tiles in the wall branch,
 * resetting them to fresh 500/1000 HP 'p'/'q' on every hit, so a wall
 * could cycle p<->q forever and never die to bombs alone. */
void test_weak_explosion_destroys_degraded_walls(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));
    g_num_active_players = 0;

    /* Small bomb at (5,5); its weak cross hits (5,4),(5,6),(4,5),(6,5) */
    map.tiles[5][5] = BOMB_SMALL_1;
    map.overlay[5][5] = 1;
    map.collision[5][5] = BOMB_COLLISION_DEFAULT;

    map.tiles[5][4] = 'p'; map.collision[5][4] = 1000;
    map.tiles[5][6] = 'q'; map.collision[5][6] = 500;
    map.tiles[4][5] = '5'; map.collision[4][5] = 300;
    map.tiles[6][5] = '6'; map.collision[6][5] = 700;

    bombs_update(&map);

    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION, map.tiles[5][4]);
    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION, map.tiles[5][6]);
    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION, map.tiles[4][5]);
    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION, map.tiles[6][5]);
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[5][4]);
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[5][6]);
}

/* The classic two-bomb wall kill: first weak hit degrades an intact wall
 * to 'p'/'q' (seg_1010:799-809), the second destroys it. */
void test_two_weak_bombs_destroy_intact_wall(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));
    g_num_active_players = 0;

    map.tiles[5][6] = '7';
    map.collision[5][6] = 3000;

    /* First bomb */
    map.tiles[5][5] = BOMB_SMALL_1;
    map.overlay[5][5] = 1;
    map.collision[5][5] = BOMB_COLLISION_DEFAULT;
    bombs_update(&map);

    uint8_t t = map.tiles[5][6];
    TEST_ASSERT_TRUE(t == 'p' || t == 'q');
    TEST_ASSERT_EQUAL_UINT16(t == 'p' ? 1000 : 500, map.collision[5][6]);

    /* Second bomb at the same spot finishes the degraded wall */
    map.tiles[5][5] = BOMB_SMALL_1;
    map.overlay[5][5] = 1;
    map.collision[5][5] = BOMB_COLLISION_DEFAULT;
    bombs_update(&map);

    TEST_ASSERT_EQUAL_HEX8(TILE_EXPLOSION, map.tiles[5][6]);
    TEST_ASSERT_EQUAL_UINT16(0, map.collision[5][6]);
}

/* BOMB DAMAGE % scales player damage in MP only (seg_1010:5378-5386:
 * raw subtraction when num_players < 2, Trunc(damage*pct/100) via the
 * Pascal real RTL otherwise). */
void test_bomb_damage_pct_scales_mp_player_damage(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_num_active_players = 2;
    place_player_at_tile(0, 5, 5, 100);
    place_player_at_tile(1, 10, 10, 100);
    g_config.bomb_damage_pct = 50;

    bombs_check_player_damage(&map, 5, 5, 60);
    TEST_ASSERT_EQUAL_INT(70, g_players[0].health);  /* 100 - 60*50/100 */

    /* At 0% MP explosions are harmless to players */
    g_config.bomb_damage_pct = 0;
    bombs_check_player_damage(&map, 5, 5, 0xFF);
    TEST_ASSERT_EQUAL_INT(70, g_players[0].health);
    TEST_ASSERT_EQUAL_INT(0, g_players[0].dead);
}

/* Single-player ignores the option (raw damage), and entities always take
 * raw damage even in MP (seg_1010:5442). */
void test_bomb_damage_pct_ignored_in_sp_and_for_entities(void)
{
    TileMap map;
    memset(&map, 0, sizeof(map));
    memset(map.bomb_owner, 0xFF, sizeof(map.bomb_owner));

    g_config.bomb_damage_pct = 50;

    g_num_active_players = 1;
    place_player_at_tile(0, 5, 5, 100);
    bombs_check_player_damage(&map, 5, 5, 60);
    TEST_ASSERT_EQUAL_INT(40, g_players[0].health);  /* raw 60 in SP */

    g_num_active_players = 2;
    place_player_at_tile(1, 10, 10, 100);
    Entity e;
    memset(&e, 0, sizeof(e));
    e.type = ENTITY_TYPE_1;
    e.health = 100;
    e.max_health = 100;
    e.x_pos = tile_to_pixel_x(7);
    e.y_pos = tile_to_pixel_y(7);
    bombs_set_entity_list(&e);

    bombs_check_player_damage(&map, 7, 7, 60);
    TEST_ASSERT_EQUAL_INT(40, e.health);  /* raw 60 despite MP + 50% */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_explosion_damages_player);
    RUN_TEST(test_explosion_kills_player);
    RUN_TEST(test_kill_credit_to_bomb_owner);
    RUN_TEST(test_no_self_kill_credit);
    RUN_TEST(test_no_damage_if_player_elsewhere);
    RUN_TEST(test_zero_damage_corpse_tile);
    RUN_TEST(test_entity_takes_explosion_damage);
    RUN_TEST(test_entity_killed_by_explosion);
    RUN_TEST(test_bomb_detonation_kills_nearby_player);
    RUN_TEST(test_weak_explosion_destroys_degraded_walls);
    RUN_TEST(test_two_weak_bombs_destroy_intact_wall);
    RUN_TEST(test_bomb_damage_pct_scales_mp_player_damage);
    RUN_TEST(test_bomb_damage_pct_ignored_in_sp_and_for_entities);
    return UNITY_END();
}
