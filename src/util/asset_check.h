#ifndef MB_ASSET_CHECK_H
#define MB_ASSET_CHECK_H

#include <stdbool.h>

/*
 * Startup sanity check for the game data directory.
 *
 * The original game assets are not distributed with this repository (see
 * README.md); without this check a missing file surfaces as a crash deep
 * inside a loader or a blank window. Verifies the assets directory and
 * every file the code references by name: backgrounds/sprite sheet (.SPY),
 * font (.FON), music (.S3M), sound effects (.VOC), campaign levels (.MNL),
 * results portraits (.PPM) and the hardcoded fallback map (KOMPLEX.MNE).
 *
 * Deliberately NOT checked — files the game creates or treats as optional:
 *   OPTIONS.CFG, keybinds.dat, SOUNDCFG.DAT, PLAYERS.DAT, IDENTIFY.DAT,
 *   HALLOFFA.DAT (settings/saves, written at runtime) and the multiplayer
 *   .MNE maps other than KOMPLEX.MNE (discovered by directory scan;
 *   the game falls back to random maps).
 *
 * Returns true if everything is present. On failure logs each missing
 * file and a summary reason (console + minebombers.log) and returns false
 * so the caller can exit before opening the window.
 */
bool asset_check_startup(const char *assets_dir);

#endif
