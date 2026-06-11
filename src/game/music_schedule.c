#include "music_schedule.h"
#include "../util/prng.h"

/* Byte table at 0x1038:0x0010 (MB.EXE data segment image, file offset
 * 195856). 1-based OEKU.S3M order numbers; strictly ascending, max 83 =
 * the last order before the shop section that starts at order 84. */
static const unsigned char GAME_ORDERS[MUSIC_GAME_ORDER_COUNT] = {
    1, 5, 15, 22, 32, 39, 43, 53, 56, 62, 68, 76, 80, 83
};

int music_schedule_shop_order(void)
{
    return MUSIC_ORDER_SHOP;
}

int music_schedule_gameplay_order(void)
{
    return GAME_ORDERS[mb_random(MUSIC_GAME_ORDER_COUNT)];
}

const unsigned char *music_schedule_game_orders(void)
{
    return GAME_ORDERS;
}
