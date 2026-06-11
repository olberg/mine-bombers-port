#include "sfx.h"
#include "loaders/voc_loader.h"
#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>

static Sound sounds[SFX_COUNT];
static bool loaded[SFX_COUNT];
static bool sfx_enabled = true;  /* matches original DAT_1038_1bde bit 1 */

static const char *sfx_files[SFX_COUNT] = {
    "assets/EXPLOS1.VOC",
    "assets/EXPLOS2.VOC",
    "assets/EXPLOS3.VOC",
    "assets/EXPLOS4.VOC",
    "assets/EXPLOS5.VOC",
    "assets/URETHAN.VOC",
    "assets/KILI.VOC",
    "assets/APPLAUSE.VOC",
    "assets/PIKKUPOM.VOC",
    "assets/AARGH.VOC",
    "assets/KARJAISU.VOC",
    "assets/PICAXE.VOC",
};

void sfx_init(void)
{
    for (int i = 0; i < SFX_COUNT; i++) {
        sounds[i] = LoadVOC(sfx_files[i]);
        loaded[i] = (sounds[i].frameCount > 0);
        if (!loaded[i]) {
            TraceLog(LOG_WARNING, "SFX: Failed to load %s", sfx_files[i]);
        }
    }
}

void sfx_play(SfxId id)
{
    if (!sfx_enabled) return;
    if (id >= 0 && id < SFX_COUNT && loaded[id]) {
        PlaySound(sounds[id]);
    }
}

void sfx_set_enabled(bool flag)
{
    sfx_enabled = flag;
}

bool sfx_is_enabled(void)
{
    return sfx_enabled;
}

void sfx_shutdown(void)
{
    for (int i = 0; i < SFX_COUNT; i++) {
        if (loaded[i]) {
            UnloadSound(sounds[i]);
            loaded[i] = false;
        }
    }
}
