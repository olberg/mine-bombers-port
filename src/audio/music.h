#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>

/* Initialize audio system (Raylib audio + libxmp context). */
void music_init(void);

/* Load an S3M module file. Returns true on success. */
bool music_load(const char *path);

/* Start playback. */
void music_play(void);

/* Stop playback. */
void music_stop(void);

/* Jump the playing module to a 1-based song order. No-op when not playing
 * or when the order is 0 or beyond the module's order count (original
 * semantics: FUN_1018_0855, seg_1018:418-430). */
void music_jump_to_order(int order);

/* Start (or restart) playback positioned at a 1-based song order. If the
 * order is out of range, playback starts/continues without a jump. */
void music_play_from_order(int order);

/* Feed audio stream with decoded samples. Call each frame. */
void music_update(void);

/* Enable or disable music playback. When disabled, music_play() is a no-op. */
void music_set_enabled(bool enabled);

/* Returns true if music is currently enabled. */
bool music_is_enabled(void);

/* Clean up all audio resources. */
void music_shutdown(void);

#endif
