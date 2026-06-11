#ifndef MAP_PICKER_H
#define MAP_PICKER_H

#include <stdbool.h>

/* Maximum rounds that can have map assignments */
#define MAP_PICKER_MAX_ROUNDS 56

/* Value indicating "use random map" for a round */
#define MAP_PICK_RANDOM 32000

typedef enum {
    MAP_PICKER_NONE,
    MAP_PICKER_DONE
} MapPickerResult;

/* Initialize the map picker screen.
 * Loads LEVSELEC.SPY background and populates the map grid. */
void map_picker_init(void);

/* Update the map picker (handle input).
 * Returns MAP_PICKER_DONE when the user exits. */
MapPickerResult map_picker_update(void);

/* Draw the map picker screen. */
void map_picker_draw(void);

/* Cleanup map picker resources. */
void map_picker_cleanup(void);

/* Get the per-round map selection array (internal representation).
 * Each entry is a 1-BASED grid index (1..N = map_list index + 1), 0
 * (in-session: unassigned or the explicit "Random" cell), or
 * MAP_PICK_RANDOM (32000, after finalize). Mirrors the original's
 * g_high_score_table contents exactly. Array has MAP_PICKER_MAX_ROUNDS
 * entries. */
const int *map_picker_get_selections(void);

/* Reset all round selections to MAP_PICK_RANDOM (default). */
void map_picker_reset(void);

/* --- Session logic (extracted from FUN_1010_e231/dfee for testing) --- */

/* Picker entry: zero all slots and the assigned counter (seg_1010:8563-8566).
 * Reopening the picker DISCARDS previous selections, as in the original. */
void map_picker_session_begin(void);

/* ENTER/SPACE: assign grid cell to the next round slot (seg_1010:8493-8498).
 * grid_index 0 is the "Random" pseudo-map, 1..N are real maps. */
void map_picker_assign_grid(int grid_index, int total_rounds);

/* Random fill key: overwrite ALL slots 0..total_rounds-1 with
 * Random(map_count + 1) — range INCLUDES the Random cell — retrying up to
 * 100 times against earlier slots to avoid duplicates, then mark all
 * rounds assigned (seg_1010:8501-8523). map_count = number of real maps. */
void map_picker_fill_random(int total_rounds, int map_count);

/* Picker exit: convert still-0 slots to MAP_PICK_RANDOM
 * (seg_1010:8601-8608). */
void map_picker_finalize(void);

/* Number of rounds assigned so far this session. */
int map_picker_assigned_count(void);

/* Check if a specific round has a map selected (not random). */
bool map_picker_has_selection(int round_index);

/* Get the map list index for a specific round.
 * Returns -1 if the round uses random maps. */
int map_picker_get_map_index(int round_index);

#endif
