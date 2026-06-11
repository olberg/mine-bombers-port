#include "config.h"
#include <stdio.h>
#include <string.h>

GameConfig g_config;

void config_set_defaults(void)
{
    g_config.num_players    = 2;
    g_config.treasures    = 0x2D;
    g_config.total_rounds   = 15;
    g_config.starting_cash  = 750;
    /* Factory default time limit: 0x1DEE (decompiled seg_1010:5486).
     * No config file ships with the game; the original writes OPTIONS.CFG
     * on options exit (verified: a game-written defaults file is
     * byte-identical to config_save's output). Value 0 = no time limit;
     * min settable through options = 1; step = 0x111; max = 0x60AE. */
    g_config.time_limit_lo  = 0x1DEE;
    g_config.time_limit_hi  = 0;
    g_config.frame_delay    = 8;
    g_config.option_toggle[0] = 0;  /* darkness off */
    g_config.option_toggle[1] = 0;  /* free_market off */
    g_config.option_toggle[2] = 0;  /* selling off */
    g_config.option_toggle[3] = 0;  /* winner_by off */
    g_config.bomb_damage_pct = 100;
}

bool config_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        config_set_defaults();
        return false;
    }

    uint8_t buf[17];
    size_t n = fread(buf, 1, 17, f);
    fclose(f);

    if (n != 17) {
        config_set_defaults();
        return false;
    }

    g_config.num_players    = buf[0];
    g_config.treasures    = buf[1];
    g_config.total_rounds   = (int16_t)(buf[2] | (buf[3] << 8));
    g_config.starting_cash  = (int16_t)(buf[4] | (buf[5] << 8));
    g_config.time_limit_lo  = (int16_t)(buf[6] | (buf[7] << 8));
    g_config.time_limit_hi  = (int16_t)(buf[8] | (buf[9] << 8));
    g_config.frame_delay    = (int16_t)(buf[10] | (buf[11] << 8));
    g_config.option_toggle[0] = buf[12];
    g_config.option_toggle[1] = buf[13];
    g_config.option_toggle[2] = buf[14];
    g_config.option_toggle[3] = buf[15];
    g_config.bomb_damage_pct = buf[16];

    /* Enforce min 1 round (matches original) */
    if (g_config.total_rounds < 1)
        g_config.total_rounds = 1;

    return true;
}

bool config_save(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    uint8_t buf[17];
    buf[0]  = g_config.num_players;
    buf[1]  = g_config.treasures;
    buf[2]  = (uint8_t)(g_config.total_rounds & 0xFF);
    buf[3]  = (uint8_t)((g_config.total_rounds >> 8) & 0xFF);
    buf[4]  = (uint8_t)(g_config.starting_cash & 0xFF);
    buf[5]  = (uint8_t)((g_config.starting_cash >> 8) & 0xFF);
    buf[6]  = (uint8_t)(g_config.time_limit_lo & 0xFF);
    buf[7]  = (uint8_t)((g_config.time_limit_lo >> 8) & 0xFF);
    buf[8]  = (uint8_t)(g_config.time_limit_hi & 0xFF);
    buf[9]  = (uint8_t)((g_config.time_limit_hi >> 8) & 0xFF);
    buf[10] = (uint8_t)(g_config.frame_delay & 0xFF);
    buf[11] = (uint8_t)((g_config.frame_delay >> 8) & 0xFF);
    buf[12] = g_config.option_toggle[0];
    buf[13] = g_config.option_toggle[1];
    buf[14] = g_config.option_toggle[2];
    buf[15] = g_config.option_toggle[3];
    buf[16] = g_config.bomb_damage_pct;

    size_t n = fwrite(buf, 1, 17, f);
    fclose(f);
    return n == 17;
}
