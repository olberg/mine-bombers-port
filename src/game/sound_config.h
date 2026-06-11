#ifndef SOUND_CONFIG_H
#define SOUND_CONFIG_H

#include <stdbool.h>

typedef enum {
    SOUND_CONFIG_ACTIVE,
    SOUND_CONFIG_DONE
} SoundConfigResult;

void sound_config_init(void);
SoundConfigResult sound_config_update(void);
void sound_config_draw(void);
void sound_config_cleanup(void);

/* Load sound configuration from file. Applies music/SFX enabled state.
 * Original format: 6 bytes (DAT_1038_1bda..1bdf), byte 4 = features bitmask.
 * Returns true on success, false on failure (defaults applied). */
bool sound_config_load(const char *path);

/* Save sound configuration to file. */
bool sound_config_save(const char *path);

#endif
