#include "prng.h"

#include <stdlib.h>
#include <time.h>

static uint32_t g_seed = 1u;
static int g_seed_pinned;   /* MB_SEED / set_seed: randomize() becomes a no-op */

static uint32_t lcg_advance(void)
{
    g_seed = g_seed * 0x08088405u + 1u;
    return g_seed;
}

void mb_prng_init(void)
{
    const char *env = getenv("MB_SEED");
    if (env && *env) {
        g_seed = (uint32_t)strtoul(env, NULL, 0);
        g_seed_pinned = 1;
    } else {
        g_seed = 1u;
    }
}

void mb_prng_set_seed(uint32_t seed)
{
    g_seed = seed;
    g_seed_pinned = 1;
}

void mb_prng_randomize(void)
{
    /* Pascal Randomize (FUN_1030_1a73): reseed from the system clock
     * (DOS INT 21h time → CX:DX). The original calls this before each
     * random map generation (seg_1008:77), so consecutive random maps
     * differ. When a seed is pinned (MB_SEED or set_seed), this is a
     * no-op: the LCG orbit continues, so maps still differ per round
     * while the whole run stays reproducible. */
    if (g_seed_pinned) return;
    g_seed = (uint32_t)time(NULL) * 1000u + (uint32_t)clock();
}

uint32_t mb_prng_get_seed(void)
{
    return g_seed;
}

int mb_random(int n)
{
    if (n <= 0) return 0;
    uint32_t s = lcg_advance();
    uint64_t product = (uint64_t)s * (uint64_t)(uint32_t)n;
    return (int)(product >> 32);
}

int mb_random_range(int lo, int hi)
{
    if (hi < lo) return lo;
    return lo + mb_random(hi - lo + 1);
}

