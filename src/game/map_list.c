#include "map_list.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Sorted array of map base names (no extension, no path) */
static char map_names[MAP_LIST_MAX][MAP_NAME_MAX];
static int  map_count = 0;

/* Compare function for qsort — case-insensitive alphabetical */
static int cmp_map_names(const void *a, const void *b)
{
    const char *na = (const char *)a;
    const char *nb = (const char *)b;
    /* Case-insensitive compare, matching original's alphabetical sort */
    for (int i = 0; i < MAP_NAME_MAX; i++) {
        int ca = toupper((unsigned char)na[i]);
        int cb = toupper((unsigned char)nb[i]);
        if (ca != cb) return ca - cb;
        if (ca == 0) break;
    }
    return 0;
}

/* Extract base name from a full path, stripping directory and .MNE extension */
static void extract_basename(char *dst, int dst_size, const char *path)
{
    /* Find last path separator */
    const char *name = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }

    /* Copy up to the last '.' or end of string */
    int i = 0;
    for (; name[i] && name[i] != '.' && i < dst_size - 1; i++) {
        dst[i] = name[i];
    }
    dst[i] = '\0';
}

int map_list_load(const char *assets_dir)
{
    map_count = 0;
    memset(map_names, 0, sizeof(map_names));

    FilePathList files = LoadDirectoryFilesEx(assets_dir, ".MNE", false);

    for (unsigned int i = 0; i < files.count && map_count < MAP_LIST_MAX; i++) {
        char basename[MAP_NAME_MAX];
        extract_basename(basename, sizeof(basename), files.paths[i]);

        if (basename[0] == '\0') continue;

        /* Store the basename */
        strncpy(map_names[map_count], basename, MAP_NAME_MAX - 1);
        map_names[map_count][MAP_NAME_MAX - 1] = '\0';
        map_count++;
    }

    UnloadDirectoryFiles(files);

    /* Sort alphabetically, matching original behavior (bubble sort → same result) */
    if (map_count > 1) {
        qsort(map_names, map_count, MAP_NAME_MAX, cmp_map_names);
    }

    TraceLog(LOG_INFO, "MAP_LIST: Found %d multiplayer maps", map_count);

    return map_count;
}

int map_list_count(void)
{
    return map_count;
}

const char *map_list_name(int index)
{
    if (index < 0 || index >= map_count) return NULL;
    return map_names[index];
}

bool map_list_build_path(char *buf, int buf_size, const char *assets_dir, int index)
{
    if (index < 0 || index >= map_count) return false;
    snprintf(buf, buf_size, "%s/%s.MNE", assets_dir, map_names[index]);
    return true;
}
