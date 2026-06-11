#ifndef HALL_OF_FAME_H
#define HALL_OF_FAME_H

#include <stdint.h>
#include <stdbool.h>

#define HOF_ENTRIES      10
#define HOF_NAME_LEN     20
#define HOF_ENTRY_SIZE   26   /* 21 + 1 + 2 + 2 */
#define HOF_FILE_SIZE    260  /* 10 * 26 */

#pragma pack(push, 1)
typedef struct {
    uint8_t  name_len;           /* Pascal string length byte */
    char     name[HOF_NAME_LEN]; /* player name (padded with spaces) */
    uint8_t  level;              /* level reached (1-15) */
    int16_t  score_lo;           /* score low word */
    int16_t  score_hi;           /* score high word */
} HofEntry;
#pragma pack(pop)

typedef enum {
    HOF_ACTIVE,
    HOF_DONE
} HofResult;

/* Initialize the Hall of Fame screen.
 * player_name: name of the player who just finished
 * level_reached: which level they got to (1-15)
 * score: their final score (cash/points) */
void hof_init(const char *player_name, int level_reached, int32_t score);

/* Update the Hall of Fame screen (handles input). */
HofResult hof_update(void);

/* Draw the Hall of Fame screen. */
void hof_draw(void);

/* Clean up resources. */
void hof_cleanup(void);

#endif
