#include "music.h"
#include "raylib.h"
#include <xmp.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_RATE  44100
#define STREAM_BUFFER_FRAMES 4096

static xmp_context xmp_ctx;
static AudioStream stream;
static bool initialized;
static bool loaded;
static bool playing;
static bool enabled = true;  /* matches original DAT_1038_1bde bit 0 */

/* Audio callback — called by Raylib's audio thread to request PCM data */
static void music_callback(void *buffer, unsigned int frames)
{
    if (!playing) {
        memset(buffer, 0, frames * 4); /* 4 bytes per frame (16-bit stereo) */
        return;
    }

    int ret = xmp_play_buffer(xmp_ctx, buffer, frames * 4, 0);
    if (ret != 0) {
        memset(buffer, 0, frames * 4);
    }
}

void music_init(void)
{
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
    }
    xmp_ctx = xmp_create_context();

    SetAudioStreamBufferSizeDefault(STREAM_BUFFER_FRAMES);
    stream = LoadAudioStream(SAMPLE_RATE, 16, 2); /* 16-bit stereo */
    SetAudioStreamCallback(stream, music_callback);

    initialized = true;
    loaded = false;
    playing = false;
}

bool music_load(const char *path)
{
    if (!initialized) return false;

    if (loaded) {
        xmp_release_module(xmp_ctx);
        loaded = false;
    }

    if (xmp_load_module(xmp_ctx, path) != 0) {
        return false;
    }

    loaded = true;
    return true;
}

void music_play(void)
{
    if (!initialized || !loaded || !enabled) return;

    xmp_start_player(xmp_ctx, SAMPLE_RATE, 0);
    playing = true;
    PlayAudioStream(stream);
}

/* Mirrors FUN_1018_0855 (seg_1018:418-430): the jump is ignored unless
 * 0 < order <= module order count. Orders are 1-based (the original player
 * indexes its order list with order-1, seg_1018:610-614). The stream is
 * stopped around the position change — both for thread safety against the
 * audio callback and because the original brackets its jumps with
 * mute/unmute (seg_1000:7121/7127). */
static void apply_order_jump(int order)
{
    struct xmp_module_info mi;
    xmp_get_module_info(xmp_ctx, &mi);
    if (order <= 0 || order > mi.mod->len) return;
    xmp_set_position(xmp_ctx, order - 1);
}

void music_jump_to_order(int order)
{
    if (!initialized || !loaded || !playing) return;

    StopAudioStream(stream);
    apply_order_jump(order);
    PlayAudioStream(stream);
}

void music_play_from_order(int order)
{
    if (!initialized || !loaded || !enabled) return;

    if (!playing) {
        xmp_start_player(xmp_ctx, SAMPLE_RATE, 0);
        playing = true;
    } else {
        StopAudioStream(stream);
    }
    apply_order_jump(order);
    PlayAudioStream(stream);
}

void music_set_enabled(bool flag)
{
    enabled = flag;
    if (!flag) music_stop();
}

bool music_is_enabled(void)
{
    return enabled;
}

void music_stop(void)
{
    if (!initialized) return;

    if (playing) {
        playing = false;
        StopAudioStream(stream);
        xmp_end_player(xmp_ctx);
    }
}

void music_update(void)
{
    /* Nothing needed — callback handles audio filling */
}

void music_shutdown(void)
{
    if (!initialized) return;

    music_stop();

    if (loaded) {
        xmp_release_module(xmp_ctx);
        loaded = false;
    }

    UnloadAudioStream(stream);
    xmp_free_context(xmp_ctx);
    initialized = false;
}
