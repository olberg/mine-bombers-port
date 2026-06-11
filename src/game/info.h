#ifndef INFO_H
#define INFO_H

typedef enum {
    INFO_ACTIVE,
    INFO_DONE
} InfoResult;

/* Initialize info screen. */
void info_init(void);

/* Update info screen. Returns INFO_DONE when finished. */
InfoResult info_update(void);

/* Draw the info screen. */
void info_draw(void);

/* Unload resources. */
void info_cleanup(void);

#endif
