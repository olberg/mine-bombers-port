#ifndef MAP_LIST_H
#define MAP_LIST_H

#include <stdbool.h>

/* Maximum map name length (8 chars + null), matching original 9-byte entries */
#define MAP_NAME_MAX 9

/* Maximum number of maps (matching original limit of 0x1F0 = 496) */
#define MAP_LIST_MAX 496

/* Scan the given directory for .MNE files and build a sorted map list.
 * Returns the number of maps found. */
int map_list_load(const char *assets_dir);

/* Get the number of loaded maps. */
int map_list_count(void);

/* Get map name by index (0-based). Returns NULL if out of range. */
const char *map_list_name(int index);

/* Build a full path for the map at the given index.
 * Writes to buf (up to buf_size). Returns false if index is out of range. */
bool map_list_build_path(char *buf, int buf_size, const char *assets_dir, int index);

#endif
