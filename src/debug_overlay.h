#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include "game/round.h"
#include "game/player.h"
#include <stdbool.h>

/* Parse command-line args for --debug flag. */
void debug_parse_args(int argc, char *argv[]);

/* Set pointer to the current round (NULL when not in gameplay). */
void debug_set_round(const Round *r);

/* Draw the debug overlay at window resolution (1280x960).
 * Call AFTER the 2x upscale blit, inside BeginDrawing/EndDrawing.
 * Only draws if --debug was passed and F3 toggle is active. */
void debug_draw(const Player players[], int num_players);

#endif
