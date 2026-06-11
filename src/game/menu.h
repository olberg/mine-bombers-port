#ifndef MENU_H
#define MENU_H

typedef enum {
    MENU_NONE,
    MENU_START,
    MENU_OPTIONS,
    MENU_INFO,
    MENU_QUIT
} MenuSelection;

/* Load MAIN3.SPY background, extract cursor, prepare menu. */
void menu_init(void);

/* Handle input, advance menu state. Returns selection when confirmed. */
MenuSelection menu_update(void);

/* Draw the menu (background + title text + cursor). */
void menu_draw(void);

/* Unload menu resources. */
void menu_cleanup(void);

#endif
