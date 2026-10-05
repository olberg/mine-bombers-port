#ifndef HARNESS_ENV_H
#define HARNESS_ENV_H

#include "raylib.h"
#include <stdlib.h>

/*
 * Run-environment knobs for unattended runs (ctest, autoplay), so a test
 * window need not land on the working display or play sound. Header-only
 * so the unit tests that open their own window can honor them too.
 *
 *   MB_MONITOR=<n>  move the window to raylib monitor n (0 = primary,
 *                   1 = second display, ...)
 *   MB_VOLUME=<n>   master volume in percent, 0-100 (0 = silent)
 */

/* Call right after InitWindow(). */
static inline void harness_env_apply_monitor(void)
{
    const char *v = getenv("MB_MONITOR");
    if (v && v[0]) SetWindowMonitor(atoi(v));
}

/* Call after InitAudioDevice(). */
static inline void harness_env_apply_volume(void)
{
    const char *v = getenv("MB_VOLUME");
    if (!v || !v[0]) return;
    SetMasterVolume((float)atoi(v) / 100.0f);
    TraceLog(LOG_INFO, "HARNESS: master volume %d%%", atoi(v));
}

#endif
