#ifndef KEY_CONFIG_H
#define KEY_CONFIG_H

typedef enum {
    KEY_CONFIG_ACTIVE,
    KEY_CONFIG_DONE
} KeyConfigResult;

void key_config_init(void);
KeyConfigResult key_config_update(void);
void key_config_draw(void);
void key_config_cleanup(void);

#endif
