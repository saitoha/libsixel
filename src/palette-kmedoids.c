/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * k-medoids palette builder.
 *
 * The implementation keeps medoids anchored to observed samples so every
 * palette center remains an actual source color when final merge is disabled.
 * Four solver frontends are provided and selected through -Q
 * kmedoids:algo=...:
 *   - PAM: full swap search.
 *   - CLARA: repeated PAM runs on subsamples, scored globally.
 *   - CLARANS: randomized neighborhood search.
 *   - BanditPAM: swap pruning with progressive mini-batch scoring.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmedoids.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"


#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define SIXEL_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
# define SIXEL_TLS __thread
#else
# define SIXEL_TLS
#endif

static SIXEL_TLS int sixel_kmedoids_algo_override_enabled = 0;
static SIXEL_TLS sixel_kmedoids_algo_t sixel_kmedoids_algo_override_value
    = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
static SIXEL_TLS int sixel_kmedoids_seed_override_enabled = 0;
static SIXEL_TLS uint32_t sixel_kmedoids_seed_override_value = 1u;
static SIXEL_TLS int sixel_kmedoids_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_iter_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_sample_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_sample_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clara_trials_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_clara_trials_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clara_sample_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_clara_sample_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clarans_local_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_clarans_local_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_clarans_neighbors_override_enabled = 0;
static SIXEL_TLS unsigned int
    sixel_kmedoids_clarans_neighbors_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_bandit_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_bandit_iter_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_bandit_candidates_override_enabled = 0;
static SIXEL_TLS unsigned int
    sixel_kmedoids_bandit_candidates_override_value = 0u;
static SIXEL_TLS int sixel_kmedoids_bandit_batch_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmedoids_bandit_batch_override_value = 0u;
static SIXEL_TLS double const *sixel_kmedoids_distance_cache = NULL;
static SIXEL_TLS unsigned int sixel_kmedoids_distance_cache_size = 0u;

#undef SIXEL_TLS

typedef struct sixel_kmedoids_unique_slot {
    uint64_t key0;
    uint64_t key1;
    uint64_t key2;
    unsigned int index;
    int used;
} sixel_kmedoids_unique_slot_t;

static uint32_t
sixel_kmedoids_rng_next(uint32_t *state);

static int
sixel_palette_kmedoids_log_start(sixel_logger_t *logger,
                                 int *job_seq,
                                 char const *engine_name,
                                 char const *role,
                                 char const *phase)
{
    int job_id;

    job_id = -1;
    if (logger == NULL) {
        return job_id;
    }
    if (job_seq != NULL) {
        job_id = *job_seq;
        *job_seq += 1;
    }
    sixel_logger_logf(logger,
                      (role != NULL && role[0] != '\0') ? role : "palette",
                      "palette/build",
                      "start",
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "engine=%s phase=%s",
                      engine_name != NULL ? engine_name : "(unknown)",
                      phase);
    return job_id;
}

static void
sixel_palette_kmedoids_log_finish(sixel_logger_t *logger,
                                  int job_id,
                                  char const *engine_name,
                                  char const *role,
                                  char const *phase,
                                  char const *detail)
{
    char const *suffix;

    suffix = "";
    if (logger == NULL || job_id < 0) {
        return;
    }
    if (detail != NULL && detail[0] != '\0') {
        suffix = detail;
    }
    sixel_logger_logf(logger,
                      (role != NULL && role[0] != '\0') ? role : "palette",
                      "palette/build",
                      "finish",
                      job_id,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "engine=%s phase=%s%s%s",
                      engine_name != NULL ? engine_name : "(unknown)",
                      phase,
                      suffix[0] != '\0' ? " " : "",
                      suffix);
}

static unsigned int
sixel_kmedoids_clamp_uint(unsigned int value,
                          unsigned int minimum,
                          unsigned int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static unsigned int
sixel_kmedoids_parse_env_uint(char const *env_name,
                              unsigned int fallback,
                              unsigned int minimum,
                              unsigned int maximum,
                              int allow_zero)
{
    char const *env_value;
    char *endptr;
    unsigned long parsed;

    env_value = NULL;
    endptr = NULL;
    parsed = 0ul;
    if (env_name == NULL || env_name[0] == '\0') {
        return fallback;
    }

    env_value = sixel_compat_getenv(env_name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtoul(env_value, &endptr, 10);
    if (endptr == env_value || endptr == NULL || endptr[0] != '\0'
            || errno != 0 || parsed > (unsigned long)UINT_MAX) {
        return fallback;
    }
    if (allow_zero && parsed == 0ul) {
        return 0u;
    }
    if (parsed == 0ul) {
        return fallback;
    }

    return sixel_kmedoids_clamp_uint((unsigned int)parsed,
                                     minimum,
                                     maximum);
}

static void
sixel_kmedoids_set_distance_cache(double const *cache,
                                  unsigned int point_count)
{
    sixel_kmedoids_distance_cache = cache;
    sixel_kmedoids_distance_cache_size = point_count;
}

static double
sixel_kmedoids_rng_unit(uint32_t *state)
{
    uint32_t raw;

    raw = sixel_kmedoids_rng_next(state);
    return ((double)raw) / 4294967295.0;
}

static const char *
sixel_kmedoids_algo_to_string(sixel_kmedoids_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO:
        return "auto";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA:
        return "clara";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS:
        return "clarans";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM:
        return "banditpam";
    case SIXEL_PALETTE_KMEDOIDS_ALGO_PAM:
    default:
        return "pam";
    }
}

static sixel_kmedoids_algo_t
sixel_kmedoids_resolve_algo(sixel_kmedoids_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_PAM:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS:
    case SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM:
        return algo;
    default:
        return SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
    }
}

void
sixel_set_kmedoids_algo_override(int enabled,
                                 sixel_kmedoids_algo_t algo)
{
    sixel_kmedoids_algo_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_algo_override_value = sixel_kmedoids_resolve_algo(algo);
}

SIXEL_INTERNAL_API sixel_kmedoids_algo_t
sixel_get_kmedoids_algo(void)
{
    char const *env_value;
    static int loaded = 0;
    static sixel_kmedoids_algo_t cached = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;

    env_value = NULL;
    if (sixel_kmedoids_algo_override_enabled) {
        return sixel_kmedoids_algo_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEDOIDS_ALGO");
    if (env_value != NULL && env_value[0] != '\0') {
        if (sixel_compat_strcasecmp(env_value, "auto") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO;
        } else if (sixel_compat_strcasecmp(env_value, "pam") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
        } else if (sixel_compat_strcasecmp(env_value, "clara") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA;
        } else if (sixel_compat_strcasecmp(env_value, "clarans") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS;
        } else if (sixel_compat_strcasecmp(env_value, "banditpam") == 0) {
            cached = SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM;
        }
    }

    return cached;
}

void
sixel_set_kmedoids_seed_override(int enabled,
                                 uint32_t seed)
{
    sixel_kmedoids_seed_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_seed_override_value = seed;
}

SIXEL_INTERNAL_API uint32_t
sixel_get_kmedoids_seed(void)
{
    char const *env_value;
    char *endptr;
    unsigned long long parsed;
    static int loaded = 0;
    static uint32_t cached = 1u;

    env_value = NULL;
    endptr = NULL;
    parsed = 0u;
    if (sixel_kmedoids_seed_override_enabled) {
        return sixel_kmedoids_seed_override_value;
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEDOIDS_SEED");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed = strtoull(env_value, &endptr, 10);
        if (endptr != env_value && endptr != NULL && endptr[0] == '\0'
                && errno == 0 && parsed <= 0xffffffffULL) {
            cached = (uint32_t)parsed;
        }
    }

    return cached;
}

void
sixel_set_kmedoids_iter_override(int enabled,
                                 unsigned int iter_count)
{
    sixel_kmedoids_iter_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_iter_override_value = iter_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_iter(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_iter_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_iter_override_value,
            1u,
            64u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint("SIXEL_PALETTE_KMEDOIDS_ITER",
                                           0u,
                                           1u,
                                           64u,
                                           0);
    return cached;
}

void
sixel_set_kmedoids_sample_override(int enabled,
                                   unsigned int sample_count)
{
    sixel_kmedoids_sample_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_sample_override_value = sample_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_sample(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_sample_override_enabled) {
        if (sixel_kmedoids_sample_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_sample_override_value,
            64u,
            1048576u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint("SIXEL_PALETTE_KMEDOIDS_SAMPLE",
                                           0u,
                                           64u,
                                           1048576u,
                                           1);
    return cached;
}

void
sixel_set_kmedoids_clara_trials_override(int enabled,
                                         unsigned int trials)
{
    sixel_kmedoids_clara_trials_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clara_trials_override_value = trials;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clara_trials(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clara_trials_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clara_trials_override_value,
            1u,
            32u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        0u,
        1u,
        32u,
        0);
    return cached;
}

void
sixel_set_kmedoids_clara_sample_override(int enabled,
                                         unsigned int sample_count)
{
    sixel_kmedoids_clara_sample_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clara_sample_override_value = sample_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clara_sample(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clara_sample_override_enabled) {
        if (sixel_kmedoids_clara_sample_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clara_sample_override_value,
            64u,
            1048576u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        0u,
        64u,
        1048576u,
        1);
    return cached;
}

void
sixel_set_kmedoids_clarans_local_override(int enabled,
                                          unsigned int local_searches)
{
    sixel_kmedoids_clarans_local_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clarans_local_override_value = local_searches;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clarans_local(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clarans_local_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clarans_local_override_value,
            1u,
            32u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        0u,
        1u,
        32u,
        0);
    return cached;
}

void
sixel_set_kmedoids_clarans_neighbors_override(int enabled,
                                              unsigned int neighbors)
{
    sixel_kmedoids_clarans_neighbors_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_clarans_neighbors_override_value = neighbors;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_clarans_neighbors(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_clarans_neighbors_override_enabled) {
        if (sixel_kmedoids_clarans_neighbors_override_value == 0u) {
            return 0u;
        }
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_clarans_neighbors_override_value,
            1u,
            5000000u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS",
        0u,
        1u,
        5000000u,
        1);
    return cached;
}

void
sixel_set_kmedoids_bandit_iter_override(int enabled,
                                        unsigned int iter_count)
{
    sixel_kmedoids_bandit_iter_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_bandit_iter_override_value = iter_count;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_iter(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_bandit_iter_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_bandit_iter_override_value,
            1u,
            64u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        0u,
        1u,
        64u,
        0);
    return cached;
}

void
sixel_set_kmedoids_bandit_candidates_override(int enabled,
                                              unsigned int candidates)
{
    sixel_kmedoids_bandit_candidates_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_bandit_candidates_override_value = candidates;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_candidates(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_bandit_candidates_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_bandit_candidates_override_value,
            8u,
            4096u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES",
        0u,
        8u,
        4096u,
        0);
    return cached;
}

void
sixel_set_kmedoids_bandit_batch_override(int enabled,
                                         unsigned int batch_size)
{
    sixel_kmedoids_bandit_batch_override_enabled = enabled ? 1 : 0;
    sixel_kmedoids_bandit_batch_override_value = batch_size;
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kmedoids_bandit_batch(void)
{
    static int loaded = 0;
    static unsigned int cached = 0u;

    if (sixel_kmedoids_bandit_batch_override_enabled) {
        return sixel_kmedoids_clamp_uint(
            sixel_kmedoids_bandit_batch_override_value,
            8u,
            4096u);
    }
    if (loaded) {
        return cached;
    }
    loaded = 1;
    cached = sixel_kmedoids_parse_env_uint(
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        0u,
        8u,
        4096u,
        0);
    return cached;
}

/*
 * Keep this helper name k-medoids specific so amalgamation builds can include
 * palette-kmeans.c in the same translation unit without static symbol
 * collisions.
 */
static int
sixel_kmedoids_float32_alpha_visible(double alpha)
{
    if (!isfinite(alpha)) {
        return 0;
    }
    return alpha > 0.0;
}

static unsigned int
sixel_kmedoids_quality_pam_iterations(int quality_mode)
{
    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        return 4u;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        return 16u;
    case SIXEL_QUALITY_FULL:
        return 24u;
    case SIXEL_QUALITY_AUTO:
    default:
        return 8u;
    }
}

static unsigned int
sixel_kmedoids_quality_clara_trials(int quality_mode)
{
    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        return 3u;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        return 7u;
    case SIXEL_QUALITY_FULL:
        return 9u;
    case SIXEL_QUALITY_AUTO:
    default:
        return 5u;
    }
}

static unsigned int
sixel_kmedoids_quality_bandit_iterations(int quality_mode)
{
    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        return 4u;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        return 12u;
    case SIXEL_QUALITY_FULL:
        return 16u;
    case SIXEL_QUALITY_AUTO:
    default:
        return 8u;
    }
}

static unsigned int
sixel_kmedoids_sample_target(unsigned int reqcolors,
                             unsigned int pixel_count,
                             int quality_mode)
{
    unsigned int floor_target;
    unsigned int color_floor;

    floor_target = 0u;
    color_floor = 0u;
    if (reqcolors == 0u) {
        reqcolors = 1u;
    }

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        floor_target = 768u;
        break;
    case SIXEL_QUALITY_HIGH:
    case SIXEL_QUALITY_HIGHCOLOR:
        floor_target = 1536u;
        break;
    case SIXEL_QUALITY_FULL:
        floor_target = 2048u;
        break;
    case SIXEL_QUALITY_AUTO:
    default:
        floor_target = 1024u;
        break;
    }

    color_floor = reqcolors * 4u;
    if (floor_target < color_floor) {
        floor_target = color_floor;
    }
    if (floor_target < reqcolors) {
        floor_target = reqcolors;
    }
    if (floor_target > pixel_count) {
        floor_target = pixel_count;
    }

    return floor_target;
}

static uint32_t
sixel_kmedoids_rng_next(uint32_t *state)
{
    uint32_t x;

    x = 0u;
    if (state == NULL) {
        return 0u;
    }

    x = *state;
    if (x == 0u) {
        x = 0x6d2b79f5u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (x == 0u) {
        x = 0x9e3779b9u;
    }
    *state = x;
    return x;
}

static unsigned int
sixel_kmedoids_rng_bounded(uint32_t *state,
                           unsigned int upper)
{
    uint32_t r;

    r = 0u;
    if (upper == 0u) {
        return 0u;
    }
    r = sixel_kmedoids_rng_next(state);
    return (unsigned int)(r % upper);
}

static uint64_t
sixel_kmedoids_double_bits(double value)
{
    union {
        double d;
        uint64_t u;
    } conv;

    conv.d = value;
    return conv.u;
}

static uint64_t
sixel_kmedoids_hash_keys(uint64_t key0,
                         uint64_t key1,
                         uint64_t key2)
{
    uint64_t h;

    h = 1469598103934665603ULL;
    h ^= key0;
    h *= 1099511628211ULL;
    h ^= key1;
    h *= 1099511628211ULL;
    h ^= key2;
    h *= 1099511628211ULL;
    return h;
}

static unsigned int
sixel_kmedoids_next_power_of_two(unsigned int value)
{
    unsigned int size;

    size = 1u;
    while (size < value && size < (1u << 30)) {
        size <<= 1u;
    }
    if (size < 8u) {
        size = 8u;
    }
    return size;
}

static SIXELSTATUS
sixel_kmedoids_collect_samples(unsigned char const *data,
                               unsigned int length,
                               unsigned int channels,
                               unsigned int pixel_stride,
                               int input_is_float32,
                               double const *float_scale,
                               double const *float_offset,
                               unsigned int sample_target,
                               uint32_t seed,
                               double **samples_out,
                               unsigned int *sample_count_out,
                               unsigned int *visible_count_out,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    double *samples;
    unsigned int pixel_count;
    unsigned int index;
    unsigned int base;
    unsigned int visible_count;
    unsigned int sample_count;
    unsigned int slot;
    unsigned int channel;
    uint32_t rng_state;
    unsigned int *bin_counts;
    unsigned char *bin_filled;
    double *bin_samples;
    unsigned int bin;
    unsigned int bin0;
    unsigned int bin1;
    unsigned int bin2;
    unsigned int rare_budget;
    unsigned int rare_added;
    unsigned int scan;
    unsigned int best_bin;
    unsigned int best_count;
    double component;
    double mapped;
    double c0;
    double c1;
    double c2;

    status = SIXEL_BAD_ARGUMENT;
    samples = NULL;
    pixel_count = 0u;
    index = 0u;
    base = 0u;
    visible_count = 0u;
    sample_count = 0u;
    slot = 0u;
    channel = 0u;
    rng_state = seed;
    bin_counts = NULL;
    bin_filled = NULL;
    bin_samples = NULL;
    bin = 0u;
    bin0 = 0u;
    bin1 = 0u;
    bin2 = 0u;
    rare_budget = 0u;
    rare_added = 0u;
    scan = 0u;
    best_bin = 0u;
    best_count = 0u;
    component = 0.0;
    mapped = 0.0;
    c0 = 0.0;
    c1 = 0.0;
    c2 = 0.0;

    if (samples_out != NULL) {
        *samples_out = NULL;
    }
    if (sample_count_out != NULL) {
        *sample_count_out = 0u;
    }
    if (visible_count_out != NULL) {
        *visible_count_out = 0u;
    }
    if (data == NULL || allocator == NULL || samples_out == NULL
            || sample_count_out == NULL || visible_count_out == NULL) {
        return status;
    }
    if (channels != 3u && channels != 4u) {
        return status;
    }
    if (pixel_stride == 0u) {
        return status;
    }

    pixel_count = length / pixel_stride;
    if (pixel_count == 0u) {
        return SIXEL_OK;
    }
    if (sample_target == 0u) {
        sample_target = 1u;
    }
    if (sample_target > pixel_count) {
        sample_target = pixel_count;
    }

    samples = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_target * 3u * sizeof(double));
    bin_counts = (unsigned int *)sixel_allocator_malloc(
        allocator,
        4096u * sizeof(unsigned int));
    bin_filled = (unsigned char *)sixel_allocator_malloc(
        allocator,
        4096u * sizeof(unsigned char));
    bin_samples = (double *)sixel_allocator_malloc(
        allocator,
        4096u * 3u * sizeof(double));
    if (samples == NULL || bin_counts == NULL || bin_filled == NULL
            || bin_samples == NULL) {
        if (samples != NULL) {
            sixel_allocator_free(allocator, samples);
        }
        if (bin_counts != NULL) {
            sixel_allocator_free(allocator, bin_counts);
        }
        if (bin_filled != NULL) {
            sixel_allocator_free(allocator, bin_filled);
        }
        if (bin_samples != NULL) {
            sixel_allocator_free(allocator, bin_samples);
        }
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < 4096u; ++index) {
        bin_counts[index] = 0u;
        bin_filled[index] = 0u;
    }

    for (index = 0u; index < pixel_count; ++index) {
        base = index * pixel_stride;
        if (input_is_float32) {
            float const *fpixels;

            fpixels = (float const *)(void const *)(data + base);
            if (channels == 4u
                    && !sixel_kmedoids_float32_alpha_visible(
                           (double)fpixels[3u])) {
                continue;
            }
            c0 = (double)fpixels[0u];
            c1 = (double)fpixels[1u];
            c2 = (double)fpixels[2u];
            if (sample_count < sample_target) {
                samples[sample_count * 3u + 0u] = c0;
                samples[sample_count * 3u + 1u] = c1;
                samples[sample_count * 3u + 2u] = c2;
                ++sample_count;
            } else {
                slot = sixel_kmedoids_rng_bounded(&rng_state,
                                                  visible_count + 1u);
                if (slot < sample_target) {
                    samples[slot * 3u + 0u] = c0;
                    samples[slot * 3u + 1u] = c1;
                    samples[slot * 3u + 2u] = c2;
                }
            }

            for (channel = 0u; channel < 3u; ++channel) {
                if (channel == 0u) {
                    component = c0;
                } else if (channel == 1u) {
                    component = c1;
                } else {
                    component = c2;
                }
                mapped = component;
                if (float_scale != NULL && float_offset != NULL
                        && float_scale[channel] > 0.0) {
                    mapped = component * float_scale[channel]
                           + float_offset[channel];
                }
                if (!isfinite(mapped) || mapped < 0.0) {
                    mapped = 0.0;
                } else if (mapped > 255.0) {
                    mapped = 255.0;
                }
                if (channel == 0u) {
                    bin0 = ((unsigned int)mapped) >> 4u;
                } else if (channel == 1u) {
                    bin1 = ((unsigned int)mapped) >> 4u;
                } else {
                    bin2 = ((unsigned int)mapped) >> 4u;
                }
            }
            if (bin0 > 15u) {
                bin0 = 15u;
            }
            if (bin1 > 15u) {
                bin1 = 15u;
            }
            if (bin2 > 15u) {
                bin2 = 15u;
            }
            bin = (bin0 << 8u) | (bin1 << 4u) | bin2;
            bin_counts[bin] += 1u;
            if (bin_filled[bin] == 0u) {
                bin_filled[bin] = 1u;
                bin_samples[bin * 3u + 0u] = c0;
                bin_samples[bin * 3u + 1u] = c1;
                bin_samples[bin * 3u + 2u] = c2;
            }
            ++visible_count;
        } else {
            if (channels == 4u && data[base + 3u] == 0u) {
                continue;
            }
            c0 = (double)data[base + 0u];
            c1 = (double)data[base + 1u];
            c2 = (double)data[base + 2u];
            if (sample_count < sample_target) {
                samples[sample_count * 3u + 0u] = c0;
                samples[sample_count * 3u + 1u] = c1;
                samples[sample_count * 3u + 2u] = c2;
                ++sample_count;
            } else {
                slot = sixel_kmedoids_rng_bounded(&rng_state,
                                                  visible_count + 1u);
                if (slot < sample_target) {
                    samples[slot * 3u + 0u] = c0;
                    samples[slot * 3u + 1u] = c1;
                    samples[slot * 3u + 2u] = c2;
                }
            }

            bin0 = data[base + 0u] >> 4u;
            bin1 = data[base + 1u] >> 4u;
            bin2 = data[base + 2u] >> 4u;
            bin = (bin0 << 8u) | (bin1 << 4u) | bin2;
            bin_counts[bin] += 1u;
            if (bin_filled[bin] == 0u) {
                bin_filled[bin] = 1u;
                bin_samples[bin * 3u + 0u] = c0;
                bin_samples[bin * 3u + 1u] = c1;
                bin_samples[bin * 3u + 2u] = c2;
            }
            ++visible_count;
        }
    }

    rare_budget = sample_count / 4u;
    if (rare_budget < 8u) {
        rare_budget = 8u;
    }
    if (rare_budget > 128u) {
        rare_budget = 128u;
    }
    if (rare_budget > sample_count) {
        rare_budget = sample_count;
    }
    rare_added = 0u;
    while (rare_added < rare_budget) {
        best_bin = 4096u;
        best_count = UINT_MAX;
        for (scan = 0u; scan < 4096u; ++scan) {
            if (bin_filled[scan] == 0u || bin_counts[scan] == 0u) {
                continue;
            }
            if (bin_counts[scan] < best_count) {
                best_count = bin_counts[scan];
                best_bin = scan;
            }
        }
        if (best_bin >= 4096u) {
            break;
        }
        slot = sample_count - 1u - rare_added;
        samples[slot * 3u + 0u] = bin_samples[best_bin * 3u + 0u];
        samples[slot * 3u + 1u] = bin_samples[best_bin * 3u + 1u];
        samples[slot * 3u + 2u] = bin_samples[best_bin * 3u + 2u];
        bin_filled[best_bin] = 0u;
        ++rare_added;
    }

    sixel_allocator_free(allocator, bin_samples);
    sixel_allocator_free(allocator, bin_filled);
    sixel_allocator_free(allocator, bin_counts);

    *samples_out = samples;
    *sample_count_out = sample_count;
    *visible_count_out = visible_count;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kmedoids_compress_samples(double const *samples,
                                unsigned int sample_count,
                                double **points_out,
                                double **weights_out,
                                unsigned int *point_count_out,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_kmedoids_unique_slot_t *table;
    double *points;
    double *weights;
    unsigned int capacity;
    unsigned int mask;
    unsigned int index;
    unsigned int slot;
    unsigned int probe;
    unsigned int unique_count;
    uint64_t key0;
    uint64_t key1;
    uint64_t key2;
    uint64_t hash;
    unsigned int entry_index;

    status = SIXEL_BAD_ARGUMENT;
    table = NULL;
    points = NULL;
    weights = NULL;
    capacity = 0u;
    mask = 0u;
    index = 0u;
    slot = 0u;
    probe = 0u;
    unique_count = 0u;
    key0 = 0u;
    key1 = 0u;
    key2 = 0u;
    hash = 0u;
    entry_index = 0u;

    if (points_out != NULL) {
        *points_out = NULL;
    }
    if (weights_out != NULL) {
        *weights_out = NULL;
    }
    if (point_count_out != NULL) {
        *point_count_out = 0u;
    }
    if (samples == NULL || points_out == NULL || weights_out == NULL
            || point_count_out == NULL || allocator == NULL) {
        return status;
    }
    if (sample_count == 0u) {
        return SIXEL_OK;
    }

    capacity = sixel_kmedoids_next_power_of_two(sample_count * 2u);
    mask = capacity - 1u;
    table = (sixel_kmedoids_unique_slot_t *)sixel_allocator_malloc(
        allocator,
        (size_t)capacity * sizeof(sixel_kmedoids_unique_slot_t));
    points = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_count * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_count * sizeof(double));
    if (table == NULL || points == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < capacity; ++index) {
        table[index].key0 = 0u;
        table[index].key1 = 0u;
        table[index].key2 = 0u;
        table[index].index = 0u;
        table[index].used = 0;
    }

    for (index = 0u; index < sample_count; ++index) {
        key0 = sixel_kmedoids_double_bits(samples[index * 3u + 0u]);
        key1 = sixel_kmedoids_double_bits(samples[index * 3u + 1u]);
        key2 = sixel_kmedoids_double_bits(samples[index * 3u + 2u]);
        hash = sixel_kmedoids_hash_keys(key0, key1, key2);
        slot = (unsigned int)(hash & (uint64_t)mask);
        for (;;) {
            if (!table[slot].used) {
                table[slot].used = 1;
                table[slot].key0 = key0;
                table[slot].key1 = key1;
                table[slot].key2 = key2;
                table[slot].index = unique_count;
                points[unique_count * 3u + 0u] = samples[index * 3u + 0u];
                points[unique_count * 3u + 1u] = samples[index * 3u + 1u];
                points[unique_count * 3u + 2u] = samples[index * 3u + 2u];
                weights[unique_count] = 1.0;
                ++unique_count;
                break;
            }
            if (table[slot].key0 == key0 && table[slot].key1 == key1
                    && table[slot].key2 == key2) {
                entry_index = table[slot].index;
                weights[entry_index] += 1.0;
                break;
            }
            probe = (slot + 1u) & mask;
            slot = probe;
        }
    }

    *points_out = points;
    *weights_out = weights;
    *point_count_out = unique_count;
    points = NULL;
    weights = NULL;
    status = SIXEL_OK;

end:
    if (table != NULL) {
        sixel_allocator_free(allocator, table);
    }
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    return status;
}

static double
sixel_kmedoids_distance_sq(double const *points,
                           unsigned int lhs,
                           unsigned int rhs)
{
    size_t cache_index;
    double diff0;
    double diff1;
    double diff2;

    cache_index = 0u;
    if (sixel_kmedoids_distance_cache != NULL
            && sixel_kmedoids_distance_cache_size > 0u
            && lhs < sixel_kmedoids_distance_cache_size
            && rhs < sixel_kmedoids_distance_cache_size) {
        cache_index = (size_t)lhs * (size_t)sixel_kmedoids_distance_cache_size
                    + (size_t)rhs;
        return sixel_kmedoids_distance_cache[cache_index];
    }

    diff0 = points[lhs * 3u + 0u] - points[rhs * 3u + 0u];
    diff1 = points[lhs * 3u + 1u] - points[rhs * 3u + 1u];
    diff2 = points[lhs * 3u + 2u] - points[rhs * 3u + 2u];
    return diff0 * diff0 + diff1 * diff1 + diff2 * diff2;
}

static void
sixel_kmedoids_mark_medoids(unsigned char *flags,
                            unsigned int const *medoids,
                            unsigned int k,
                            unsigned int point_count)
{
    unsigned int index;

    index = 0u;
    if (flags == NULL || medoids == NULL) {
        return;
    }
    for (index = 0u; index < point_count; ++index) {
        flags[index] = 0u;
    }
    for (index = 0u; index < k; ++index) {
        if (medoids[index] < point_count) {
            flags[medoids[index]] = 1u;
        }
    }
}

static void
sixel_kmedoids_choose_initial_deterministic(double const *points,
                                            double const *weights,
                                            unsigned int point_count,
                                            unsigned int k,
                                            unsigned int *medoids,
                                            unsigned char *flags)
{
    unsigned int selected;
    unsigned int index;
    unsigned int seed_index;
    unsigned int candidate;
    double seed_weight;
    double best_score;
    double nearest;
    double distance;
    double weight;
    unsigned int slot;

    selected = 0u;
    index = 0u;
    seed_index = 0u;
    candidate = 0u;
    seed_weight = -1.0;
    best_score = -1.0;
    nearest = 0.0;
    distance = 0.0;
    weight = 1.0;
    slot = 0u;

    if (points == NULL || medoids == NULL || flags == NULL || point_count == 0u
            || k == 0u) {
        return;
    }

    for (index = 0u; index < point_count; ++index) {
        flags[index] = 0u;
        weight = 1.0;
        if (weights != NULL) {
            weight = weights[index];
        }
        if (weight > seed_weight) {
            seed_weight = weight;
            seed_index = index;
        }
    }

    medoids[0u] = seed_index;
    flags[seed_index] = 1u;
    selected = 1u;

    while (selected < k) {
        candidate = 0u;
        best_score = -1.0;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index]) {
                continue;
            }
            nearest = 0.0;
            for (slot = 0u; slot < selected; ++slot) {
                distance = sixel_kmedoids_distance_sq(points,
                                                      index,
                                                      medoids[slot]);
                if (slot == 0u || distance < nearest) {
                    nearest = distance;
                }
            }
            weight = 1.0;
            if (weights != NULL) {
                weight = weights[index];
            }
            distance = nearest * weight;
            if (distance > best_score) {
                best_score = distance;
                candidate = index;
            }
        }
        medoids[selected] = candidate;
        flags[candidate] = 1u;
        ++selected;
    }
}

static SIXELSTATUS
sixel_kmedoids_choose_initial_kpp(double const *points,
                                  double const *weights,
                                  unsigned int point_count,
                                  unsigned int k,
                                  unsigned int *medoids,
                                  unsigned char *flags,
                                  double *nearest_dist,
                                  uint32_t *rng_state)
{
    SIXELSTATUS status;
    unsigned int index;
    unsigned int slot;
    unsigned int candidate;
    unsigned int fallback;
    double total;
    double pick;
    double distance;
    double weight;
    double running;

    status = SIXEL_BAD_ARGUMENT;
    index = 0u;
    slot = 0u;
    candidate = 0u;
    fallback = 0u;
    total = 0.0;
    pick = 0.0;
    distance = 0.0;
    weight = 1.0;
    running = 0.0;

    if (points == NULL || medoids == NULL || flags == NULL
            || nearest_dist == NULL || rng_state == NULL
            || point_count == 0u || k == 0u) {
        return status;
    }

    for (index = 0u; index < point_count; ++index) {
        flags[index] = 0u;
        weight = 1.0;
        if (weights != NULL && isfinite(weights[index])) {
            weight = weights[index];
        }
        if (weight < 0.0) {
            weight = 0.0;
        }
        total += weight;
    }

    candidate = sixel_kmedoids_rng_bounded(rng_state, point_count);
    if (total > 0.0) {
        pick = sixel_kmedoids_rng_unit(rng_state) * total;
        running = 0.0;
        for (index = 0u; index < point_count; ++index) {
            weight = 1.0;
            if (weights != NULL && isfinite(weights[index])) {
                weight = weights[index];
            }
            if (weight < 0.0) {
                weight = 0.0;
            }
            running += weight;
            if (running >= pick) {
                candidate = index;
                break;
            }
        }
    }

    medoids[0u] = candidate;
    flags[candidate] = 1u;

    for (index = 0u; index < point_count; ++index) {
        nearest_dist[index] = sixel_kmedoids_distance_sq(points,
                                                         index,
                                                         candidate);
    }

    for (slot = 1u; slot < k; ++slot) {
        total = 0.0;
        fallback = point_count;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] != 0u) {
                continue;
            }
            if (fallback >= point_count) {
                fallback = index;
            }
            weight = 1.0;
            if (weights != NULL && isfinite(weights[index])) {
                weight = weights[index];
            }
            if (weight < 0.0) {
                weight = 0.0;
            }
            total += nearest_dist[index] * weight;
        }
        if (fallback >= point_count) {
            break;
        }

        candidate = fallback;
        if (total > 0.0) {
            pick = sixel_kmedoids_rng_unit(rng_state) * total;
            running = 0.0;
            for (index = 0u; index < point_count; ++index) {
                if (flags[index] != 0u) {
                    continue;
                }
                weight = 1.0;
                if (weights != NULL && isfinite(weights[index])) {
                    weight = weights[index];
                }
                if (weight < 0.0) {
                    weight = 0.0;
                }
                running += nearest_dist[index] * weight;
                if (running >= pick) {
                    candidate = index;
                    break;
                }
            }
        }

        medoids[slot] = candidate;
        flags[candidate] = 1u;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] != 0u) {
                continue;
            }
            distance = sixel_kmedoids_distance_sq(points, index, candidate);
            if (distance < nearest_dist[index]) {
                nearest_dist[index] = distance;
            }
        }
    }

    return SIXEL_OK;
}

static void
sixel_kmedoids_assign_points(double const *points,
                             double const *weights,
                             unsigned int point_count,
                             unsigned int const *medoids,
                             unsigned int k,
                             unsigned int *nearest_slot,
                             double *nearest_dist,
                             double *second_dist,
                             double *cluster_weights,
                             double *cluster_sums,
                             double *cost_out)
{
    unsigned int point_index;
    unsigned int slot;
    unsigned int best_slot;
    double best_distance;
    double next_distance;
    double distance;
    double weight;
    double cost;
    unsigned int channel;
    size_t offset;

    point_index = 0u;
    slot = 0u;
    best_slot = 0u;
    best_distance = 0.0;
    next_distance = 0.0;
    distance = 0.0;
    weight = 1.0;
    cost = 0.0;
    channel = 0u;
    offset = 0u;

    if (points == NULL || medoids == NULL || point_count == 0u || k == 0u) {
        if (cost_out != NULL) {
            *cost_out = 0.0;
        }
        return;
    }

    if (cluster_weights != NULL) {
        for (slot = 0u; slot < k; ++slot) {
            cluster_weights[slot] = 0.0;
        }
    }
    if (cluster_sums != NULL) {
        for (slot = 0u; slot < k * 3u; ++slot) {
            cluster_sums[slot] = 0.0;
        }
    }

    for (point_index = 0u; point_index < point_count; ++point_index) {
        best_slot = 0u;
        best_distance = sixel_kmedoids_distance_sq(points,
                                                   point_index,
                                                   medoids[0u]);
        next_distance = best_distance;
        for (slot = 1u; slot < k; ++slot) {
            distance = sixel_kmedoids_distance_sq(points,
                                                  point_index,
                                                  medoids[slot]);
            if (distance < best_distance) {
                next_distance = best_distance;
                best_distance = distance;
                best_slot = slot;
            } else if (slot == 1u || distance < next_distance) {
                next_distance = distance;
            }
        }
        if (k == 1u) {
            next_distance = best_distance;
        }
        if (nearest_slot != NULL) {
            nearest_slot[point_index] = best_slot;
        }
        if (nearest_dist != NULL) {
            nearest_dist[point_index] = best_distance;
        }
        if (second_dist != NULL) {
            second_dist[point_index] = next_distance;
        }
        weight = 1.0;
        if (weights != NULL) {
            weight = weights[point_index];
        }
        cost += weight * best_distance;
        if (cluster_weights != NULL) {
            cluster_weights[best_slot] += weight;
        }
        if (cluster_sums != NULL) {
            offset = (size_t)best_slot * 3u;
            for (channel = 0u; channel < 3u; ++channel) {
                cluster_sums[offset + channel]
                    += points[point_index * 3u + channel] * weight;
            }
        }
    }

    if (cost_out != NULL) {
        *cost_out = cost;
    }
}

static double
sixel_kmedoids_swap_cost(double const *points,
                         double const *weights,
                         unsigned int point_count,
                         unsigned int const *nearest_slot,
                         double const *nearest_dist,
                         double const *second_dist,
                         unsigned int replace_slot,
                         unsigned int candidate_point)
{
    unsigned int index;
    double distance;
    double chosen;
    double weight;
    double cost;

    index = 0u;
    distance = 0.0;
    chosen = 0.0;
    weight = 1.0;
    cost = 0.0;

    for (index = 0u; index < point_count; ++index) {
        distance = sixel_kmedoids_distance_sq(points, index, candidate_point);
        if (nearest_slot[index] == replace_slot) {
            chosen = second_dist[index];
            if (distance < chosen) {
                chosen = distance;
            }
        } else {
            chosen = nearest_dist[index];
            if (distance < chosen) {
                chosen = distance;
            }
        }
        weight = 1.0;
        if (weights != NULL) {
            weight = weights[index];
        }
        cost += chosen * weight;
    }

    return cost;
}

static SIXELSTATUS
sixel_kmedoids_pick_sample_indices(unsigned int point_count,
                                   unsigned int sample_size,
                                   uint32_t *rng_state,
                                   unsigned int **indices_out,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *indices;
    unsigned int *perm;
    unsigned int index;
    unsigned int slot;
    unsigned int pick;
    unsigned int temp;

    status = SIXEL_BAD_ARGUMENT;
    indices = NULL;
    perm = NULL;
    index = 0u;
    slot = 0u;
    pick = 0u;
    temp = 0u;

    if (indices_out != NULL) {
        *indices_out = NULL;
    }
    if (indices_out == NULL || allocator == NULL || point_count == 0u
            || sample_size == 0u || sample_size > point_count) {
        return status;
    }

    indices = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(unsigned int));
    if (indices == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    if (sample_size == point_count) {
        for (index = 0u; index < sample_size; ++index) {
            indices[index] = index;
        }
        *indices_out = indices;
        return SIXEL_OK;
    }

    perm = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    if (perm == NULL) {
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0u; index < point_count; ++index) {
        perm[index] = index;
    }

    for (slot = 0u; slot < sample_size; ++slot) {
        pick = slot + sixel_kmedoids_rng_bounded(rng_state,
                                                 point_count - slot);
        temp = perm[slot];
        perm[slot] = perm[pick];
        perm[pick] = temp;
        indices[slot] = perm[slot];
    }

    sixel_allocator_free(allocator, perm);
    *indices_out = indices;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_kmedoids_run_pam(double const *points,
                       double const *weights,
                       unsigned int point_count,
                       unsigned int k,
                       unsigned int max_iterations,
                       unsigned int const *initial_medoids,
                       unsigned int *medoids_out,
                       double *cost_out,
                       unsigned int *iterations_out,
                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *medoids;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned char *flags;
    unsigned int iteration;
    unsigned int slot;
    unsigned int candidate;
    unsigned int best_slot;
    unsigned int best_candidate;
    unsigned int old_medoid;
    double best_cost;
    double best_gain;
    double current_cost;
    double swap_cost;

    status = SIXEL_BAD_ARGUMENT;
    medoids = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    flags = NULL;
    iteration = 0u;
    slot = 0u;
    candidate = 0u;
    best_slot = 0u;
    best_candidate = 0u;
    old_medoid = 0u;
    best_cost = 0.0;
    best_gain = 0.0;
    current_cost = 0.0;
    swap_cost = 0.0;

    if (cost_out != NULL) {
        *cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || point_count == 0u || k == 0u
            || medoids_out == NULL || allocator == NULL) {
        return status;
    }

    medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    if (medoids == NULL || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL || flags == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (initial_medoids != NULL) {
        for (slot = 0u; slot < k; ++slot) {
            medoids[slot] = initial_medoids[slot];
        }
        sixel_kmedoids_mark_medoids(flags, medoids, k, point_count);
    } else {
        sixel_kmedoids_choose_initial_deterministic(points,
                                                    weights,
                                                    point_count,
                                                    k,
                                                    medoids,
                                                    flags);
    }

    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 medoids,
                                 k,
                                 nearest_slot,
                                 nearest_dist,
                                 second_dist,
                                 NULL,
                                 NULL,
                                 &current_cost);

    for (iteration = 0u; iteration < max_iterations; ++iteration) {
        best_gain = 0.0;
        best_cost = current_cost;
        best_slot = 0u;
        best_candidate = medoids[0u];

        for (slot = 0u; slot < k; ++slot) {
            old_medoid = medoids[slot];
            flags[old_medoid] = 0u;
            for (candidate = 0u; candidate < point_count; ++candidate) {
                if (flags[candidate] != 0u || candidate == old_medoid) {
                    continue;
                }
                swap_cost = sixel_kmedoids_swap_cost(points,
                                                     weights,
                                                     point_count,
                                                     nearest_slot,
                                                     nearest_dist,
                                                     second_dist,
                                                     slot,
                                                     candidate);
                if (swap_cost < best_cost) {
                    best_cost = swap_cost;
                    best_gain = current_cost - swap_cost;
                    best_slot = slot;
                    best_candidate = candidate;
                }
            }
            flags[old_medoid] = 1u;
        }

        if (best_gain <= 1.0e-12) {
            break;
        }

        flags[medoids[best_slot]] = 0u;
        medoids[best_slot] = best_candidate;
        flags[best_candidate] = 1u;
        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     medoids,
                                     k,
                                     nearest_slot,
                                     nearest_dist,
                                     second_dist,
                                     NULL,
                                     NULL,
                                     &current_cost);
    }

    for (slot = 0u; slot < k; ++slot) {
        medoids_out[slot] = medoids[slot];
    }
    if (cost_out != NULL) {
        *cost_out = current_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iteration;
    }
    status = SIXEL_OK;

end:
    if (flags != NULL) {
        sixel_allocator_free(allocator, flags);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (medoids != NULL) {
        sixel_allocator_free(allocator, medoids);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_run_clara(double const *points,
                         double const *weights,
                         unsigned int point_count,
                         unsigned int k,
                         unsigned int pam_iterations,
                         unsigned int trials,
                         unsigned int sample_size_override,
                         uint32_t *rng_state,
                         unsigned int *medoids_out,
                         double *cost_out,
                         unsigned int *iterations_out,
                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int sample_size;
    unsigned int trial;
    unsigned int *sample_indices;
    double *sample_points;
    double *sample_weights;
    unsigned int *sample_medoids;
    unsigned char *sample_flags;
    double *sample_nearest;
    unsigned int *candidate_medoids;
    unsigned int sample_index;
    unsigned int slot;
    double sample_cost;
    double candidate_cost;
    double best_cost;
    unsigned int iter_count;
    unsigned int iter_total;

    status = SIXEL_BAD_ARGUMENT;
    sample_size = 0u;
    trial = 0u;
    sample_indices = NULL;
    sample_points = NULL;
    sample_weights = NULL;
    sample_medoids = NULL;
    sample_flags = NULL;
    sample_nearest = NULL;
    candidate_medoids = NULL;
    sample_index = 0u;
    slot = 0u;
    sample_cost = 0.0;
    candidate_cost = 0.0;
    best_cost = 0.0;
    iter_count = 0u;
    iter_total = 0u;

    if (cost_out != NULL) {
        *cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || point_count == 0u || k == 0u || medoids_out == NULL
            || allocator == NULL || rng_state == NULL) {
        return status;
    }

    if (sample_size_override > 0u) {
        sample_size = sample_size_override;
        if (sample_size < k) {
            sample_size = k;
        }
    } else {
        sample_size = 40u * k;
        if (sample_size < 1024u) {
            sample_size = 1024u;
        }
    }
    if (sample_size > point_count) {
        sample_size = point_count;
    }

    sample_points = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * 3u * sizeof(double));
    sample_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(double));
    sample_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    sample_flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(unsigned char));
    sample_nearest = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)sample_size * sizeof(double));
    candidate_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    if (sample_points == NULL || sample_weights == NULL
            || sample_medoids == NULL || sample_flags == NULL
            || sample_nearest == NULL || candidate_medoids == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (trials == 0u) {
        trials = 1u;
    }

    for (trial = 0u; trial < trials; ++trial) {
        status = sixel_kmedoids_pick_sample_indices(point_count,
                                                    sample_size,
                                                    rng_state,
                                                    &sample_indices,
                                                    allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        for (sample_index = 0u; sample_index < sample_size; ++sample_index) {
            slot = sample_indices[sample_index];
            sample_points[sample_index * 3u + 0u] = points[slot * 3u + 0u];
            sample_points[sample_index * 3u + 1u] = points[slot * 3u + 1u];
            sample_points[sample_index * 3u + 2u] = points[slot * 3u + 2u];
            sample_weights[sample_index] = weights[slot];
        }

        status = sixel_kmedoids_choose_initial_kpp(sample_points,
                                                   sample_weights,
                                                   sample_size,
                                                   k,
                                                   sample_medoids,
                                                   sample_flags,
                                                   sample_nearest,
                                                   rng_state);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = sixel_kmedoids_run_pam(sample_points,
                                        sample_weights,
                                        sample_size,
                                        k,
                                        pam_iterations,
                                        sample_medoids,
                                        sample_medoids,
                                        &sample_cost,
                                        &iter_count,
                                        allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        iter_total += iter_count;

        for (slot = 0u; slot < k; ++slot) {
            candidate_medoids[slot] = sample_indices[sample_medoids[slot]];
        }

        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     candidate_medoids,
                                     k,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &candidate_cost);
        if (trial == 0u || candidate_cost < best_cost) {
            best_cost = candidate_cost;
            for (slot = 0u; slot < k; ++slot) {
                medoids_out[slot] = candidate_medoids[slot];
            }
        }

        if (sample_indices != NULL) {
            sixel_allocator_free(allocator, sample_indices);
            sample_indices = NULL;
        }

        if (sample_size == point_count) {
            break;
        }
    }

    if (cost_out != NULL) {
        *cost_out = best_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iter_total;
    }
    status = SIXEL_OK;

end:
    if (sample_nearest != NULL) {
        sixel_allocator_free(allocator, sample_nearest);
    }
    if (sample_flags != NULL) {
        sixel_allocator_free(allocator, sample_flags);
    }
    if (sample_indices != NULL) {
        sixel_allocator_free(allocator, sample_indices);
    }
    if (candidate_medoids != NULL) {
        sixel_allocator_free(allocator, candidate_medoids);
    }
    if (sample_medoids != NULL) {
        sixel_allocator_free(allocator, sample_medoids);
    }
    if (sample_weights != NULL) {
        sixel_allocator_free(allocator, sample_weights);
    }
    if (sample_points != NULL) {
        sixel_allocator_free(allocator, sample_points);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_run_clarans(double const *points,
                           double const *weights,
                           unsigned int point_count,
                           unsigned int k,
                           unsigned int local_searches,
                           unsigned int neighbors,
                           uint32_t *rng_state,
                           unsigned int const *base_medoids,
                           unsigned int *medoids_out,
                           double *cost_out,
                           unsigned int *iterations_out,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *current_medoids;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned char *flags;
    unsigned int local_index;
    unsigned int neighbor_count;
    unsigned int slot;
    unsigned int candidate;
    unsigned int attempts;
    double current_cost;
    double best_cost;
    double swap_cost;
    unsigned int iter_total;

    status = SIXEL_BAD_ARGUMENT;
    current_medoids = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    flags = NULL;
    local_index = 0u;
    neighbor_count = 0u;
    slot = 0u;
    candidate = 0u;
    attempts = 0u;
    current_cost = 0.0;
    best_cost = 0.0;
    swap_cost = 0.0;
    iter_total = 0u;

    if (cost_out != NULL) {
        *cost_out = 0.0;
    }
    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || point_count == 0u || k == 0u || rng_state == NULL
            || medoids_out == NULL || allocator == NULL) {
        return status;
    }

    current_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    if (current_medoids == NULL || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL || flags == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (local_searches == 0u) {
        local_searches = 1u;
    }
    if (neighbors == 0u) {
        neighbors = 1u;
    }

    for (local_index = 0u; local_index < local_searches; ++local_index) {
        if (local_index == 0u && base_medoids != NULL) {
            for (slot = 0u; slot < k; ++slot) {
                current_medoids[slot] = base_medoids[slot];
            }
            sixel_kmedoids_mark_medoids(flags,
                                        current_medoids,
                                        k,
                                        point_count);
        } else {
            status = sixel_kmedoids_choose_initial_kpp(points,
                                                       weights,
                                                       point_count,
                                                       k,
                                                       current_medoids,
                                                       flags,
                                                       nearest_dist,
                                                       rng_state);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     current_medoids,
                                     k,
                                     nearest_slot,
                                     nearest_dist,
                                     second_dist,
                                     NULL,
                                     NULL,
                                     &current_cost);

        neighbor_count = 0u;
        while (neighbor_count < neighbors) {
            slot = sixel_kmedoids_rng_bounded(rng_state, k);
            candidate = 0u;
            attempts = 0u;
            do {
                candidate = sixel_kmedoids_rng_bounded(rng_state,
                                                       point_count);
                ++attempts;
            } while (flags[candidate] != 0u && attempts < point_count * 2u);
            if (flags[candidate] != 0u) {
                break;
            }

            swap_cost = sixel_kmedoids_swap_cost(points,
                                                 weights,
                                                 point_count,
                                                 nearest_slot,
                                                 nearest_dist,
                                                 second_dist,
                                                 slot,
                                                 candidate);
            ++iter_total;
            if (swap_cost + 1.0e-12 < current_cost) {
                flags[current_medoids[slot]] = 0u;
                current_medoids[slot] = candidate;
                flags[candidate] = 1u;
                sixel_kmedoids_assign_points(points,
                                             weights,
                                             point_count,
                                             current_medoids,
                                             k,
                                             nearest_slot,
                                             nearest_dist,
                                             second_dist,
                                             NULL,
                                             NULL,
                                             &current_cost);
                neighbor_count = 0u;
            } else {
                ++neighbor_count;
            }
        }

        if (local_index == 0u || current_cost < best_cost) {
            best_cost = current_cost;
            for (slot = 0u; slot < k; ++slot) {
                medoids_out[slot] = current_medoids[slot];
            }
        }
    }

    if (cost_out != NULL) {
        *cost_out = best_cost;
    }
    if (iterations_out != NULL) {
        *iterations_out = iter_total;
    }
    status = SIXEL_OK;

end:
    if (flags != NULL) {
        sixel_allocator_free(allocator, flags);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (current_medoids != NULL) {
        sixel_allocator_free(allocator, current_medoids);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_bandit_prune_candidates(double const *points,
                                       double const *weights,
                                       unsigned int point_count,
                                       unsigned int const *nearest_slot,
                                       double const *nearest_dist,
                                       double const *second_dist,
                                       unsigned int *candidate_slots,
                                       unsigned int *candidate_points,
                                       unsigned int *active,
                                       unsigned int *active_count,
                                       unsigned int batch_limit,
                                       uint32_t *rng_state,
                                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *sample_points;
    double *candidate_costs;
    unsigned int batch_size;
    unsigned int index;
    unsigned int keep;
    unsigned int i;
    unsigned int j;
    unsigned int temp_index;
    double temp_cost;
    unsigned int point_index;
    unsigned int slot;
    unsigned int candidate;
    double distance;
    double chosen;
    double weight;
    double cost;

    status = SIXEL_BAD_ARGUMENT;
    sample_points = NULL;
    candidate_costs = NULL;
    batch_size = 0u;
    index = 0u;
    keep = 0u;
    i = 0u;
    j = 0u;
    temp_index = 0u;
    temp_cost = 0.0;
    point_index = 0u;
    slot = 0u;
    candidate = 0u;
    distance = 0.0;
    chosen = 0.0;
    weight = 1.0;
    cost = 0.0;

    if (points == NULL || weights == NULL || nearest_slot == NULL
            || nearest_dist == NULL || second_dist == NULL
            || candidate_slots == NULL || candidate_points == NULL
            || active == NULL || active_count == NULL
            || rng_state == NULL || allocator == NULL) {
        return status;
    }

    if (*active_count <= 4u || point_count == 0u) {
        return SIXEL_OK;
    }
    if (batch_limit == 0u) {
        batch_limit = 64u;
    }
    if (batch_limit < 8u) {
        batch_limit = 8u;
    }

    sample_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    candidate_costs = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)(*active_count) * sizeof(double));
    if (sample_points == NULL || candidate_costs == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    batch_size = point_count;
    if (batch_size > batch_limit) {
        batch_size = batch_limit;
    }

    while (*active_count > 4u && batch_size <= point_count) {
        for (index = 0u; index < batch_size; ++index) {
            sample_points[index] = sixel_kmedoids_rng_bounded(rng_state,
                                                              point_count);
        }

        for (index = 0u; index < *active_count; ++index) {
            slot = candidate_slots[active[index]];
            candidate = candidate_points[active[index]];
            cost = 0.0;
            for (i = 0u; i < batch_size; ++i) {
                point_index = sample_points[i];
                distance = sixel_kmedoids_distance_sq(points,
                                                      point_index,
                                                      candidate);
                if (nearest_slot[point_index] == slot) {
                    chosen = second_dist[point_index];
                    if (distance < chosen) {
                        chosen = distance;
                    }
                } else {
                    chosen = nearest_dist[point_index];
                    if (distance < chosen) {
                        chosen = distance;
                    }
                }
                weight = weights[point_index];
                cost += chosen * weight;
            }
            candidate_costs[index] = cost;
        }

        for (i = 0u; i + 1u < *active_count; ++i) {
            for (j = i + 1u; j < *active_count; ++j) {
                if (candidate_costs[j] < candidate_costs[i]) {
                    temp_cost = candidate_costs[i];
                    candidate_costs[i] = candidate_costs[j];
                    candidate_costs[j] = temp_cost;
                    temp_index = active[i];
                    active[i] = active[j];
                    active[j] = temp_index;
                }
            }
        }

        keep = *active_count / 2u;
        if (keep < 4u) {
            keep = 4u;
        }
        *active_count = keep;

        if (batch_size == point_count) {
            break;
        }
        if (batch_size > point_count / 2u) {
            batch_size = point_count;
        } else {
            batch_size *= 2u;
        }
    }

    status = SIXEL_OK;

end:
    if (candidate_costs != NULL) {
        sixel_allocator_free(allocator, candidate_costs);
    }
    if (sample_points != NULL) {
        sixel_allocator_free(allocator, sample_points);
    }
    return status;
}

static SIXELSTATUS
sixel_kmedoids_run_banditpam(double const *points,
                             double const *weights,
                             unsigned int point_count,
                             unsigned int k,
                             unsigned int max_iterations,
                             unsigned int candidate_budget,
                             unsigned int batch_limit,
                             uint32_t *rng_state,
                             unsigned int *medoids_io,
                             double *cost_io,
                             unsigned int *iterations_out,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned char *flags;
    unsigned int *non_medoids;
    unsigned int *candidate_slots;
    unsigned int *candidate_points;
    unsigned int *active;
    unsigned int iteration;
    unsigned int slot;
    unsigned int index;
    unsigned int non_count;
    unsigned int candidate_cap;
    unsigned int candidate_count;
    unsigned int active_count;
    unsigned int pick;
    unsigned int candidate;
    unsigned int best_slot;
    unsigned int best_candidate;
    unsigned int attempts;
    unsigned int probe;
    int duplicate_pair;
    uint64_t candidate_total;
    double current_cost;
    double swap_cost;
    double best_cost;

    status = SIXEL_BAD_ARGUMENT;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    flags = NULL;
    non_medoids = NULL;
    candidate_slots = NULL;
    candidate_points = NULL;
    active = NULL;
    iteration = 0u;
    slot = 0u;
    index = 0u;
    non_count = 0u;
    candidate_cap = 0u;
    candidate_count = 0u;
    active_count = 0u;
    pick = 0u;
    candidate = 0u;
    best_slot = 0u;
    best_candidate = 0u;
    attempts = 0u;
    probe = 0u;
    duplicate_pair = 0;
    candidate_total = 0u;
    current_cost = 0.0;
    swap_cost = 0.0;
    best_cost = 0.0;

    if (iterations_out != NULL) {
        *iterations_out = 0u;
    }
    if (points == NULL || weights == NULL || point_count == 0u || k == 0u
            || medoids_io == NULL || cost_io == NULL || rng_state == NULL
            || allocator == NULL) {
        return status;
    }

    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    flags = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned char));
    non_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    candidate_cap = candidate_budget;
    if (candidate_cap == 0u) {
        candidate_cap = k * 64u;
        if (candidate_cap < k) {
            candidate_cap = k;
        }
    }
    if (candidate_cap < 8u) {
        candidate_cap = 8u;
    }
    if (candidate_cap > 4096u) {
        candidate_cap = 4096u;
    }
    candidate_slots = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)candidate_cap * sizeof(unsigned int));
    candidate_points = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)candidate_cap * sizeof(unsigned int));
    active = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)candidate_cap * sizeof(unsigned int));
    if (nearest_slot == NULL || nearest_dist == NULL || second_dist == NULL
            || flags == NULL || non_medoids == NULL
            || candidate_slots == NULL || candidate_points == NULL
            || active == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    current_cost = *cost_io;
    for (iteration = 0u; iteration < max_iterations; ++iteration) {
        sixel_kmedoids_mark_medoids(flags, medoids_io, k, point_count);
        sixel_kmedoids_assign_points(points,
                                     weights,
                                     point_count,
                                     medoids_io,
                                     k,
                                     nearest_slot,
                                     nearest_dist,
                                     second_dist,
                                     NULL,
                                     NULL,
                                     &current_cost);

        non_count = 0u;
        for (index = 0u; index < point_count; ++index) {
            if (flags[index] == 0u) {
                non_medoids[non_count] = index;
                ++non_count;
            }
        }
        if (non_count == 0u) {
            break;
        }

        candidate_total = (uint64_t)non_count * (uint64_t)k;
        if ((uint64_t)candidate_cap > candidate_total) {
            candidate_cap = (unsigned int)candidate_total;
        }
        candidate_count = 0u;
        attempts = 0u;
        while (candidate_count < candidate_cap
                && attempts < candidate_cap * 16u) {
            slot = sixel_kmedoids_rng_bounded(rng_state, k);
            pick = sixel_kmedoids_rng_bounded(rng_state, non_count);
            candidate = non_medoids[pick];
            duplicate_pair = 0;
            for (probe = 0u; probe < candidate_count; ++probe) {
                if (candidate_slots[probe] == slot
                        && candidate_points[probe] == candidate) {
                    duplicate_pair = 1;
                    break;
                }
            }
            if (!duplicate_pair) {
                candidate_slots[candidate_count] = slot;
                candidate_points[candidate_count] = candidate;
                ++candidate_count;
            }
            ++attempts;
        }
        if (candidate_count == 0u) {
            break;
        }

        active_count = candidate_count;
        for (index = 0u; index < active_count; ++index) {
            active[index] = index;
        }

        status = sixel_kmedoids_bandit_prune_candidates(points,
                                                        weights,
                                                        point_count,
                                                        nearest_slot,
                                                        nearest_dist,
                                                        second_dist,
                                                        candidate_slots,
                                                        candidate_points,
                                                        active,
                                                        &active_count,
                                                        batch_limit,
                                                        rng_state,
                                                        allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        best_cost = current_cost;
        best_slot = 0u;
        best_candidate = medoids_io[0u];
        for (index = 0u; index < active_count; ++index) {
            slot = candidate_slots[active[index]];
            candidate = candidate_points[active[index]];
            swap_cost = sixel_kmedoids_swap_cost(points,
                                                 weights,
                                                 point_count,
                                                 nearest_slot,
                                                 nearest_dist,
                                                 second_dist,
                                                 slot,
                                                 candidate);
            if (swap_cost < best_cost) {
                best_cost = swap_cost;
                best_slot = slot;
                best_candidate = candidate;
            }
        }

        if (best_cost + 1.0e-12 >= current_cost) {
            break;
        }

        medoids_io[best_slot] = best_candidate;
        current_cost = best_cost;
    }

    *cost_io = current_cost;
    if (iterations_out != NULL) {
        *iterations_out = iteration;
    }
    status = SIXEL_OK;

end:
    if (active != NULL) {
        sixel_allocator_free(allocator, active);
    }
    if (candidate_points != NULL) {
        sixel_allocator_free(allocator, candidate_points);
    }
    if (candidate_slots != NULL) {
        sixel_allocator_free(allocator, candidate_slots);
    }
    if (non_medoids != NULL) {
        sixel_allocator_free(allocator, non_medoids);
    }
    if (flags != NULL) {
        sixel_allocator_free(allocator, flags);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    return status;
}

static double
sixel_palette_kmedoids_sum_float_to_byte(double component,
                                         double sample_total,
                                         unsigned int channel,
                                         double const *scale,
                                         double const *offset)
{
    double scaled;

    scaled = component;
    if (scale == NULL || offset == NULL) {
        return scaled;
    }
    if (scale[channel] <= 0.0) {
        return 0.0;
    }

    scaled *= scale[channel];
    scaled += sample_total * offset[channel];
    return scaled;
}

static double
sixel_palette_kmedoids_sum_byte_to_float(double component,
                                         double sample_total,
                                         unsigned int channel,
                                         double const *scale,
                                         double const *offset)
{
    double restored;

    restored = component;
    if (scale == NULL || offset == NULL) {
        return restored;
    }
    if (scale[channel] <= 0.0) {
        return 0.0;
    }

    restored -= sample_total * offset[channel];
    restored /= scale[channel];
    return restored;
}

static SIXELSTATUS
build_palette_kmedoids(unsigned char **result,
                       float **result_float32,
                       unsigned char const *data,
                       unsigned int length,
                       unsigned int depth,
                       unsigned int reqcolors,
                       unsigned int *ncolors,
                       unsigned int *origcolors,
                       int quality_mode,
                       int force_palette,
                       int use_reversible,
                       int final_merge_mode,
                       sixel_allocator_t *allocator,
                       int pixelformat,
                       int treat_input_as_float32,
                       sixel_logger_t *logger,
                       int *job_seq,
                       char const *engine_name,
                       sixel_palette_telemetry_t *telemetry)
{
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int pixel_count;
    unsigned int sample_target;
    unsigned int sample_count;
    unsigned int visible_count;
    double *samples;
    double *points;
    double *weights;
    unsigned int point_count;
    sixel_kmedoids_algo_t algo;
    sixel_kmedoids_algo_t resolved_algo;
    uint32_t seed;
    uint32_t rng_state;
    unsigned int pam_iterations;
    unsigned int iter_override;
    unsigned int sample_override;
    unsigned int clara_trials;
    unsigned int clara_sample_override;
    unsigned int clarans_local;
    unsigned int clarans_neighbors_override;
    unsigned int bandit_iterations;
    unsigned int bandit_candidates;
    unsigned int bandit_batch;
    unsigned int clarans_neighbors;
    unsigned int k;
    unsigned int overshoot;
    unsigned int *medoids;
    unsigned int *base_medoids;
    unsigned int slot;
    unsigned int index;
    unsigned int channel;
    unsigned int iteration_count;
    unsigned int merge_iterations;
    unsigned int total_iterations;
    double solution_cost;
    double *cluster_weights;
    double *cluster_sums;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *second_dist;
    unsigned long *merge_weights;
    double *merge_sums;
    int resolved_merge;
    int apply_merge;
    int cluster_total;
    double merge_component;
    double restored_component;
    double weight_value;
    double *final_centers;
    unsigned int final_count;
    unsigned char *palette;
    float *float_palette;
    unsigned int *order;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    double *sort_weights;
    double float32_channel_scale[3];
    double float32_channel_offset[3];
    int input_is_float32;
    int job_init;
    int job_iter;
    int job_merge;
    int job_export;
    char log_detail[128];
    double wall_start;
    double init_stop;
    double iterate_start;
    double iterate_stop;
    double merge_start;
    double merge_stop;
    double export_start;
    double export_stop;
    unsigned int polish_iterations;
    double *distance_cache;
    uint64_t cache_cells;
    double cache_distance;
    size_t cache_left;
    size_t cache_right;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_stride = depth;
    pixel_count = 0u;
    sample_target = 0u;
    sample_count = 0u;
    visible_count = 0u;
    samples = NULL;
    points = NULL;
    weights = NULL;
    point_count = 0u;
    algo = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
    resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
    seed = 1u;
    rng_state = 1u;
    pam_iterations = 0u;
    iter_override = 0u;
    sample_override = 0u;
    clara_trials = 0u;
    clara_sample_override = 0u;
    clarans_local = 0u;
    clarans_neighbors_override = 0u;
    bandit_iterations = 0u;
    bandit_candidates = 0u;
    bandit_batch = 0u;
    clarans_neighbors = 0u;
    k = 0u;
    overshoot = 0u;
    medoids = NULL;
    base_medoids = NULL;
    slot = 0u;
    index = 0u;
    channel = 0u;
    iteration_count = 0u;
    merge_iterations = 0u;
    total_iterations = 0u;
    solution_cost = 0.0;
    cluster_weights = NULL;
    cluster_sums = NULL;
    nearest_slot = NULL;
    nearest_dist = NULL;
    second_dist = NULL;
    merge_weights = NULL;
    merge_sums = NULL;
    resolved_merge = SIXEL_FINAL_MERGE_NONE;
    apply_merge = 0;
    cluster_total = 0;
    merge_component = 0.0;
    restored_component = 0.0;
    weight_value = 0.0;
    final_centers = NULL;
    final_count = 0u;
    palette = NULL;
    float_palette = NULL;
    order = NULL;
    fill = 0u;
    source = 0u;
    swap_temp = 0u;
    sort_weights = NULL;
    float32_channel_scale[0] = 0.0;
    float32_channel_scale[1] = 0.0;
    float32_channel_scale[2] = 0.0;
    float32_channel_offset[0] = 0.0;
    float32_channel_offset[1] = 0.0;
    float32_channel_offset[2] = 0.0;
    input_is_float32 = 0;
    job_init = -1;
    job_iter = -1;
    job_merge = -1;
    job_export = -1;
    log_detail[0] = '\0';
    wall_start = sixel_timer_now();
    init_stop = wall_start;
    iterate_start = wall_start;
    iterate_stop = wall_start;
    merge_start = wall_start;
    merge_stop = wall_start;
    export_start = wall_start;
    export_stop = wall_start;
    polish_iterations = 0u;
    distance_cache = NULL;
    cache_cells = 0u;
    cache_distance = 0.0;
    cache_left = 0u;
    cache_right = 0u;

    if (result != NULL) {
        *result = NULL;
    }
    if (result_float32 != NULL) {
        *result_float32 = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0u;
    }
    if (origcolors != NULL) {
        *origcolors = 0u;
    }
    if (allocator == NULL || data == NULL || result == NULL) {
        return status;
    }

    input_is_float32 = (treat_input_as_float32
                        && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat));
    if (input_is_float32) {
        for (channel = 0u; channel < 3u; ++channel) {
            float float_minimum;
            float float_maximum;
            double range;

#if HAVE_FLOAT_H
# define SIXEL_KMEDOIDS_FLOAT_BOUND FLT_MAX
#else
# define SIXEL_KMEDOIDS_FLOAT_BOUND 1.0e9f
#endif
            float_minimum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                -SIXEL_KMEDOIDS_FLOAT_BOUND);
            float_maximum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                SIXEL_KMEDOIDS_FLOAT_BOUND);
#undef SIXEL_KMEDOIDS_FLOAT_BOUND
            range = (double)float_maximum - (double)float_minimum;
            if (range <= 0.0) {
                float32_channel_scale[channel] = 0.0;
                float32_channel_offset[channel] = 0.0;
            } else {
                float32_channel_scale[channel] = 255.0 / range;
                float32_channel_offset[channel]
                    = -((double)float_minimum)
                    * float32_channel_scale[channel];
            }
        }
        if (depth == 0u || depth % (unsigned int)sizeof(float) != 0u) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = depth / (unsigned int)sizeof(float);
        pixel_stride = channels * (unsigned int)sizeof(float);
    }

    if (channels != 3u && channels != 4u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixel_stride == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    pixel_count = length / pixel_stride;
    if (pixel_count == 0u) {
        return SIXEL_OK;
    }

    if (reqcolors == 0u) {
        reqcolors = 1u;
    }

    job_init = sixel_palette_kmedoids_log_start(logger,
                                                job_seq,
                                                engine_name,
                                                "palette/init",
                                                "init");

    algo = sixel_get_kmedoids_algo();
    seed = sixel_get_kmedoids_seed();
    if (seed == 0u) {
        seed = 1u;
    }
    rng_state = seed;

    pam_iterations = sixel_kmedoids_quality_pam_iterations(quality_mode);
    iter_override = sixel_get_kmedoids_iter();
    if (iter_override > 0u) {
        pam_iterations = iter_override;
    }
    clara_trials = sixel_kmedoids_quality_clara_trials(quality_mode);
    iter_override = sixel_get_kmedoids_clara_trials();
    if (iter_override > 0u) {
        clara_trials = iter_override;
    }
    clara_sample_override = sixel_get_kmedoids_clara_sample();
    clarans_local = sixel_get_kmedoids_clarans_local();
    clarans_neighbors_override = sixel_get_kmedoids_clarans_neighbors();
    bandit_iterations = sixel_kmedoids_quality_bandit_iterations(quality_mode);
    iter_override = sixel_get_kmedoids_bandit_iter();
    if (iter_override > 0u) {
        bandit_iterations = iter_override;
    }
    bandit_candidates = sixel_get_kmedoids_bandit_candidates();
    bandit_batch = sixel_get_kmedoids_bandit_batch();

    sample_target = sixel_kmedoids_sample_target(reqcolors,
                                                 pixel_count,
                                                 quality_mode);
    sample_override = sixel_get_kmedoids_sample();
    if (sample_override > 0u) {
        sample_target = sample_override;
    }
    status = sixel_kmedoids_collect_samples(data,
                                            length,
                                            channels,
                                            pixel_stride,
                                            input_is_float32,
                                            float32_channel_scale,
                                            float32_channel_offset,
                                            sample_target,
                                            seed,
                                            &samples,
                                            &sample_count,
                                            &visible_count,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors != NULL) {
        *origcolors = visible_count;
    }
    if (sample_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    status = sixel_kmedoids_compress_samples(samples,
                                             sample_count,
                                             &points,
                                             &weights,
                                             &point_count,
                                             allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (point_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    resolved_algo = algo;
    if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO) {
        if (point_count <= 4096u) {
            resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_PAM;
        } else if (point_count <= 65536u) {
            resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA;
        } else {
            resolved_algo = SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM;
        }
    }

    if (point_count <= 2048u) {
        cache_cells = (uint64_t)point_count * (uint64_t)point_count;
        if (cache_cells > 0u
                && cache_cells
                    <= ((uint64_t)SIZE_MAX / (uint64_t)sizeof(double))) {
            distance_cache = (double *)sixel_allocator_malloc(
                allocator,
                (size_t)cache_cells * sizeof(double));
        }
        if (distance_cache != NULL) {
            for (index = 0u; index < point_count; ++index) {
                for (slot = index; slot < point_count; ++slot) {
                    if (index == slot) {
                        cache_distance = 0.0;
                    } else {
                        cache_distance = sixel_kmedoids_distance_sq(points,
                                                                    index,
                                                                    slot);
                    }
                    cache_left = (size_t)index * (size_t)point_count
                               + (size_t)slot;
                    cache_right = (size_t)slot * (size_t)point_count
                                + (size_t)index;
                    distance_cache[cache_left] = cache_distance;
                    distance_cache[cache_right] = cache_distance;
                }
            }
            sixel_kmedoids_set_distance_cache(distance_cache, point_count);
        }
    }

    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    overshoot = reqcolors;
    if (apply_merge) {
        sixel_final_merge_load_env();
        overshoot = sixel_final_merge_target(reqcolors, resolved_merge);
    }
    if (overshoot > point_count) {
        overshoot = point_count;
    }
    if (overshoot == 0u) {
        overshoot = 1u;
    }
    k = overshoot;

    medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    base_medoids = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    if (medoids == NULL || base_medoids == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    init_stop = sixel_timer_now();
    iterate_start = init_stop;
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "samples=%u unique=%u k=%u algo=%s/%s seed=%u",
                                sample_count,
                                point_count,
                                k,
                                sixel_kmedoids_algo_to_string(algo),
                                sixel_kmedoids_algo_to_string(resolved_algo),
                                seed);
    sixel_palette_kmedoids_log_finish(logger,
                                      job_init,
                                      engine_name,
                                      "palette/init",
                                      "init",
                                      log_detail);

    job_iter = sixel_palette_kmedoids_log_start(logger,
                                                job_seq,
                                                engine_name,
                                                "palette/iterate",
                                                "iterate");

    if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_PAM) {
        status = sixel_kmedoids_run_pam(points,
                                        weights,
                                        point_count,
                                        k,
                                        pam_iterations,
                                        NULL,
                                        medoids,
                                        &solution_cost,
                                        &iteration_count,
                                        allocator);
        total_iterations = iteration_count;
    } else if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA) {
        status = sixel_kmedoids_run_clara(points,
                                          weights,
                                          point_count,
                                          k,
                                          pam_iterations,
                                          clara_trials,
                                          clara_sample_override,
                                          &rng_state,
                                          medoids,
                                          &solution_cost,
                                          &iteration_count,
                                          allocator);
        total_iterations = iteration_count;
    } else if (resolved_algo == SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS) {
        status = sixel_kmedoids_run_clara(points,
                                          weights,
                                          point_count,
                                          k,
                                          pam_iterations,
                                          clara_trials,
                                          clara_sample_override,
                                          &rng_state,
                                          base_medoids,
                                          &solution_cost,
                                          &iteration_count,
                                          allocator);
        total_iterations = iteration_count;
        if (SIXEL_SUCCEEDED(status)) {
            if (clarans_local == 0u) {
                clarans_local = clara_trials;
            }
            if (clarans_neighbors_override > 0u) {
                clarans_neighbors = clarans_neighbors_override;
            } else if (point_count > k) {
                uint64_t neigh64;

                neigh64 = (uint64_t)point_count
                        * (uint64_t)(point_count - k);
                clarans_neighbors = 250u * k;
                if (neigh64 < (uint64_t)clarans_neighbors) {
                    clarans_neighbors = (unsigned int)neigh64;
                }
            } else {
                clarans_neighbors = 1u;
            }
            status = sixel_kmedoids_run_clarans(points,
                                                weights,
                                                point_count,
                                                k,
                                                clarans_local,
                                                clarans_neighbors,
                                                &rng_state,
                                                base_medoids,
                                                medoids,
                                                &solution_cost,
                                                &iteration_count,
                                                allocator);
            total_iterations += iteration_count;
        }
    } else {
        status = sixel_kmedoids_run_clara(points,
                                          weights,
                                          point_count,
                                          k,
                                          pam_iterations,
                                          clara_trials,
                                          clara_sample_override,
                                          &rng_state,
                                          medoids,
                                          &solution_cost,
                                          &iteration_count,
                                          allocator);
        total_iterations = iteration_count;
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_kmedoids_run_banditpam(points,
                                                  weights,
                                                  point_count,
                                                  k,
                                                  bandit_iterations,
                                                  bandit_candidates,
                                                  bandit_batch,
                                                  &rng_state,
                                                  medoids,
                                                  &solution_cost,
                                                  &iteration_count,
                                                  allocator);
            total_iterations += iteration_count;
        }
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (resolved_algo != SIXEL_PALETTE_KMEDOIDS_ALGO_PAM
            && quality_mode != SIXEL_QUALITY_LOW
            && point_count <= 16384u) {
        status = sixel_kmedoids_run_pam(points,
                                        weights,
                                        point_count,
                                        k,
                                        1u,
                                        medoids,
                                        medoids,
                                        &solution_cost,
                                        &polish_iterations,
                                        allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        total_iterations += polish_iterations;
    }

    iterate_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "algo=%s iter=%u cost=%.4f",
                                sixel_kmedoids_algo_to_string(resolved_algo),
                                total_iterations,
                                solution_cost);
    sixel_palette_kmedoids_log_finish(logger,
                                      job_iter,
                                      engine_name,
                                      "palette/iterate",
                                      "iterate",
                                      log_detail);

    merge_start = iterate_stop;
    merge_stop = iterate_stop;
    final_centers = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    cluster_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    cluster_sums = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    second_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    if (final_centers == NULL || cluster_weights == NULL || cluster_sums == NULL
            || nearest_slot == NULL || nearest_dist == NULL
            || second_dist == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    sixel_kmedoids_assign_points(points,
                                 weights,
                                 point_count,
                                 medoids,
                                 k,
                                 nearest_slot,
                                 nearest_dist,
                                 second_dist,
                                 cluster_weights,
                                 cluster_sums,
                                 &solution_cost);

    for (slot = 0u; slot < k; ++slot) {
        final_centers[slot * 3u + 0u] = points[medoids[slot] * 3u + 0u];
        final_centers[slot * 3u + 1u] = points[medoids[slot] * 3u + 1u];
        final_centers[slot * 3u + 2u] = points[medoids[slot] * 3u + 2u];
    }
    final_count = k;

    if (apply_merge && k > reqcolors) {
        merge_start = sixel_timer_now();
        job_merge = sixel_palette_kmedoids_log_start(logger,
                                                     job_seq,
                                                     engine_name,
                                                     "palette/merge",
                                                     "merge");
        merge_weights = (unsigned long *)sixel_allocator_malloc(
            allocator,
            (size_t)k * sizeof(unsigned long));
        merge_sums = (double *)sixel_allocator_malloc(
            allocator,
            (size_t)k * 3u * sizeof(double));
        if (merge_weights == NULL || merge_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        for (slot = 0u; slot < k; ++slot) {
            weight_value = cluster_weights[slot];
            if (weight_value <= 0.0) {
                merge_weights[slot] = 1ul;
                merge_sums[slot * 3u + 0u] = 0.0;
                merge_sums[slot * 3u + 1u] = 0.0;
                merge_sums[slot * 3u + 2u] = 0.0;
                continue;
            }
            if (weight_value > (double)ULONG_MAX) {
                merge_weights[slot] = ULONG_MAX;
            } else {
                merge_weights[slot] = (unsigned long)(weight_value + 0.5);
                if (merge_weights[slot] == 0ul) {
                    merge_weights[slot] = 1ul;
                }
            }
            for (channel = 0u; channel < 3u; ++channel) {
                merge_component = cluster_sums[slot * 3u + channel]
                    / cluster_weights[slot];
                if (input_is_float32) {
                    merge_component = sixel_palette_kmedoids_sum_float_to_byte(
                        merge_component,
                        1.0,
                        channel,
                        float32_channel_scale,
                        float32_channel_offset);
                }
                if (merge_component < 0.0) {
                    merge_component = 0.0;
                }
                if (merge_component > 255.0) {
                    merge_component = 255.0;
                }
                merge_sums[slot * 3u + channel]
                    = merge_component * (double)merge_weights[slot];
            }
        }

        cluster_total = sixel_palette_apply_merge(merge_weights,
                                                  merge_sums,
                                                  3u,
                                                  (int)k,
                                                  (int)reqcolors,
                                                  resolved_merge,
                                                  use_reversible,
                                                  pixelformat,
                                                  allocator);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > reqcolors) {
            cluster_total = (int)reqcolors;
        }
        final_count = (unsigned int)cluster_total;
        if (final_count == 0u) {
            final_count = 1u;
        }

        for (slot = 0u; slot < final_count; ++slot) {
            weight_value = (double)merge_weights[slot];
            if (weight_value <= 0.0) {
                weight_value = 1.0;
            }
            cluster_weights[slot] = weight_value;
            for (channel = 0u; channel < 3u; ++channel) {
                restored_component = merge_sums[slot * 3u + channel]
                    / weight_value;
                if (input_is_float32) {
                    restored_component
                        = sixel_palette_kmedoids_sum_byte_to_float(
                            restored_component,
                            1.0,
                            channel,
                            float32_channel_scale,
                            float32_channel_offset);
                }
                final_centers[slot * 3u + channel] = restored_component;
            }
        }
        merge_iterations = 1u;
        merge_stop = sixel_timer_now();
        (void)sixel_compat_snprintf(log_detail,
                                    sizeof(log_detail),
                                    "clusters=%u merge=%d",
                                    final_count,
                                    resolved_merge);
        sixel_palette_kmedoids_log_finish(logger,
                                          job_merge,
                                          engine_name,
                                          "palette/merge",
                                          "merge",
                                          log_detail);
    }

    export_start = sixel_timer_now();
    job_export = sixel_palette_kmedoids_log_start(logger,
                                                  job_seq,
                                                  engine_name,
                                                  "palette/export",
                                                  "export");

    palette = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)final_count * 3u);
    if (palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (result_float32 != NULL && input_is_float32 && final_count > 0u) {
        float_palette = (float *)sixel_allocator_malloc(
            allocator,
            (size_t)final_count * 3u * sizeof(float));
        if (float_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }

    for (slot = 0u; slot < final_count; ++slot) {
        for (channel = 0u; channel < 3u; ++channel) {
            restored_component = final_centers[slot * 3u + channel];
            if (float_palette != NULL) {
                float_palette[slot * 3u + channel]
                    = sixel_pixelformat_float_channel_clamp(
                        pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            if (input_is_float32) {
                restored_component
                    = (double)sixel_pixelformat_float_channel_to_byte(
                        pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            if (restored_component < 0.0) {
                restored_component = 0.0;
            }
            if (restored_component > 255.0) {
                restored_component = 255.0;
            }
            palette[slot * 3u + channel]
                = (unsigned char)(restored_component + 0.5);
        }
    }

    if (force_palette && final_count < reqcolors && final_count > 0u) {
        unsigned char *grown_palette;
        float *grown_float;

        grown_palette = NULL;
        grown_float = NULL;
        grown_palette = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)reqcolors * 3u);
        sort_weights = (double *)sixel_allocator_malloc(
            allocator,
            (size_t)final_count * sizeof(double));
        order = (unsigned int *)sixel_allocator_malloc(
            allocator,
            (size_t)final_count * sizeof(unsigned int));
        if (grown_palette == NULL || sort_weights == NULL || order == NULL) {
            if (grown_palette != NULL) {
                sixel_allocator_free(allocator, grown_palette);
            }
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (float_palette != NULL) {
            grown_float = (float *)sixel_allocator_malloc(
                allocator,
                (size_t)reqcolors * 3u * sizeof(float));
            if (grown_float == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_allocator_free(allocator, grown_palette);
                goto end;
            }
        }

        for (index = 0u; index < final_count * 3u; ++index) {
            grown_palette[index] = palette[index];
        }
        for (index = 0u; index < final_count; ++index) {
            order[index] = index;
            sort_weights[index] = cluster_weights[index];
        }
        for (index = 0u; index + 1u < final_count; ++index) {
            for (slot = index + 1u; slot < final_count; ++slot) {
                if (sort_weights[order[slot]] > sort_weights[order[index]]) {
                    swap_temp = order[index];
                    order[index] = order[slot];
                    order[slot] = swap_temp;
                }
            }
        }
        if (grown_float != NULL) {
            for (index = 0u; index < final_count * 3u; ++index) {
                grown_float[index] = float_palette[index];
            }
        }

        fill = final_count;
        source = 0u;
        while (fill < reqcolors) {
            slot = order[source];
            for (channel = 0u; channel < 3u; ++channel) {
                grown_palette[fill * 3u + channel]
                    = palette[slot * 3u + channel];
                if (grown_float != NULL) {
                    grown_float[fill * 3u + channel]
                        = float_palette[slot * 3u + channel];
                }
            }
            ++fill;
            ++source;
            if (source >= final_count) {
                source = 0u;
            }
        }

        sixel_allocator_free(allocator, palette);
        palette = grown_palette;
        if (float_palette != NULL) {
            sixel_allocator_free(allocator, float_palette);
            float_palette = grown_float;
        }
        final_count = reqcolors;
    }

    *result = palette;
    palette = NULL;
    if (result_float32 != NULL) {
        *result_float32 = float_palette;
        float_palette = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = final_count;
    }

    export_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "colors=%u",
                                final_count);
    sixel_palette_kmedoids_log_finish(logger,
                                      job_export,
                                      engine_name,
                                      "palette/export",
                                      "export",
                                      log_detail);

    status = SIXEL_OK;

end:
    sixel_kmedoids_set_distance_cache(NULL, 0u);
    if (telemetry != NULL) {
        double now;
        double init_span;
        double iterate_span;
        double merge_span;
        double export_span;

        now = sixel_timer_now();
        if (init_stop < wall_start) {
            init_stop = now;
        }
        if (iterate_stop < iterate_start) {
            iterate_stop = init_stop;
        }
        if (merge_stop < merge_start) {
            merge_stop = iterate_stop;
        }
        if (export_stop < export_start) {
            export_stop = now;
        }

        init_span = init_stop - wall_start;
        if (init_span < 0.0) {
            init_span = 0.0;
        }
        iterate_span = iterate_stop - iterate_start;
        if (iterate_span < 0.0) {
            iterate_span = 0.0;
        }
        merge_span = merge_stop - merge_start;
        if (merge_span < 0.0) {
            merge_span = 0.0;
        }
        export_span = export_stop - export_start;
        if (export_span < 0.0) {
            export_span = 0.0;
        }

        telemetry->init_ms = init_span * 1000.0;
        telemetry->iterate_ms = iterate_span * 1000.0;
        telemetry->merge_ms = merge_span * 1000.0;
        telemetry->export_ms = export_span * 1000.0;
        telemetry->iterate_count = total_iterations;
        telemetry->merge_iterate_count = merge_iterations;
        telemetry->merge_mode = (apply_merge && k > reqcolors)
            ? resolved_merge
            : SIXEL_FINAL_MERGE_NONE;
    }

    if (float_palette != NULL) {
        sixel_allocator_free(allocator, float_palette);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (sort_weights != NULL) {
        sixel_allocator_free(allocator, sort_weights);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (merge_sums != NULL) {
        sixel_allocator_free(allocator, merge_sums);
    }
    if (merge_weights != NULL) {
        sixel_allocator_free(allocator, merge_weights);
    }
    if (second_dist != NULL) {
        sixel_allocator_free(allocator, second_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weights != NULL) {
        sixel_allocator_free(allocator, cluster_weights);
    }
    if (final_centers != NULL) {
        sixel_allocator_free(allocator, final_centers);
    }
    if (base_medoids != NULL) {
        sixel_allocator_free(allocator, base_medoids);
    }
    if (medoids != NULL) {
        sixel_allocator_free(allocator, medoids);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    if (samples != NULL) {
        sixel_allocator_free(allocator, samples);
    }
    if (distance_cache != NULL) {
        sixel_allocator_free(allocator, distance_cache);
    }
    return status;
}

static SIXELSTATUS
sixel_palette_build_kmedoids_internal(sixel_palette_t *palette,
                                      unsigned char const *data,
                                      unsigned int length,
                                      int pixelformat,
                                      sixel_allocator_t *allocator,
                                      sixel_logger_t *logger,
                                      int *job_seq,
                                      char const *engine_name,
                                      int treat_input_as_float32,
                                      sixel_palette_telemetry_t *telemetry)
{
    SIXELSTATUS status;
    sixel_allocator_t *work_allocator;
    unsigned char *entries;
    float *entries_float32;
    unsigned int ncolors;
    unsigned int origcolors;
    unsigned int input_depth;
    unsigned int entry_depth;
    int depth_result;
    size_t payload_size;

    status = SIXEL_BAD_ARGUMENT;
    work_allocator = allocator;
    entries = NULL;
    entries_float32 = NULL;
    ncolors = 0u;
    origcolors = 0u;
    input_depth = 0u;
    entry_depth = 0u;
    depth_result = 0;
    payload_size = 0u;

    if (palette == NULL) {
        return status;
    }
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return status;
    }

    depth_result = sixel_helper_compute_depth(pixelformat);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kmedoids: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    input_depth = (unsigned int)depth_result;

    depth_result = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kmedoids: rgb888 depth lookup failed.");
        return SIXEL_BAD_ARGUMENT;
    }
    entry_depth = (unsigned int)depth_result;

    status = build_palette_kmedoids(&entries,
                                    &entries_float32,
                                    data,
                                    length,
                                    input_depth,
                                    palette->requested_colors,
                                    &ncolors,
                                    &origcolors,
                                    palette->quality_mode,
                                    palette->force_palette,
                                    palette->use_reversible,
                                    palette->final_merge_mode,
                                    work_allocator,
                                    pixelformat,
                                    treat_input_as_float32,
                                    logger,
                                    job_seq,
                                    engine_name,
                                    telemetry);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (palette->use_reversible && entries != NULL) {
        sixel_palette_reversible_palette(entries,
                                         ncolors,
                                         SIXEL_PIXELFORMAT_RGB888);
    }

    payload_size = (size_t)ncolors * (size_t)entry_depth;
    status = sixel_palette_resize(palette,
                                  ncolors,
                                  (int)entry_depth,
                                  work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (payload_size > 0u) {
        if (palette->entries == NULL || entries == NULL) {
            sixel_helper_set_additional_message(
                "sixel_palette_build_kmedoids: palette payload is missing.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(palette->entries, entries, payload_size);
    }
    palette->entry_count = ncolors;
    palette->original_colors = origcolors;
    palette->depth = (int)entry_depth;

    if (entries_float32 != NULL) {
        status = sixel_palette_set_entries_float32(
            palette,
            entries_float32,
            ncolors,
            (int)(3u * (unsigned int)sizeof(float)),
            work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        status = sixel_palette_set_entries_float32(palette,
                                                   NULL,
                                                   0u,
                                                   0,
                                                   work_allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    if (entries_float32 != NULL) {
        sixel_allocator_free(work_allocator, entries_float32);
    }
    if (entries != NULL) {
        sixel_allocator_free(work_allocator, entries);
    }
    return status;
}

SIXELSTATUS
sixel_palette_build_kmedoids(sixel_palette_t *palette,
                             unsigned char const *data,
                             unsigned int length,
                             int pixelformat,
                             sixel_allocator_t *allocator,
                             sixel_logger_t *logger,
                             int *job_seq,
                             char const *engine_name,
                             sixel_palette_telemetry_t *telemetry)
{
    return sixel_palette_build_kmedoids_internal(palette,
                                                 data,
                                                 length,
                                                 pixelformat,
                                                 allocator,
                                                 logger,
                                                 job_seq,
                                                 engine_name,
                                                 0,
                                                 telemetry);
}

SIXELSTATUS
sixel_palette_build_kmedoids_float32(sixel_palette_t *palette,
                                     float const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator,
                                     sixel_logger_t *logger,
                                     int *job_seq,
                                     char const *engine_name,
                                     sixel_palette_telemetry_t *telemetry)
{
    return sixel_palette_build_kmedoids_internal(palette,
                                                 (unsigned char const *)data,
                                                 length,
                                                 pixelformat,
                                                 allocator,
                                                 logger,
                                                 job_seq,
                                                 engine_name,
                                                 1,
                                                 telemetry);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
