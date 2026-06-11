#ifndef OPTIONS_H
#define OPTIONS_H

typedef enum {
    OPTIONS_ACTIVE,
    OPTIONS_DONE,
    OPTIONS_KEY_CONFIG,
    OPTIONS_SOUND_CONFIG,
    OPTIONS_MAP_SELECT
} OptionsResult;

/* Load OPTIONS5.SPY background, init cursor. */
void options_init(void);

/* Handle input. Returns OPTIONS_DONE when user exits. */
OptionsResult options_update(void);

/* Draw the options screen. */
void options_draw(void);

/* Unload resources. */
void options_cleanup(void);

#endif
