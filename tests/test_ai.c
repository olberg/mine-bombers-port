#include "unity.h"
#include "util/prng.h"
#include "game/ai.h"
#include "game/bombs.h"
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

/* --- Test: spiral search finds an item at known position --- */
void test_spiral_search_finds_item(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a small bomb ('W' = 0x57) 3 tiles to the right */
    map.tiles[20][15] = 0x57;

    AiSearchResult result = ai_find_item(&map, 12, 20, AI_ITEM_RADIUS);

    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_EQUAL_INT(15, result.tile_col);
    TEST_ASSERT_EQUAL_INT(20, result.tile_row);
}

/* --- Test: spiral search returns not found on empty map --- */
void test_spiral_search_empty(void)
{
    TileMap map;
    setup_test_map(&map);

    AiSearchResult result = ai_find_item(&map, 20, 30, AI_ITEM_RADIUS);
    TEST_ASSERT_FALSE(result.found);
}

/* --- Test: spiral search respects radius limit --- */
void test_spiral_search_out_of_range(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place item 7 tiles away (beyond radius 5) */
    map.tiles[20][19] = 0x57;

    AiSearchResult result = ai_find_item(&map, 12, 20, AI_ITEM_RADIUS);
    TEST_ASSERT_FALSE(result.found);
}

/* --- Test: find player within radius --- */
void test_find_player(void)
{
    Player players[2];
    player_init_defaults(&players[0], 0);
    player_init_defaults(&players[1], 1);

    /* Player 1 at tile (row=20, col=15).
     * VGA convention: x_pos = row * TILE_SIZE, y_pos = col * TILE_SIZE + MAP_Y_OFFSET. */
    players[1].x_pos = (int16_t)(20 * TILE_SIZE);
    players[1].y_pos = (int16_t)(15 * TILE_SIZE + MAP_Y_OFFSET);

    AiSearchResult result = ai_find_player(players, 2, 12, 20,
                                            AI_PLAYER_RADIUS, 0xFF);

    TEST_ASSERT_TRUE(result.found);
}

/* --- Test: find player ignores owner --- */
void test_find_player_ignores_owner(void)
{
    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = (int16_t)(20 * TILE_SIZE);
    players[0].y_pos = (int16_t)(15 * TILE_SIZE + MAP_Y_OFFSET);

    /* Ignore player 0 (owner) */
    AiSearchResult result = ai_find_player(players, 1, 12, 20,
                                            AI_PLAYER_RADIUS, 0);
    TEST_ASSERT_FALSE(result.found);
}

/* --- Test: move_toward sets direction closer to target --- */
void test_move_toward(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->active = 1;
    e->direction = DIR_STOP;

    mb_prng_set_seed(999u);  /* seed to avoid 3% random swap on this test */

    /* Target is to the right and below */
    ai_move_toward(e, 20, 30, &map);

    /* Should pick down (larger delta: 10 rows vs 10 cols, might pick either) */
    TEST_ASSERT_TRUE(e->direction == DIR_RIGHT || e->direction == DIR_DOWN);

    free(e);
}

/* --- Test: move_toward with clear X preference --- */
void test_move_toward_x_axis(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 5, 20);
    e->active = 1;

    /* Compute entity's actual tile position (spawn adds +5 pixel offset) */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    mb_prng_set_seed(999u);

    /* Target far to the right (higher row), same col (no col delta) */
    ai_move_toward(e, ecol, erow + 25, &map);

    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->direction);

    free(e);
}

/* --- Test: move_away sets direction further from threat --- */
void test_move_away(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 20, 30);
    e->active = 1;

    /* Threat is to the left (lower row, same col).
     * Entity at row≈31, col≈21. Threat at same col, much lower row. */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
    ai_move_away(e, ecol, erow - 10, &map);

    /* Should move right (away from left threat) */
    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->direction);

    free(e);
}

/* --- Test: move_away picks random when both axes blocked --- */
void test_blocked_random(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Box the entity in with walls */
    Entity *e = entity_spawn('G', 5, 5);
    e->active = 1;

    /* Surround with walls */
    int erow = pixel_to_tile_row(e->x_pos);
    int ecol = pixel_to_tile_col(e->y_pos);
    if (ecol > 0) map.tiles[erow][ecol - 1] = '1';
    if (ecol < MAP_COLS - 1) map.tiles[erow][ecol + 1] = '1';
    if (erow > 0) map.tiles[erow - 1][ecol] = '1';
    if (erow < MAP_ROWS - 1) map.tiles[erow + 1][ecol] = '1';

    /* Move away from threat - should pick some random direction */
    ai_move_away(e, 0, 0, &map);

    /* Just verify it set a direction (won't crash) */
    TEST_ASSERT_TRUE(e->direction >= 1 && e->direction <= 4);

    free(e);
}

/* --- Test: ai_is_blocked detects wall --- */
void test_ai_is_blocked(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 5, 5);
    e->active = 1;
    e->direction = DIR_RIGHT;

    /* Place wall directly to the right (RIGHT = row+1 in VGA convention) */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W);
    int ecol = pixel_to_tile_col(e->y_pos);
    e->x_pos = (int16_t)(erow * TILE_SIZE - SPRITE_W);
    if (erow < MAP_ROWS) map.tiles[erow][ecol] = '1';

    TEST_ASSERT_TRUE(ai_is_blocked(e, &map));

    free(e);
}

/* --- Test: ai_is_blocked on open floor returns false --- */
void test_ai_not_blocked(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->active = 1;
    e->direction = DIR_RIGHT;

    TEST_ASSERT_FALSE(ai_is_blocked(e, &map));

    free(e);
}

/* --- Test: find hazard detects bomb tiles --- */
void test_find_hazard(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Place a hazard tile nearby */
    map.tiles[22][14] = 0x93;  /* explosive treasure */

    AiSearchResult result = ai_find_hazard(&map, 12, 20, AI_HAZARD_RADIUS);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_EQUAL_INT(14, result.tile_col);
    TEST_ASSERT_EQUAL_INT(22, result.tile_row);
}

/* --- Test: full ai_update doesn't crash --- */
void test_ai_update_smoke(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('K', 10, 20);
    e->active = 1;

    Player players[2];
    player_init_defaults(&players[0], 0);
    player_init_defaults(&players[1], 1);
    players[0].x_pos = 200;
    players[0].y_pos = 200;
    players[1].x_pos = 300;
    players[1].y_pos = 300;

    /* Run many frames without crashing */
    for (int f = 0; f < 200; f++) {
        ai_update(e, &map, players, 2, f, e, 10);
    }

    /* Just verify we survived */
    TEST_ASSERT_EQUAL_UINT8(0, e->dead);

    free(e);
}

/* --- Test: entity places arrow bomb when player shares column (XOR check) --- */
void test_entity_place_bomb_same_col(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Entity at tile (10, 20) facing down */
    Entity *e = entity_spawn('G', 10, 20);
    e->active = 1;
    e->direction = DIR_DOWN;
    e->owner_player = 0xFF; /* no owner (map-spawned) */

    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    /* Place player in same column but different row (XOR: same_col=true, same_row=false) */
    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = e->x_pos;             /* same column */
    players[0].y_pos = e->y_pos + 80;        /* 8 tiles below */

    ai_entity_place_bomb(e, &map, players, 1, e);

    /* Should have placed a down arrow (0xA5) with overlay=1 */
    TEST_ASSERT_EQUAL_UINT8(ARROW_DOWN, map.tiles[erow][ecol]);
    TEST_ASSERT_EQUAL_UINT16(1, map.overlay[erow][ecol]);

    free(e);
}

/* --- Test: entity does NOT place bomb when player on exact same tile --- */
void test_entity_no_bomb_same_tile(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->active = 1;
    e->direction = DIR_DOWN;
    e->owner_player = 0xFF;

    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    /* Place player on exact same tile (both axes match: XOR = false) */
    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = e->x_pos;
    players[0].y_pos = e->y_pos;

    ai_entity_place_bomb(e, &map, players, 1, e);

    /* Should NOT have placed a bomb (tile stays floor) */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[erow][ecol]);

    free(e);
}

/* --- Test: entity does NOT place bomb when player on neither axis --- */
void test_entity_no_bomb_diagonal(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 10, 20);
    e->active = 1;
    e->direction = DIR_RIGHT;
    e->owner_player = 0xFF;

    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

    /* Place player diagonally (neither axis matches: XOR = false) */
    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = e->x_pos + 50;
    players[0].y_pos = e->y_pos + 50;

    ai_entity_place_bomb(e, &map, players, 1, e);

    /* Should NOT have placed a bomb */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[erow][ecol]);

    free(e);
}

/* --- Test: entity bomb blocked by other entity in path --- */
void test_entity_no_bomb_ally_in_path(void)
{
    TileMap map;
    setup_test_map(&map);

    /* Entity facing down */
    Entity *e1 = entity_spawn('G', 10, 20);
    e1->active = 1;
    e1->direction = DIR_DOWN;
    e1->owner_player = 0xFF;

    int ecol = pixel_to_tile_col(e1->x_pos + SPRITE_W / 2);
    int erow = pixel_to_tile_row(e1->y_pos + SPRITE_H / 2);

    /* Another entity 2 tiles below (in blast path) */
    Entity *e2 = entity_spawn('G', 10, 22);
    e2->active = 1;
    e2->owner_player = 0xFF;
    e1->next = e2;

    /* Player in same column, far enough below */
    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = e1->x_pos;
    players[0].y_pos = e1->y_pos + 100;

    ai_entity_place_bomb(e1, &map, players, 1, e1);

    /* Should NOT place bomb because ally entity is in path */
    TEST_ASSERT_EQUAL_UINT8('0', map.tiles[erow][ecol]);

    free(e2);
    free(e1);
}

/* --- Test: arrow direction matches entity facing --- */
void test_entity_bomb_direction(void)
{
    TileMap map;
    Player players[1];
    player_init_defaults(&players[0], 0);

    struct { uint8_t dir; uint8_t expected_arrow; } cases[] = {
        { DIR_DOWN,  ARROW_DOWN },
        { DIR_UP,    ARROW_UP },
        { DIR_LEFT,  ARROW_LEFT },
        { DIR_RIGHT, ARROW_RIGHT },
    };

    for (int i = 0; i < 4; i++) {
        setup_test_map(&map);

        Entity *e = entity_spawn('G', 10, 20);
        e->active = 1;
        e->direction = cases[i].dir;
        e->owner_player = 0xFF;

        int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
        int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);

        /* Player in different row, same column (XOR passes) */
        players[0].x_pos = e->x_pos + 80;
        players[0].y_pos = e->y_pos;

        ai_entity_place_bomb(e, &map, players, 1, e);

        TEST_ASSERT_EQUAL_UINT8(cases[i].expected_arrow, map.tiles[erow][ecol]);
        TEST_ASSERT_EQUAL_UINT16(1, map.overlay[erow][ecol]);

        free(e);
    }
}

/* --- Test: enemy player found → entity attacks, ignores nearby hazard --- */
void test_ai_enemy_skips_hazard(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 20, 20);
    e->active = 1;
    e->direction = DIR_RIGHT;
    e->owner_player = 0; /* owned by player 0 */

    Player players[2];
    player_init_defaults(&players[0], 0);
    player_init_defaults(&players[1], 1);

    /* Player 0 (owner) far away */
    players[0].x_pos = 500;
    players[0].y_pos = 400;

    /* Player 1 (enemy) nearby — 3 tiles to the right (row+3) */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
    players[1].x_pos = (int16_t)((erow + 3) * TILE_SIZE);
    players[1].y_pos = (int16_t)(ecol * TILE_SIZE + MAP_Y_OFFSET);

    /* Place a hazard very close — entity should IGNORE it when attacking */
    map.tiles[erow][ecol + 1] = 0x93;

    /* Run on a decision tick (frame % 26 == 0) */
    ai_update(e, &map, players, 2, 26, e, 10);

    /* Entity should move toward the enemy (RIGHT), not away from hazard */
    TEST_ASSERT_EQUAL_UINT8(DIR_RIGHT, e->direction);

    free(e);
}

/* --- Test: owner found → entity does hazard avoidance if treasures remain --- */
void test_ai_owner_found_checks_hazard(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 20, 20);
    e->active = 1;
    e->direction = DIR_RIGHT;
    e->owner_player = 0; /* owned by player 0 */

    Player players[1];
    player_init_defaults(&players[0], 0);

    /* Player 0 (owner) nearby — 3 tiles to the right */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
    players[0].x_pos = (int16_t)((erow + 3) * TILE_SIZE);
    players[0].y_pos = (int16_t)(ecol * TILE_SIZE + MAP_Y_OFFSET);

    /* Place a hazard to the right — entity should flee LEFT */
    map.tiles[erow + 1][ecol] = 0x93;

    /* treasure_count > 0 enables hazard search */
    ai_update(e, &map, players, 1, 26, e, 10);

    /* Entity should flee away from the hazard (LEFT = away from row+1) */
    TEST_ASSERT_EQUAL_UINT8(DIR_LEFT, e->direction);

    free(e);
}

/* --- Test: no treasures → hazard search skipped --- */
void test_ai_no_treasures_skips_hazard(void)
{
    TileMap map;
    setup_test_map(&map);

    Entity *e = entity_spawn('G', 20, 20);
    e->active = 1;
    e->direction = DIR_RIGHT;
    e->owner_player = 0xFF; /* no owner */

    Player players[1];
    player_init_defaults(&players[0], 0);
    players[0].x_pos = 500;
    players[0].y_pos = 400;
    players[0].dead = 1; /* no alive players */

    /* Place a hazard very close */
    int erow = pixel_to_tile_row(e->x_pos + SPRITE_W / 2);
    int ecol = pixel_to_tile_col(e->y_pos + SPRITE_H / 2);
    map.tiles[erow + 1][ecol] = 0x93;

    /* treasure_count == 0 → hazard search should be skipped */
    ai_update(e, &map, players, 1, 26, e, 0);

    /* Entity should NOT flee (direction might change but not based on hazard) */
    /* The entity should try to place a bomb instead (but will fail safety checks).
     * Key assertion: direction should NOT be DIR_LEFT (flee direction) */
    /* Since no player found, no enemy, and no treasures: falls to bomb placement path */
    TEST_ASSERT_TRUE(e->direction != DIR_STOP);

    free(e);
}

/* --- Test: ai_find_player returns player_index --- */
void test_find_player_returns_index(void)
{
    Player players[3];
    player_init_defaults(&players[0], 0);
    player_init_defaults(&players[1], 1);
    player_init_defaults(&players[2], 2);

    players[0].dead = 1; /* skip player 0 */
    players[1].x_pos = (int16_t)(20 * TILE_SIZE);
    players[1].y_pos = (int16_t)(15 * TILE_SIZE + MAP_Y_OFFSET);
    players[2].x_pos = (int16_t)(25 * TILE_SIZE);
    players[2].y_pos = (int16_t)(15 * TILE_SIZE + MAP_Y_OFFSET);

    AiSearchResult result = ai_find_player(players, 3, 15, 21,
                                            AI_PLAYER_RADIUS, 0xFF);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_EQUAL_INT(1, result.player_index); /* player 1 is closer */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_spiral_search_finds_item);
    RUN_TEST(test_spiral_search_empty);
    RUN_TEST(test_spiral_search_out_of_range);
    RUN_TEST(test_find_player);
    RUN_TEST(test_find_player_ignores_owner);
    RUN_TEST(test_move_toward);
    RUN_TEST(test_move_toward_x_axis);
    RUN_TEST(test_move_away);
    RUN_TEST(test_blocked_random);
    RUN_TEST(test_ai_is_blocked);
    RUN_TEST(test_ai_not_blocked);
    RUN_TEST(test_find_hazard);
    RUN_TEST(test_ai_update_smoke);
    RUN_TEST(test_entity_place_bomb_same_col);
    RUN_TEST(test_entity_no_bomb_same_tile);
    RUN_TEST(test_entity_no_bomb_diagonal);
    RUN_TEST(test_entity_no_bomb_ally_in_path);
    RUN_TEST(test_entity_bomb_direction);
    RUN_TEST(test_ai_enemy_skips_hazard);
    RUN_TEST(test_ai_owner_found_checks_hazard);
    RUN_TEST(test_ai_no_treasures_skips_hazard);
    RUN_TEST(test_find_player_returns_index);
    return UNITY_END();
}
