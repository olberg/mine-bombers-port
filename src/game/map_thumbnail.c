#include "game/map_thumbnail.h"
#include "gfx/palette.h"
#include "raylib.h"

uint8_t map_thumbnail_color(uint8_t tile)
{
    /* Indestructible walls '2'-'4' */
    if (tile >= 0x32 && tile <= 0x34) return 12;

    /* Destructible walls '7'-'9', 'A'-'F' */
    if ((tile >= 0x37 && tile <= 0x39) || (tile >= 0x41 && tile <= 0x46)) return 9;

    /* Treasure 's' (0x73) or 0x92-0x9A */
    if (tile == 0x73 || (tile > 0x91 && tile < 0x9B)) return 5;

    /* Empty floor '0', 'f', 0xAF */
    if (tile == 0x30 || tile == 0x66 || tile == 0xAF) return 14;

    /* Damaged walls '5'-'6' */
    if (tile >= 0x35 && tile <= 0x36) return 12;

    /* Indestructible wall '1' */
    if (tile == 0x31) return 8;

    /* Special tiles: 0xA4, 'p' (0x70), 'q' (0x71) */
    if (tile == 0xA4 || tile == 0x70 || tile == 0x71) return 9;

    /* Explosive 'e' (0x65) */
    if (tile == 0x65) return 14;

    /* Mystery box 'y' (0x79) */
    if (tile == 0x79) return 12;

    /* Teleporter 0x9C */
    if (tile == 0x9C) return 12;

    /* Proximity mine 'o' (0x6F) */
    if (tile == 0x6F) return 4;

    /* Default: dark */
    return 12;
}

void map_thumbnail_draw(const TileMap *map, int x, int y)
{
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            Color c = palette_get_color(map_thumbnail_color(map->tiles[row][col]));
            DrawRectangle(x + row, y + col, 1, 1, c);
        }
    }
}
