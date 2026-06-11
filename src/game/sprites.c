#include "game/sprites.h"
#include "loaders/spy_loader.h"
#include <stdlib.h>
#include <string.h>

/* Atlas texture (loaded from SIKA.SPY) */
static Texture2D atlas;
static bool      atlas_loaded = false;

/* Source rectangle lookup: indexed by tile byte value (0-255) */
static Rectangle tile_rects[256];
static bool      tile_valid[256];

/* Floor sprite rect (fallback) */
static Rectangle floor_rect;

/*
 * Tile sprite extraction coordinates from decompiled load_tile_resources().
 * Each entry maps a tile byte value to its (x, y) position in SIKA.SPY.
 * Sprites are 10x10 pixels on a 10-pixel grid.
 */
typedef struct {
    uint8_t tile_byte;
    int     x, y;
} TileSpriteEntry;

static const TileSpriteEntry TILE_SPRITES[] = {
    /* Row Y=0: Basic terrain tiles '0'-'F' (16 tiles)
     * Traced from decompiled load_tile_resources() → DAT_1038_0498..04d4
     * load_sprite_from_sheet(stack, Y, X) at seg_1010:4609-4624 */
    { '0',   0,  0 }, { '1',  10,  0 }, { '2',  20,  0 }, { '3',  30,  0 },
    { '4',  40,  0 }, { '5',  50,  0 }, { '6',  60,  0 }, { '7',  70,  0 },
    { '8',  80,  0 }, { '9',  90,  0 }, { 'A', 100,  0 }, { 'B', 110,  0 },
    { 'C', 120,  0 }, { 'D', 130,  0 }, { 'E', 140,  0 }, { 'F', 150,  0 },

    /* Row Y=10: Bombs, weapon tiles, explosion stages
     * DAT_1038_04d8..04f8 at seg_1010:4625-4646
     * 0x84/0x85/0x89/'Z'/0xB0 share same explosion sprite (DAT_04f8 = 90,10) */
    { 'W',    0, 10 }, { 'X',  10, 10 }, { 'Y',  20, 10 },
    { 'Z',   90, 10 },
    { 0x7F,  40, 10 }, { 0x80,  50, 10 }, { 0x81,  60, 10 },
    { 0x82,  70, 10 }, { 0x83,  80, 10 },
    { 0x84,  90, 10 }, { 0x85,  90, 10 }, { 0x86, 100, 10 },
    { 0x87, 110, 10 }, { 0x88, 130, 10 }, { 0x89,  90, 10 },

    /* Bomb/weapon letter tiles — Y=10 row continued
     * 'a','b' share sprites with 0x86/0x87: DAT_04fc(Y=10,X=100), DAT_0500(Y=10,X=110)
     * 'c'-'f': DAT_0504..0510 at seg_1010:4649-4652 */
    { 'a', 100, 10 }, { 'b', 110, 10 }, { 'c', 120, 10 },
    { 'd', 130, 10 }, { 'e', 140, 10 }, { 'f', 150, 10 },

    /* Row Y=20: Item/pickup tiles (wide row: X up to 310)
     * 'w','x','y': DAT_0540..0548 at seg_1010:4664-4666
     * 0x8A: DAT_0550 at (Y=20,X=50) — note Y=20, not Y=10!
     * 0x8B-0xA4: DAT_0554..05b8 at seg_1010:4669-4694 */
    { 'w',    0, 20 }, { 'x',   10, 20 }, { 'y',   20, 20 },
    { 0xA3,  40, 20 },
    { 0x8A,  50, 20 }, { 0x8B,  60, 20 }, { 0x8C,  70, 20 },
    { 0x8D,  80, 20 }, { 0x8E,  90, 20 },
    { 0x8F, 100, 20 }, { 0x90, 110, 20 }, { 0x91, 120, 20 },
    { 0x92, 130, 20 }, { 0x93, 140, 20 }, { 0x94, 150, 20 },
    { 0x95, 160, 20 }, { 0x96, 170, 20 }, { 0x97, 180, 20 },
    { 0x98, 190, 20 }, { 0x99, 200, 20 }, { 0x9A, 210, 20 },
    { 0x9B, 220, 20 }, { 0x9C, 230, 20 }, { 0x9D, 240, 20 },
    { 0x9E, 250, 20 }, { 0x9F, 260, 20 }, { 0xA0, 270, 20 },
    { 0xA1, 280, 20 }, { 0xA2, 290, 20 },
    { 0xA4, 300, 20 },
    /* 0xA5-0xA8 share DAT_05b8 (Y=20,X=310) — directional explosion */
    { 0xA5, 310, 20 }, { 0xA6, 310, 20 },
    { 0xA7, 310, 20 }, { 0xA8, 310, 20 },

    /* Row Y=30: Weapon/bomb signature tiles and game objects
     * 'g'-'l','m','o','p','q','s': DAT_0514..053c at seg_1010:4653-4663
     * Pipe/connector tiles '|','}','~': DAT_05bc..05c4 at seg_1010:4628-4630
     * 0xAA: DAT_05c8 at (Y=30,X=140) — NOT an arrow tile */
    { 'g',   20, 30 }, { 'h',   30, 30 },
    { 'i',   40, 30 }, { 'j',   50, 30 },
    { 'k',    0, 30 }, { 'l',   10, 30 }, { 'm',   60, 30 },
    { 'o',   70, 30 },
    { 'q',   90, 30 }, { 'p',  100, 30 }, { '|',  110, 30 },
    { '}',  120, 30 }, { '~',  130, 30 }, { 0xAA, 140, 30 },
    { 's',  150, 30 },

    /* 'z' (0x7A): directional rocket expansion marker — uses same sprite as 'a'
     * (fire tile at Y=10, X=100). Transient during expansion, never persists. */
    { 'z', 100, 10 },

    /* Row Y=40: Misc tiles
     * 0xAB: DAT_05cc at (Y=40,X=150)
     * 0xAF: DAT_05dc at (Y=40,X=140) */
    { 0xAF, 140, 40 }, { 0xAB, 150, 40 },

    /* Row Y=50: Extra life (0xB3)
     * DAT_05e0 at (Y=50,X=0x88=136) — not on 10px grid! */
    { 0xB3, 136, 50 },

    /* Row Y=60: Monster spawn tiles G-V (never drawn on map — replaced during load) */
    { 'G',    0, 60 }, { 'H',  10, 60 }, { 'I',  20, 60 }, { 'J',  30, 60 },
    { 'K',   40, 60 }, { 'L',  50, 60 }, { 'M',  60, 60 }, { 'N',  70, 60 },
    { 'O',   80, 60 }, { 'P',  90, 60 }, { 'Q', 100, 60 }, { 'R', 110, 60 },
    { 'S',  120, 60 }, { 'T', 130, 60 }, { 'U', 140, 60 }, { 'V', 150, 60 },

    /* Row Y=70: Reinforced walls, shop tiles
     * 0xAC-0xAE: DAT_05d0..05d8 at seg_1010:4633-4635
     * 0xB4-0xB6: DAT_05e4..05ec at seg_1010:4638-4640 */
    { 0xAC,   0, 70 }, { 0xAD,  10, 70 }, { 0xAE,  20, 70 },
    { 0xB4,  30, 70 }, { 0xB5,  40, 70 }, { 0xB6,  50, 70 },

    /* 0xB0: shared explosion sprite (same as 0x84) */
    { 0xB0,  90, 10 },
};

#define TILE_SPRITE_COUNT (sizeof(TILE_SPRITES) / sizeof(TILE_SPRITES[0]))

/*
 * Player sprite sets: 12 color variants.
 * Each set: 4 directions x 4 animation frames = 16 sprites (10x10 each).
 * In the sprite sheet, each direction band is 40px wide (4 frames x 10px).
 * X = x_base + direction * 40 + frame * 10
 * Y = y_base (single 10px row)
 *
 * Decompiled load_player_sprite_set(stack, dest, seg, param_3, param_4):
 *   capture_screen_region(buf, param_3+9, dir*40+param_4+frame*10+9, param_3, ...)
 *   → param_3 = Y coord, param_4 = X base (horizontal)
 *
 * Sets 0-3: STD colors for players 1-4 (option_toggle[i]=0)
 * Sets 4-7: ALT colors for players 1-4 (option_toggle[i]=1)
 * Sets 8-11: Monster sprite variants
 */
typedef struct {
    int x_base, y_base;
} PlayerSpriteSet;

static const PlayerSpriteSet PLAYER_SETS[12] = {
    { 160,  10 },  /* Set 0  (P1 STD): param_3=10(Y),   param_4=0xA0=160(X) */
    { 160,   0 },  /* Set 1  (P2 STD): param_3=0(Y),    param_4=0xA0=160(X) */
    { 160,  30 },  /* Set 2  (P3 STD): param_3=0x1E(Y), param_4=0xA0=160(X) */
    { 160,  40 },  /* Set 3  (P4 STD): param_3=0x28(Y), param_4=0xA0=160(X) */
    { 160, 200 },  /* Set 4  (P1 ALT): param_3=200(Y),  param_4=0xA0=160(X) */
    {   0, 200 },  /* Set 5  (P2 ALT): param_3=200(Y),  param_4=0(X)        */
    {   0, 210 },  /* Set 6  (P3 ALT): param_3=0xD2(Y), param_4=0(X)        */
    { 160, 210 },  /* Set 7  (P4 ALT): param_3=0xD2(Y), param_4=0xA0=160(X) */
    { 160,  50 },  /* Set 8  (Mon 1):  param_3=0x32(Y), param_4=0xA0=160(X) */
    { 160,  60 },  /* Set 9  (Mon 2):  param_3=0x3C(Y), param_4=0xA0=160(X) */
    { 160,  70 },  /* Set 10 (Mon 3):  param_3=0x46(Y), param_4=0xA0=160(X) */
    {   0,  80 },  /* Set 11 (Mon 4):  param_3=0x50(Y), param_4=0(X)        */
};

/* SIKA.SPY's 256-entry palette — the in-game palette. The original fades
 * the round in to the palette at DS:0x688 (seg_1000:7135), which is this
 * one; round_init installs it so HUD text and the timer bar draw with the
 * correct colors. */
static uint8_t game_palette[768];

const uint8_t *sprites_get_palette(void)
{
    return game_palette;
}

bool sprites_init(void)
{
    /* Load SIKA.SPY — get the RGBA Image plus indexed pixels for transparency */
    uint8_t palette[768];
    uint8_t *indexed = (uint8_t *)malloc(SPY_WIDTH * SPY_HEIGHT);
    if (!indexed) return false;

    Image img = LoadSPY("assets/SIKA.SPY", palette, indexed);
    if (img.width == 0) {
        free(indexed);
        return false;
    }
    memcpy(game_palette, palette, sizeof(game_palette));

    /* Make color index 0 transparent in the RGBA data */
    uint8_t *rgba = (uint8_t *)img.data;
    for (int i = 0; i < SPY_WIDTH * SPY_HEIGHT; i++) {
        if (indexed[i] == 0) {
            rgba[i * 4 + 3] = 0;  /* set alpha to 0 */
        }
    }
    free(indexed);

    atlas = LoadTextureFromImage(img);
    UnloadImage(img);
    atlas_loaded = true;

    /* Build tile lookup table */
    memset(tile_valid, 0, sizeof(tile_valid));
    memset(tile_rects, 0, sizeof(tile_rects));

    for (int i = 0; i < (int)TILE_SPRITE_COUNT; i++) {
        uint8_t byte_val = TILE_SPRITES[i].tile_byte;
        tile_rects[byte_val] = (Rectangle){
            (float)TILE_SPRITES[i].x,
            (float)TILE_SPRITES[i].y,
            (float)SPRITE_W,
            (float)SPRITE_H
        };
        tile_valid[byte_val] = true;
    }

    /* Floor rect as fallback */
    floor_rect = tile_rects['0'];

    return true;
}

void sprites_cleanup(void)
{
    if (atlas_loaded) {
        UnloadTexture(atlas);
        atlas_loaded = false;
    }
}

Rectangle sprites_get_tile_rect(uint8_t tile_byte)
{
    if (tile_valid[tile_byte]) return tile_rects[tile_byte];
    return floor_rect;
}

void sprites_draw_tile(uint8_t tile_byte, int x, int y)
{
    if (!atlas_loaded) return;
    Rectangle src = sprites_get_tile_rect(tile_byte);
    Rectangle dst = { (float)x, (float)y, (float)SPRITE_W, (float)SPRITE_H };
    DrawTexturePro(atlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

Texture2D sprites_get_atlas(void)
{
    return atlas;
}

bool sprites_has_tile(uint8_t tile_byte)
{
    return tile_valid[tile_byte];
}

Rectangle sprites_get_player_rect(int color_variant, int direction, int frame)
{
    if (color_variant < 0 || color_variant >= 12) color_variant = 0;
    if (direction < 0 || direction >= PLAYER_SPRITE_DIRS) direction = 0;
    if (frame < 0 || frame >= PLAYER_SPRITE_FRAMES) frame = 0;

    const PlayerSpriteSet *set = &PLAYER_SETS[color_variant];
    /* Sprites arranged horizontally: X = x_base + direction * 40 + frame * 10
     * Each direction band = 40px wide (4 frames x 10px side by side) */
    int x = set->x_base + direction * 40 + frame * SPRITE_W;
    int y = set->y_base;

    return (Rectangle){ (float)x, (float)y, (float)SPRITE_W, (float)SPRITE_H };
}
