#include "asset_check.h"
#include "raylib.h"
#include <stdio.h>

/* Every asset referenced by name in the code. Sources:
 * .SPY/.FON: title.c, menu.c, options.c, sound_config.c, info.c,
 *   key_config.c, map_picker.c, player_select.c, shop.c, hud.c,
 *   results.c, hall_of_fame.c, sp_complete.c, sp_level_complete.c,
 *   sprites.c
 * .S3M: main.c   .VOC: sfx.c   .PPM: results.c (4 colors x 3 outcomes)
 * KOMPLEX.MNE: main.c build_map_path fallback */
static const char *REQUIRED_FILES[] = {
    /* Backgrounds + sprite sheet (.SPY) */
    "TITLEBE.SPY", "MAIN3.SPY", "SIKA.SPY", "OPTIONS5.SPY", "KEYS.SPY",
    "LEVSELEC.SPY", "IDENTIFW.SPY", "SHOPPIC.SPY", "PLAYERS.SPY",
    "FINAL.SPY", "HALLOFFA.SPY", "GAMEOVER.SPY", "CONGRATU.SPY",
    "INFO1.SPY", "INFO2.SPY", "INFO3.SPY", "SHAPET.SPY", "CODES.SPY",
    /* Font */
    "FONTTI.FON",
    /* Music */
    "HUIPPE.S3M", "OEKU.S3M",
    /* Sound effects */
    "EXPLOS1.VOC", "EXPLOS2.VOC", "EXPLOS3.VOC", "EXPLOS4.VOC",
    "EXPLOS5.VOC", "URETHAN.VOC", "KILI.VOC", "APPLAUSE.VOC",
    "PIKKUPOM.VOC", "AARGH.VOC", "KARJAISU.VOC", "PICAXE.VOC",
    /* Results portraits (PCX data despite the .PPM extension) */
    "SINVOIT.PPM", "SINDRAW.PPM", "SINLOSE.PPM",
    "PUNVOIT.PPM", "PUNDRAW.PPM", "PUNLOSE.PPM",
    "VIHVOIT.PPM", "VIHDRAW.PPM", "VIHLOSE.PPM",
    "KELVOIT.PPM", "KELDRAW.PPM", "KELLOSE.PPM",
    /* Hardcoded multiplayer fallback map */
    "KOMPLEX.MNE",
};

#define REQUIRED_COUNT \
    ((int)(sizeof(REQUIRED_FILES) / sizeof(REQUIRED_FILES[0])))

#define CAMPAIGN_LEVELS 15  /* LEVEL0.MNL .. LEVEL14.MNL */

static bool check_one(const char *assets_dir, const char *name, int *missing)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", assets_dir, name);
    if (FileExists(path)) return true;
    TraceLog(LOG_ERROR, "ASSETS: missing %s", path);
    (*missing)++;
    return false;
}

bool asset_check_startup(const char *assets_dir)
{
    if (!DirectoryExists(assets_dir)) {
        TraceLog(LOG_FATAL,
                 "ASSETS: directory '%s' not found (looked in the working "
                 "directory). The original game data is not distributed "
                 "with this port — copy the Mine Bombers 3.11 files into "
                 "'%s' next to the executable (see README.md), then run "
                 "again.", assets_dir, assets_dir);
        return false;
    }

    int missing = 0;
    for (int i = 0; i < REQUIRED_COUNT; i++) {
        check_one(assets_dir, REQUIRED_FILES[i], &missing);
    }

    char level[32];
    for (int i = 0; i < CAMPAIGN_LEVELS; i++) {
        snprintf(level, sizeof(level), "LEVEL%d.MNL", i);
        check_one(assets_dir, level, &missing);
    }

    if (missing > 0) {
        TraceLog(LOG_FATAL,
                 "ASSETS: %d required game file(s) missing from '%s' "
                 "(listed above). Copy the complete Mine Bombers 3.11 "
                 "data into '%s' (see README.md), then run again.",
                 missing, assets_dir, assets_dir);
        return false;
    }

    return true;
}
