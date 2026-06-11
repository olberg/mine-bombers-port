#ifndef SFX_H
#define SFX_H

#include <stdbool.h>

typedef enum {
    SFX_EXPLOS1,
    SFX_EXPLOS2,
    SFX_EXPLOS3,
    SFX_EXPLOS4,
    SFX_EXPLOS5,
    SFX_URETHAN,
    SFX_KILI,
    SFX_APPLAUSE,
    SFX_PIKKUPOM,
    SFX_AARGH,
    SFX_KARJAISU,
    SFX_PICAXE,
    SFX_COUNT
} SfxId;

/* Load all 12 VOC sound effects. Call after InitAudioDevice(). */
void sfx_init(void);

/* Play a sound effect by ID. No-op if SFX is disabled. */
void sfx_play(SfxId id);

/* Enable or disable sound effects. When disabled, sfx_play() is a no-op. */
void sfx_set_enabled(bool enabled);

/* Returns true if sound effects are currently enabled. */
bool sfx_is_enabled(void);

/* Unload all sound effects. */
void sfx_shutdown(void);

#endif
