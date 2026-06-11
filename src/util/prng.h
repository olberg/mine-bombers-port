#ifndef MB_PRNG_H
#define MB_PRNG_H

#include <stdint.h>

/*
 * Borland Pascal 7 compatible LCG. Decompiled from seg_1030_pascal_rtl.c:
 *   FUN_1030_1a3b:  seed = seed * 0x08088405 + 1    (mod 2^32)
 *   FUN_1030_19de:  Random(N) = (uint64_t)(seed * N) >> 32
 *
 * Seeding:
 *   - Reads env var MB_SEED at startup via mb_prng_init(). If unset, defaults
 *     to 1 so runs are deterministic from boot (Pascal's zero-Randomize state
 *     is seed=0; we pick 1 so tests that compare post-init state differ from
 *     the "uninitialized" sentinel).
 *   - mb_prng_set_seed() lets tests pin the seed explicitly.
 */

void     mb_prng_init(void);
void     mb_prng_set_seed(uint32_t seed);
uint32_t mb_prng_get_seed(void);

/* Pascal Randomize (FUN_1030_1a73): reseed from the clock. No-op when the
 * seed is pinned via MB_SEED or mb_prng_set_seed (reproducible runs). */
void     mb_prng_randomize(void);

/* Returns uniform int in [0, n). Returns 0 if n <= 0. */
int      mb_random(int n);

/* Inclusive-range helper: returns value in [lo, hi]. */
int      mb_random_range(int lo, int hi);

#endif
