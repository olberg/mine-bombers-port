#ifndef TITLE_H
#define TITLE_H

typedef enum {
    TITLE_FADE_IN,
    TITLE_WAIT,
    TITLE_FADE_OUT,
    TITLE_DONE
} TitleState;

/* Load TITLEBE.SPY and prepare the title screen. */
void title_init(void);

/* Advance the title screen state machine. Returns current state. */
TitleState title_update(void);

/* Draw the current title screen frame. */
void title_draw(void);

/* Unload title screen resources. */
void title_cleanup(void);

#endif
