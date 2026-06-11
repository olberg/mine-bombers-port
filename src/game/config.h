#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* 17-byte game configuration matching the original binary format.
 * See decompiled/FILE_FORMATS.md for layout. */
typedef struct {
    uint8_t  num_players;      /* 0x00: 1-4 */
    uint8_t  config_byte;      /* 0x01: game option flags */
    int16_t  total_rounds;     /* 0x02: min 1 */
    int16_t  starting_cash;    /* 0x04: MP starting wallet, 0-2650 step 100.
                                * Mislabeled "g_speed_setting" in decompiled
                                * code; process_menu_selection (seg_1010:
                                * 7169-7183) copies it into all four wallets
                                * at PLAY (SP ignores it: P1 = 250). */
    int16_t  time_limit_lo;    /* 0x06 */
    int16_t  time_limit_hi;    /* 0x08 */
    int16_t  frame_delay;      /* 0x0A */
    uint8_t  option_toggle[4]; /* 0x0C-0x0F: [0]=darkness, [1]=free_market,
                                *            [2]=selling,  [3]=winner_by
                                * Mislabeled as "player_color" in decompiled code. */
    uint8_t  starting_lives;   /* 0x10 */
} GameConfig;

/* Global game configuration */
extern GameConfig g_config;

/* Set all config values to original defaults */
void config_set_defaults(void);

/* Load config from file. Returns true on success, false on failure (defaults applied). */
bool config_load(const char *path);

/* Save config to file. Returns true on success. */
bool config_save(const char *path);

#endif
