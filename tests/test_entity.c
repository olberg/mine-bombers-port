#include "unity.h"
#include "util/prng.h"
#include "game/entity.h"
#include "game/movement.h"
#include "game/map.h"
#include "game/map_renderer.h"
#include "game/player.h"
#include "game/sprites.h"
#include <string.h>
#include <stdlib.h>

static void setup_test_map(TileMap *map)
{
    memset(map, 0, sizeof(*map));
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map->tiles[r][c] = '0';
}

void setUp(void) { mb_prng_set_seed(42u); }
void tearDown(void) {}

/* --- Test: spawn from tile G-V maps to correct type and speed --- */
void test_spawn_from_tile(void)
{
    /* G = type 0 (speed 6), facing RIGHT (original: G/K/O/S → dir 1 =
     * RIGHT per move_player dispatch) */
    Entity *e = entity_spawn('G', 5, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(ENTITY_TYPE_1, e->type);
    TEST_ASSERT_EQUAL_UINT8(6, e->speed_divisor);
    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->body.direction);
    TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);  /* starts dormant */
    free(e);

    /* H = type 0 (speed 6), facing LEFT (original: H/L/P/T → dir 2) */
    e = entity_spawn('H', 5, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(DIR_LEFT, e->body.direction);
    free(e);

    /* K = type 1 (speed 3), facing right (1st in group) */
    e = entity_spawn('K', 5, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(ENTITY_TYPE_2, e->type);
    TEST_ASSERT_EQUAL_UINT8(3, e->speed_divisor);
    free(e);

    /* O = type 2 (speed 2), facing right (1st in group) */
    e = entity_spawn('O', 5, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(ENTITY_TYPE_3, e->type);
    TEST_ASSERT_EQUAL_UINT8(2, e->speed_divisor);
    free(e);

    /* S = type 3 (speed 100), facing right (1st in group) */
    e = entity_spawn('S', 5, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(ENTITY_TYPE_4, e->type);
    TEST_ASSERT_EQUAL_UINT8(100, e->speed_divisor);
    free(e);

    /* V = type 3 (speed 100), facing DOWN (4th in group: J/N/R/V → dir 4) */
    e = entity_spawn('V', 5, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(ENTITY_TYPE_4, e->type);
    TEST_ASSERT_EQUAL_UINT8(DIR_DOWN, e->body.direction);
    free(e);

    /* Invalid tile returns NULL */
    e = entity_spawn('A', 5, 10);
    TEST_ASSERT_NULL(e);

    e = entity_spawn('Z', 5, 10);
    TEST_ASSERT_NULL(e);
}

/* --- Test: entity activation by proximity --- */
void test_entity_activation(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);

    Player players[1];
    player_init_defaults(&players[0], 0);

    /* Player far away: entity stays dormant */
    players[0].x_pos = 0;
    players[0].y_pos = 0;
    entities_activate(e, players, 1, &map);
    TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);

    /* Player within 20 pixels: entity activates */
    players[0].x_pos = e->body.x_pos + 10;
    players[0].y_pos = e->body.y_pos + 10;
    entities_activate(e, players, 1, &map);
    TEST_ASSERT_EQUAL_UINT8(1, e->body.awake);

    free(e);
}

/* --- Test: entity stays active once activated --- */
void test_entity_stays_active(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->body.awake = 1;  /* force active */

    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = 0;
    players[0].y_pos = 0;  /* far away */

    /* Entity should stay active even with no player nearby */
    entities_update(e, &map, players, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(1, e->body.awake);

    free(e);
}

/* --- Test: collision deals damage to player on same tile --- */
void test_collision_damage(void)
{
    Entity *e = entity_spawn('K', 10, 20);
    TEST_ASSERT_NOT_NULL(e);
    e->body.awake = 1;

    Player players[1];
    player_init_defaults(&players[0], 0);
    int16_t starting_health = players[0].health;

    /* Place player on same tile as entity (exact tile match) */
    players[0].x_pos = e->body.x_pos;
    players[0].y_pos = e->body.y_pos;

    entities_deal_damage(e, players, 1);

    TEST_ASSERT_LESS_THAN(starting_health, players[0].health);
    TEST_ASSERT_EQUAL_INT16(starting_health - e->body.health, players[0].health);

    free(e);
}

/* --- Test: owner player is immune to damage --- */
void test_owner_immunity(void)
{
    Entity *e = entity_spawn_creature(0, NULL, 10, 20);
    TEST_ASSERT_NOT_NULL(e);

    Player players[2];
    player_init_defaults(&players[0], 0);
    player_init_defaults(&players[1], 1);
    int16_t p0_health = players[0].health;
    int16_t p1_health = players[1].health;

    /* Both players on same tile as entity */
    players[0].x_pos = e->body.x_pos;
    players[0].y_pos = e->body.y_pos;
    players[1].x_pos = e->body.x_pos;
    players[1].y_pos = e->body.y_pos;

    entities_deal_damage(e, players, 2);

    /* Player 0 (owner) should be unharmed */
    TEST_ASSERT_EQUAL_INT16(p0_health, players[0].health);

    /* Player 1 should take damage */
    TEST_ASSERT_LESS_THAN(p1_health, players[1].health);

    free(e);
}

/* --- Test: dead entity doesn't deal damage --- */
void test_dead_entity_no_collision(void)
{
    Entity *e = entity_spawn('G', 10, 20);
    e->body.awake = 1;
    e->body.dead = 1;

    Player players[1];
    player_init_defaults(&players[0], 0);
    int16_t starting_health = players[0].health;
    players[0].x_pos = e->body.x_pos;
    players[0].y_pos = e->body.y_pos;

    entities_deal_damage(e, players, 1);
    TEST_ASSERT_EQUAL_INT16(starting_health, players[0].health);

    free(e);
}

/* --- Test: entity movement on open floor --- */
void test_entity_movement(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->body.awake = 1;
    e->body.direction = DIR_RIGHT;
    int16_t start_x = e->body.x_pos;

    bool moved = entity_move(e, &map, NULL, 0);
    TEST_ASSERT_TRUE(moved);
    TEST_ASSERT_EQUAL_INT16(start_x + 1, e->body.x_pos);

    free(e);
}

/* --- Test: entity blocked by wall --- */
void test_entity_blocked_by_wall(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->body.awake = 1;
    e->body.direction = DIR_RIGHT;

    /* Place wall to the right (RIGHT = row+1 in VGA convention) */
    int erow = pixel_to_tile_row(e->body.x_pos + SPRITE_W);
    int ecol = pixel_to_tile_col(e->body.y_pos);
    if (erow < MAP_ROWS && ecol < MAP_COLS)
        map.tiles[erow][ecol] = '1';

    /* Move entity to right edge of current tile */
    e->body.x_pos = (int16_t)(erow * TILE_SIZE - SPRITE_W);

    bool moved = entity_move(e, &map, NULL, 0);
    TEST_ASSERT_FALSE(moved);

    free(e);
}

/* --- Test: linked list operations --- */
void test_linked_list(void)
{
    Entity *head = NULL;

    Entity *e1 = entity_spawn('G', 5, 10);
    Entity *e2 = entity_spawn('K', 10, 20);
    Entity *e3 = entity_spawn('O', 15, 30);

    entity_list_add(&head, e1);
    entity_list_add(&head, e2);
    entity_list_add(&head, e3);

    TEST_ASSERT_EQUAL_INT(3, entities_count_alive(head));

    entity_kill(e2);
    TEST_ASSERT_EQUAL_INT(2, entities_count_alive(head));

    entities_cleanup(&head);
    TEST_ASSERT_NULL(head);
}

/* --- Test: creature spawner creates active entity --- */
void test_spawn_creature(void)
{
    Entity *e = entity_spawn_creature(2, NULL, 10, 20);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(1, e->body.awake);  /* starts active */
    TEST_ASSERT_EQUAL_UINT8(2, e->owner_player);
    /* Bot template speed, DAT_1038_232e=100 (seg_1010:5630) */
    TEST_ASSERT_EQUAL_UINT8(100, e->speed_divisor);
    free(e);
}

/* --- Test: speed throttling --- */
void test_speed_throttling(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 30);  /* speed 6 */
    e->body.awake = 1;
    e->body.direction = DIR_RIGHT;

    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = 0;
    players[0].y_pos = 0;

    int16_t start_x = e->body.x_pos;

    /* Frame 0: frame % 6 == 0, entity does NOT move */
    entities_update(e, &map, players, 1, 0);
    TEST_ASSERT_EQUAL_INT16(start_x, e->body.x_pos);

    /* Frame 1: frame % 6 != 0, entity moves */
    entities_update(e, &map, players, 1, 1);
    TEST_ASSERT_EQUAL_INT16(start_x + 1, e->body.x_pos);

    free(e);
}

/* --- Test: activation by rectangular LOS (same row, clear path) --- */
void test_activation_rect_los(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Entity at tile (10, 10), player at tile (10, 15) — same row, 5 tiles
     * apart. 'J' faces DOWN (+col), toward the player below. */
    Entity *e = entity_spawn('J', 10, 10);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);

    Player players[1];
    player_init_defaults(&players[0], 0);
    /* Place player 5 tiles to the right on the same row */
    players[0].x_pos = e->body.x_pos;
    players[0].y_pos = e->body.y_pos + 50;  /* same row, different col (Y-axis) */

    entities_activate(e, players, 1, &map);
    TEST_ASSERT_EQUAL_UINT8(1, e->body.awake);

    free(e);
}

/* --- Test: activation blocked by wall in LOS rectangle --- */
void test_activation_rect_los_blocked(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Entity facing LEFT — fan scans leftward, player is to the right */
    Entity *e = entity_spawn('H', 10, 10);  /* H = type 0, facing LEFT */
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(DIR_LEFT, e->body.direction);

    Player players[1];
    player_init_defaults(&players[0], 0);

    int e_row = pixel_to_tile_row(e->body.x_pos);
    int e_col = pixel_to_tile_col(e->body.y_pos);

    /* Player on same row, 5 tiles ahead in col direction (behind entity's
     * facing direction LEFT). Fan won't detect, proximity won't match. */
    players[0].x_pos = e->body.x_pos;
    players[0].y_pos = e->body.y_pos + 50;  /* same row, different col */

    /* Place wall between entity and player on LOS path */
    map.tiles[e_row][e_col + 2] = '1';

    entities_activate(e, players, 1, &map);
    TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);  /* wall blocks LOS, fan faces wrong way */

    free(e);
}

/* --- Test: activation by directional fan (player in fan cone) --- */
void test_activation_directional_fan(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Entity facing RIGHT at tile (10, 10) */
    Entity *e = entity_spawn('G', 10, 10);  /* G = type 0, facing RIGHT */
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->body.direction);

    Player players[1];
    player_init_defaults(&players[0], 0);

    int e_row = pixel_to_tile_row(e->body.x_pos);
    int e_col = pixel_to_tile_col(e->body.y_pos);

    /* Place player 3 tiles ahead and 2 tiles to the side (within fan) */
    players[0].x_pos = (int16_t)tile_to_pixel_x(e_row + 3) + 5;
    players[0].y_pos = (int16_t)tile_to_pixel_y(e_col + 2) + 5;

    /* At ring=3, spread=2, so offset ±2 is within range */
    entities_activate(e, players, 1, &map);
    TEST_ASSERT_EQUAL_UINT8(1, e->body.awake);

    free(e);
}

/* --- Test: activation fan does NOT detect behind entity --- */
void test_activation_fan_no_behind(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Entity facing RIGHT at tile (20, 20) ('G' = RIGHT) */
    Entity *e = entity_spawn('G', 20, 20);
    TEST_ASSERT_NOT_NULL(e);

    Player players[1];
    player_init_defaults(&players[0], 0);

    int e_row = pixel_to_tile_row(e->body.x_pos);
    int e_col = pixel_to_tile_col(e->body.y_pos);

    /* Place player BEHIND entity (3 tiles to the left) */
    players[0].x_pos = (int16_t)tile_to_pixel_x(e_row - 3) + 5;
    players[0].y_pos = (int16_t)tile_to_pixel_y(e_col) + 5;

    /* Fan only looks forward (RIGHT), not behind (LEFT) */
    entities_activate(e, players, 1, &map);
    TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);

    free(e);
}

/* --- Test: damage uses tile-matching, not pixel proximity --- */
void test_damage_tile_matching(void)
{
    Entity *e = entity_spawn('G', 10, 10);
    TEST_ASSERT_NOT_NULL(e);
    e->body.awake = 1;

    Player players[1];
    player_init_defaults(&players[0], 0);
    int16_t starting_health = players[0].health;

    /* Place player on adjacent tile (close in pixels but different tile) */
    int e_row = pixel_to_tile_row(e->body.x_pos);
    int e_col = pixel_to_tile_col(e->body.y_pos);
    players[0].x_pos = (int16_t)tile_to_pixel_x(e_row + 1) + 5;
    players[0].y_pos = (int16_t)tile_to_pixel_y(e_col) + 5;

    entities_deal_damage(e, players, 1);

    /* Player on different tile: should NOT take damage */
    TEST_ASSERT_EQUAL_INT16(starting_health, players[0].health);

    free(e);
}

/* --- Test: entity spawn position equivalence to original +5/-5 system ---
 * Original stores center: x_pos = tile_row * 10 + 5, y_pos = tile_col * 10 + 35.
 * Original draws at pos - 5, netting to tile top-left.
 * Port stores top-left directly: x_pos = tile_row * 10, y_pos = tile_col * 10 + 30.
 *
 * Verify: (a) draw position matches, (b) tile lookup matches, (c) collision consistent.
 * Decompiled ref: seg_1000:2530-2531 (store), 3782-3783 (draw at -5),
 *                 5834-5835 (tile lookup: offset_0xEE/10, (offset_0xF0-0x1E)/10). */
void test_entity_spawn_position_equivalence(void)
{
    int test_row = 15, test_col = 22;

    /* Port entity */
    Entity *e = entity_spawn('G', test_col, test_row);
    TEST_ASSERT_NOT_NULL(e);

    /* (a) Draw position equals tile top-left: row*10, col*10+30 */
    int expected_draw_x = test_row * TILE_SIZE;
    int expected_draw_y = test_col * TILE_SIZE + MAP_Y_OFFSET;
    TEST_ASSERT_EQUAL_INT(expected_draw_x, e->body.x_pos);
    TEST_ASSERT_EQUAL_INT(expected_draw_y, e->body.y_pos);

    /* Original would store center: row*10+5, col*10+35, and draw at pos-5 */
    int original_stored_x = test_row * 10 + 5;
    int original_stored_y = test_col * 10 + 0x23;  /* 0x23 = 35 */
    int original_draw_x = original_stored_x - 5;
    int original_draw_y = original_stored_y - 5;
    TEST_ASSERT_EQUAL_INT(original_draw_x, e->body.x_pos);
    TEST_ASSERT_EQUAL_INT(original_draw_y, e->body.y_pos);

    /* (b) Tile lookup: both produce same tile coordinates */
    int port_tile_row = pixel_to_tile_row(e->body.x_pos);
    int port_tile_col = pixel_to_tile_col(e->body.y_pos);
    /* Original: offset_0xEE / 10 = (row*10+5)/10 = row */
    int orig_tile_row = original_stored_x / 10;
    /* Original: (offset_0xF0 - 0x1E) / 10 = (col*10+35-30)/10 = col */
    int orig_tile_col = (original_stored_y - 0x1E) / 10;
    TEST_ASSERT_EQUAL_INT(orig_tile_row, port_tile_row);
    TEST_ASSERT_EQUAL_INT(orig_tile_col, port_tile_col);
    TEST_ASSERT_EQUAL_INT(test_row, port_tile_row);
    TEST_ASSERT_EQUAL_INT(test_col, port_tile_col);

    free(e);
}

/* --- Test: creature spawn position equivalence --- */
void test_creature_spawn_position_equivalence(void)
{
    int test_row = 30, test_col = 10;

    Entity *e = entity_spawn_creature(0, NULL, test_col, test_row);
    TEST_ASSERT_NOT_NULL(e);

    /* Same as entity_spawn: position at tile top-left */
    TEST_ASSERT_EQUAL_INT(test_row * TILE_SIZE, e->body.x_pos);
    TEST_ASSERT_EQUAL_INT(test_col * TILE_SIZE + MAP_Y_OFFSET, e->body.y_pos);

    /* Tile lookup consistent */
    TEST_ASSERT_EQUAL_INT(test_row, pixel_to_tile_row(e->body.x_pos));
    TEST_ASSERT_EQUAL_INT(test_col, pixel_to_tile_col(e->body.y_pos));

    free(e);
}

/* --- Test: entity/player damage uses same coordinate convention ---
 * Both entity and player positions use top-left convention, so tile
 * matching in entities_deal_damage() is consistent. */
void test_entity_player_same_convention(void)
{
    int row = 20, col = 15;

    Entity *e = entity_spawn('G', col, row);
    TEST_ASSERT_NOT_NULL(e);
    e->body.awake = 1;

    Player players[1];
    player_init_defaults(&players[0], 0);

    /* Place player at same tile using same convention */
    players[0].x_pos = tile_to_pixel_x(row);
    players[0].y_pos = tile_to_pixel_y(col);

    int16_t hp_before = players[0].health;
    entities_deal_damage(e, players, 1);

    /* Should take damage (same tile) */
    TEST_ASSERT_LESS_THAN(hp_before, players[0].health);

    /* Place player one tile away */
    players[0].health = hp_before;
    players[0].x_pos = tile_to_pixel_x(row + 1);
    entities_deal_damage(e, players, 1);

    /* Should NOT take damage (different tile) */
    TEST_ASSERT_EQUAL_INT16(hp_before, players[0].health);

    free(e);
}

/* ---- Monsters go through the shared movement routine ---- */

/* The tile in front of a monster spawned at (row 20, col 10) facing right */
#define AHEAD_ROW 21
#define AHEAD_COL 10

/* --- Test: the templates' names, dig strength and animation counter --- */
void test_spawn_template_fields(void)
{
    static const struct { uint8_t tile; const char *name; int dig, attack, hp, speed; } T[] = {
        { 'G', "KarvaMies",    5,  2, 29, 6 },
        { 'K', "HarmaaPeikko", 12, 3, 29, 3 },
        { 'O', "LimaPeikko",   12, 1, 10, 2 },
        { 'S', "Alien",        52, 5, 66, 100 },
    };
    for (int i = 0; i < 4; i++) {
        Entity *e = entity_spawn(T[i].tile, 10, 20);
        TEST_ASSERT_EQUAL_STRING(T[i].name, e->body.name);
        TEST_ASSERT_EQUAL_INT16(0, e->body.digging_power);
        TEST_ASSERT_EQUAL_INT16(T[i].dig, e->body.bonus_stat);
        TEST_ASSERT_EQUAL_INT16(T[i].attack, e->body.health);
        TEST_ASSERT_EQUAL_INT16(T[i].hp, e->body.max_health);
        TEST_ASSERT_EQUAL_UINT8(T[i].speed, e->speed_divisor);
        TEST_ASSERT_EQUAL_INT16(1, e->body.anim_frame);
        TEST_ASSERT_EQUAL_UINT8(0, e->body.awake);
        TEST_ASSERT_EQUAL_UINT8(0, e->body.has_stats);
        free(e);
    }
}

/* --- Test: a monster facing treasure takes it and stays put that frame --- */
void test_monster_takes_treasure_without_moving(void)
{
    TileMap map;
    setup_test_map(&map);
    map.tiles[AHEAD_ROW][AHEAD_COL] = 0x95;   /* worth 10 */

    Entity *e = entity_spawn('S', 10, 20);
    int16_t x = e->body.x_pos;

    TEST_ASSERT_FALSE(entity_move(e, &map, NULL, 0));
    TEST_ASSERT_EQUAL_INT16(x, e->body.x_pos);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[AHEAD_ROW][AHEAD_COL]);
    TEST_ASSERT_EQUAL_INT32(10, e->body.earned);

    /* The way is open now: the next call steps */
    TEST_ASSERT_TRUE(entity_move(e, &map, NULL, 0));
    TEST_ASSERT_EQUAL_INT16(x + 1, e->body.x_pos);
    free(e);
}

/* --- Test: a monster digs with its own dig strength (ANZULABY round
 * start in the original: boulder 24 -> 12, reinforced wall 8000 -> 7948) --- */
void test_monster_digs_with_its_dig_strength(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *lima = entity_spawn('O', 10, 20);
    map.tiles[AHEAD_ROW][AHEAD_COL] = 'B';
    map.collision[AHEAD_ROW][AHEAD_COL] = 24;
    TEST_ASSERT_FALSE(entity_move(lima, &map, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(12, map.collision[AHEAD_ROW][AHEAD_COL]);
    free(lima);

    Entity *alien = entity_spawn('S', 10, 20);
    map.tiles[AHEAD_ROW][AHEAD_COL] = 0xAC;
    map.collision[AHEAD_ROW][AHEAD_COL] = 8000;
    TEST_ASSERT_FALSE(entity_move(alien, &map, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(7948, map.collision[AHEAD_ROW][AHEAD_COL]);
    free(alien);
}

/* --- Test: a monster digs sand away and then walks in --- */
void test_monster_digs_through_sand(void)
{
    TileMap map;
    setup_test_map(&map);
    map.tiles[AHEAD_ROW][AHEAD_COL] = '2';
    map.collision[AHEAD_ROW][AHEAD_COL] = 10;

    Entity *e = entity_spawn('G', 10, 20);    /* dig strength 5 */
    int16_t x = e->body.x_pos;
    entity_move(e, &map, NULL, 0);
    TEST_ASSERT_EQUAL_UINT16(5, map.collision[AHEAD_ROW][AHEAD_COL]);
    entity_move(e, &map, NULL, 0);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[AHEAD_ROW][AHEAD_COL]);
    TEST_ASSERT_EQUAL_INT16(x, e->body.x_pos);
    TEST_ASSERT_TRUE(entity_move(e, &map, NULL, 0));
    free(e);
}

/* --- Test: a crate is opened: tile gone, random numbers drawn --- */
void test_monster_opens_crate(void)
{
    TileMap map;
    setup_test_map(&map);
    map.tiles[AHEAD_ROW][AHEAD_COL] = 0x79;

    mb_prng_set_seed(7u);
    int untouched = mb_random(1000);

    Entity *e = entity_spawn('O', 10, 20);
    mb_prng_set_seed(7u);
    entity_move(e, &map, NULL, 0);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[AHEAD_ROW][AHEAD_COL]);
    TEST_ASSERT_NOT_EQUAL(untouched, mb_random(1000));
    free(e);
}

/* --- Test: the medkit rule, +0x1B = +0x1D unless awake --- */
void test_monster_medkit(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Asleep: its contact damage becomes its hit points */
    Entity *asleep = entity_spawn('G', 10, 20);
    map.tiles[AHEAD_ROW][AHEAD_COL] = 'm';
    entity_move(asleep, &map, NULL, 0);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[AHEAD_ROW][AHEAD_COL]);
    TEST_ASSERT_EQUAL_INT16(29, asleep->body.health);
    free(asleep);

    /* Awake: the medkit is used up and nothing else changes */
    Entity *awake = entity_spawn('G', 10, 20);
    awake->body.awake = 1;
    map.tiles[AHEAD_ROW][AHEAD_COL] = 'm';
    entity_move(awake, &map, NULL, 0);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[AHEAD_ROW][AHEAD_COL]);
    TEST_ASSERT_EQUAL_INT16(2, awake->body.health);
    free(awake);
}

/* --- Test: a monster walking in the dark reveals nothing --- */
void test_monster_does_not_light_the_dark(void)
{
    TileMap map;
    setup_test_map(&map);
    map.darkness_enabled = true;
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            map.layer4[r][c] |= 0x01;

    Entity *e = entity_spawn('G', 10, 20);
    TEST_ASSERT_TRUE(entity_move(e, &map, NULL, 0));
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            TEST_ASSERT_EQUAL_HEX8(0x01, map.layer4[r][c] & 0x01);
    free(e);
}

/* --- Test: an awake Robot's finds also go to its owner --- */
void test_robot_credits_its_owner(void)
{
    TileMap map;
    setup_test_map(&map);
    Player players[2];
    player_init_defaults(&players[0], 0);
    player_init_defaults(&players[1], 1);
    players[1].direction = DIR_RIGHT;
    players[1].last_direction = DIR_RIGHT;
    players[1].bonus_stat = 4;

    Entity *e = entity_spawn_creature(1, &players[1], 10, 20);
    TEST_ASSERT_EQUAL_STRING("Robot", e->body.name);
    TEST_ASSERT_TRUE(entity_is_robot(e));
    TEST_ASSERT_EQUAL_UINT8(1, e->body.awake);
    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->body.direction);
    TEST_ASSERT_EQUAL_INT16(1, e->body.digging_power);
    TEST_ASSERT_EQUAL_INT16(4, e->body.bonus_stat);
    TEST_ASSERT_EQUAL_INT16(1, e->body.health);
    TEST_ASSERT_EQUAL_INT16(100, e->body.max_health);

    map.tiles[AHEAD_ROW][AHEAD_COL] = 0x9A;   /* worth 100 */
    entity_move(e, &map, players, 2);
    TEST_ASSERT_EQUAL_INT32(100, players[1].earned);
    TEST_ASSERT_EQUAL_INT32(0, players[0].earned);

    map.tiles[AHEAD_ROW][AHEAD_COL] = 0x90;   /* dig gem +3 */
    entity_move(e, &map, players, 2);
    TEST_ASSERT_EQUAL_INT16(4, players[1].digging_power);
    TEST_ASSERT_EQUAL_INT16(1, players[0].digging_power);
    free(e);
}

/* --- Test: a Robot made during a money bomb loan does not get the loan's 300 --- */
void test_robot_dig_power_without_the_loan(void)
{
    Player owner;
    player_init_defaults(&owner, 0);
    owner.digging_power = 301;
    owner.money_bomb_counter = 5;

    Entity *e = entity_spawn_creature(0, &owner, 10, 20);
    TEST_ASSERT_EQUAL_INT16(1, e->body.digging_power);
    free(e);
}

/* --- Test: a map monster's treasure goes to nobody --- */
void test_map_monster_credits_nobody(void)
{
    TileMap map;
    setup_test_map(&map);
    Player players[1];
    player_init_defaults(&players[0], 0);
    map.tiles[AHEAD_ROW][AHEAD_COL] = 0x9A;

    Entity *e = entity_spawn('G', 10, 20);
    e->body.awake = 1;
    entity_move(e, &map, players, 1);
    TEST_ASSERT_EQUAL_INT32(0, players[0].earned);
    free(e);
}

/* --- Test: the round-start step moves every monster once, asleep or not --- */
void test_round_start_step(void)
{
    TileMap map;
    setup_test_map(&map);
    Entity *head = NULL;
    Entity *walker = entity_spawn('G', 10, 20);   /* faces open floor */
    Entity *eater = entity_spawn('G', 30, 20);    /* faces a coin */
    entity_list_add(&head, walker);
    entity_list_add(&head, eater);
    map.tiles[21][30] = 0x95;
    int16_t wx = walker->body.x_pos, ex = eater->body.x_pos;

    entities_round_start_step(head, &map, NULL, 0);

    TEST_ASSERT_EQUAL_INT16(wx + 1, walker->body.x_pos);
    TEST_ASSERT_EQUAL_INT16(ex, eater->body.x_pos);
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[21][30]);
    TEST_ASSERT_EQUAL_INT16(2, walker->body.anim_frame);
    TEST_ASSERT_EQUAL_UINT8(0, walker->body.awake);
    entities_cleanup(&head);
}

/* --- Test: the list keeps spawn order (the original appends) --- */
void test_list_add_appends(void)
{
    Entity *head = NULL;
    Entity *e1 = entity_spawn('G', 5, 10);
    Entity *e2 = entity_spawn('K', 10, 20);
    Entity *e3 = entity_spawn('O', 15, 30);
    entity_list_add(&head, e1);
    entity_list_add(&head, e2);
    entity_list_add(&head, e3);
    TEST_ASSERT_EQUAL_PTR(e1, head);
    TEST_ASSERT_EQUAL_PTR(e2, head->next);
    TEST_ASSERT_EQUAL_PTR(e3, head->next->next);
    TEST_ASSERT_NULL(e3->next);
    entities_cleanup(&head);
}

/* --- Test: entity_tick: asleep does nothing; awake hurts, moves, faces --- */
void test_entity_tick(void)
{
    TileMap map;
    setup_test_map(&map);
    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = (int16_t)tile_to_pixel_x(20);
    players[0].y_pos = (int16_t)tile_to_pixel_y(10);

    Entity *e = entity_spawn('G', 10, 20);        /* same tile as the player */
    int16_t x = e->body.x_pos;
    entity_tick(e, &map, players, 1, 1);
    TEST_ASSERT_EQUAL_INT16(100, players[0].health);
    TEST_ASSERT_EQUAL_INT16(x, e->body.x_pos);

    e->body.awake = 1;
    e->body.last_direction = DIR_UP;
    entity_tick(e, &map, players, 1, 1);
    TEST_ASSERT_EQUAL_INT16(98, players[0].health);
    TEST_ASSERT_EQUAL_INT16(x + 1, e->body.x_pos);
    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->body.last_direction);

    /* frame % speed == 0: damage but no move */
    entity_tick(e, &map, players, 1, 6);
    TEST_ASSERT_EQUAL_INT16(96, players[0].health);
    TEST_ASSERT_EQUAL_INT16(x + 1, e->body.x_pos);
    free(e);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_spawn_from_tile);
    RUN_TEST(test_entity_activation);
    RUN_TEST(test_entity_stays_active);
    RUN_TEST(test_collision_damage);
    RUN_TEST(test_owner_immunity);
    RUN_TEST(test_dead_entity_no_collision);
    RUN_TEST(test_entity_movement);
    RUN_TEST(test_entity_blocked_by_wall);
    RUN_TEST(test_linked_list);
    RUN_TEST(test_spawn_creature);
    RUN_TEST(test_speed_throttling);
    RUN_TEST(test_activation_rect_los);
    RUN_TEST(test_activation_rect_los_blocked);
    RUN_TEST(test_activation_directional_fan);
    RUN_TEST(test_activation_fan_no_behind);
    RUN_TEST(test_damage_tile_matching);
    RUN_TEST(test_entity_spawn_position_equivalence);
    RUN_TEST(test_creature_spawn_position_equivalence);
    RUN_TEST(test_entity_player_same_convention);
    RUN_TEST(test_spawn_template_fields);
    RUN_TEST(test_monster_takes_treasure_without_moving);
    RUN_TEST(test_monster_digs_with_its_dig_strength);
    RUN_TEST(test_monster_digs_through_sand);
    RUN_TEST(test_monster_opens_crate);
    RUN_TEST(test_monster_medkit);
    RUN_TEST(test_monster_does_not_light_the_dark);
    RUN_TEST(test_robot_credits_its_owner);
    RUN_TEST(test_robot_dig_power_without_the_loan);
    RUN_TEST(test_map_monster_credits_nobody);
    RUN_TEST(test_round_start_step);
    RUN_TEST(test_list_add_appends);
    RUN_TEST(test_entity_tick);
    return UNITY_END();
}
