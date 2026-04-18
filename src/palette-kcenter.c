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
 * k-center palette builder.
 *
 * The solver keeps centers anchored to sampled points and optimizes the
 * maximum assignment radius.  Three concrete strategies are available:
 *   - fft: Gonzalez farthest-first traversal.
 *   - swap: randomized initialization with worst-point local swaps.
 *   - hybrid: farthest-first initialization followed by swap refinement.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kcenter.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"

#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
# define SIXEL_KCENTER_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_TLS _Thread_local
# define SIXEL_KCENTER_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_TLS __thread
# define SIXEL_KCENTER_TLS_AVAILABLE 1
#else
# define SIXEL_TLS
# define SIXEL_KCENTER_TLS_AVAILABLE 0
#endif

static SIXEL_TLS int sixel_kcenter_algo_override_enabled = 0;
static SIXEL_TLS sixel_kcenter_algo_t sixel_kcenter_algo_override_value
    = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
static SIXEL_TLS int sixel_kcenter_seed_override_enabled = 0;
static SIXEL_TLS uint32_t sixel_kcenter_seed_override_value = 1u;
static SIXEL_TLS int sixel_kcenter_restarts_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_restarts_override_value = 1u;
static SIXEL_TLS int sixel_kcenter_iter_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_iter_override_value = 16u;
static SIXEL_TLS int sixel_kcenter_histbits_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_histbits_override_value = 5u;
static SIXEL_TLS int sixel_kcenter_point_budget_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kcenter_point_budget_override_value = 0u;
static SIXEL_TLS int sixel_kcenter_prune_mass_override_enabled = 0;
static SIXEL_TLS double sixel_kcenter_prune_mass_override_value = 0.995;

#undef SIXEL_TLS

#if SIXEL_ENABLE_THREADS && !SIXEL_KCENTER_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#  if !defined(UNICODE)
#   define UNICODE
#  endif
#  if !defined(_UNICODE)
#   define _UNICODE
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
static CRITICAL_SECTION sixel_kcenter_override_mutex;
static INIT_ONCE sixel_kcenter_override_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_kcenter_override_lock_init_once(PINIT_ONCE once,
                                      PVOID parameter,
                                      PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&sixel_kcenter_override_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
static pthread_mutex_t sixel_kcenter_override_mutex;
static pthread_once_t sixel_kcenter_override_mutex_once = PTHREAD_ONCE_INIT;
static int sixel_kcenter_override_mutex_ready = 0;

static void
sixel_kcenter_override_lock_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&sixel_kcenter_override_mutex, NULL);
    if (rc == 0) {
        sixel_kcenter_override_mutex_ready = 1;
    }
}
# endif
#endif

/*
 * K-center tunables are thread-local when TLS exists.  Without TLS they become
 * process globals, so serialize override access in threaded builds.
 */
static int
sixel_kcenter_override_lock_acquire(void)
{
#if SIXEL_ENABLE_THREADS && !SIXEL_KCENTER_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&sixel_kcenter_override_once,
                                      sixel_kcenter_override_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        return 0;
    }
    EnterCriticalSection(&sixel_kcenter_override_mutex);
    return 1;
# else
    int rc;
    int once_status;

    once_status = pthread_once(&sixel_kcenter_override_mutex_once,
                               sixel_kcenter_override_lock_init_once);
    if (once_status != 0 || !sixel_kcenter_override_mutex_ready) {
        return 0;
    }

    rc = pthread_mutex_lock(&sixel_kcenter_override_mutex);
    if (rc != 0) {
        return 0;
    }
    return 1;
# endif
#else
    return 0;
#endif
}

static void
sixel_kcenter_override_lock_release(int acquired)
{
    if (acquired == 0) {
        return;
    }
#if SIXEL_ENABLE_THREADS && !SIXEL_KCENTER_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&sixel_kcenter_override_mutex);
# else
    if (!sixel_kcenter_override_mutex_ready) {
        return;
    }
    (void)pthread_mutex_unlock(&sixel_kcenter_override_mutex);
# endif
#endif
}

#undef SIXEL_KCENTER_TLS_AVAILABLE

typedef struct sixel_kcenter_bin {
    unsigned int index;
    unsigned int count;
    double r;
    double g;
    double b;
} sixel_kcenter_bin_t;

/* Keep solver call signatures compact for strict C compilers. */
typedef struct sixel_kcenter_swap_ctx {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int *centers;
    unsigned int k;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *radius2_io;
    double *sse_io;
    unsigned int *scratch_slot;
    double *scratch_dist;
} sixel_kcenter_swap_ctx_t;

typedef struct sixel_kcenter_solver_ctx {
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int k;
    sixel_kcenter_algo_t resolved_algo;
    unsigned int iter_limit;
    uint32_t *rng_state;
    unsigned int *centers;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    double *radius2_out;
    double *sse_out;
    unsigned int *iterations_out;
    unsigned int *scratch_indices;
    double *fft_dist_cache;
    unsigned char *center_mask;
} sixel_kcenter_solver_ctx_t;

typedef struct sixel_kcenter_build_ctx {
    unsigned char **result;
    float **result_float32;
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    unsigned int reqcolors;
    unsigned int *ncolors;
    unsigned int *origcolors;
    int quality_mode;
    int force_palette;
    int use_reversible;
    int final_merge_mode;
    sixel_allocator_t *allocator;
    int pixelformat;
    int treat_input_as_float32;
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    sixel_palette_telemetry_t *telemetry;
} sixel_kcenter_build_ctx_t;

typedef struct sixel_kcenter_internal_ctx {
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    int treat_input_as_float32;
    sixel_palette_telemetry_t *telemetry;
} sixel_kcenter_internal_ctx_t;

static int
sixel_palette_kcenter_log_start(sixel_logger_t *logger,
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
sixel_palette_kcenter_log_finish(sixel_logger_t *logger,
                                 int job_id,
                                 char const *engine_name,
                                 char const *role,
                                 char const *phase,
                                 char const *detail)
{
    char const *suffix;

    if (logger == NULL || job_id < 0) {
        return;
    }
    suffix = "";
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

static sixel_kcenter_algo_t
sixel_kcenter_resolve_algo(sixel_kcenter_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KCENTER_ALGO_FFT:
    case SIXEL_PALETTE_KCENTER_ALGO_SWAP:
    case SIXEL_PALETTE_KCENTER_ALGO_HYBRID:
    case SIXEL_PALETTE_KCENTER_ALGO_AUTO:
        return algo;
    default:
        return SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    }
}

static char const *
sixel_kcenter_algo_to_string(sixel_kcenter_algo_t algo)
{
    switch (algo) {
    case SIXEL_PALETTE_KCENTER_ALGO_FFT:
        return "fft";
    case SIXEL_PALETTE_KCENTER_ALGO_SWAP:
        return "swap";
    case SIXEL_PALETTE_KCENTER_ALGO_HYBRID:
        return "hybrid";
    case SIXEL_PALETTE_KCENTER_ALGO_AUTO:
    default:
        return "auto";
    }
}

static uint32_t
sixel_kcenter_rng_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    if (value == 0u) {
        value = 1u;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static unsigned int
sixel_kcenter_rng_bounded(uint32_t *state,
                          unsigned int upper)
{
    uint32_t value;

    if (upper <= 1u) {
        return 0u;
    }
    value = sixel_kcenter_rng_next(state);
    return (unsigned int)(value % upper);
}

static unsigned int
sixel_kcenter_parse_env_uint(char const *name,
                             unsigned int min_value,
                             unsigned int max_value,
                             unsigned int fallback,
                             int allow_zero)
{
    char const *env;
    unsigned long parsed;
    char *endptr;

    env = NULL;
    parsed = 0ul;
    endptr = NULL;

    if (name == NULL) {
        return fallback;
    }

    env = sixel_compat_getenv(name);
    if (env == NULL || env[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtoul(env, &endptr, 10);
    if (endptr == env || endptr == NULL || endptr[0] != '\0' || errno != 0) {
        return fallback;
    }
    if (allow_zero && parsed == 0ul) {
        return 0u;
    }
    if (parsed < (unsigned long)min_value
            || parsed > (unsigned long)max_value) {
        return fallback;
    }
    return (unsigned int)parsed;
}

static double
sixel_kcenter_parse_env_double(char const *name,
                               double min_value,
                               double max_value,
                               double fallback)
{
    char const *env;
    double parsed;
    char *endptr;

    env = NULL;
    parsed = 0.0;
    endptr = NULL;

    if (name == NULL) {
        return fallback;
    }

    env = sixel_compat_getenv(name);
    if (env == NULL || env[0] == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtod(env, &endptr);
    if (endptr == env || endptr == NULL || endptr[0] != '\0' || errno != 0) {
        return fallback;
    }
    if (parsed != parsed || parsed < min_value || parsed > max_value) {
        return fallback;
    }

    return parsed;
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_algo_override(int enabled,
                                sixel_kcenter_algo_t algo)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_algo_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_algo_override_value = sixel_kcenter_resolve_algo(algo);
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API sixel_kcenter_algo_t
sixel_get_kcenter_algo(void)
{
    char const *env_value;
    sixel_kcenter_algo_t parsed;
    static int cached = 0;
    static sixel_kcenter_algo_t cached_value = SIXEL_PALETTE_KCENTER_ALGO_AUTO;

    if (sixel_kcenter_algo_override_enabled) {
        return sixel_kcenter_resolve_algo(sixel_kcenter_algo_override_value);
    }

    if (cached) {
        return cached_value;
    }

    parsed = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    env_value = sixel_compat_getenv("SIXEL_PALETTE_KCENTER_ALGO");
    if (env_value != NULL) {
        if (strcmp(env_value, "fft") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_FFT;
        } else if (strcmp(env_value, "swap") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_SWAP;
        } else if (strcmp(env_value, "hybrid") == 0) {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_HYBRID;
        } else {
            parsed = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
        }
    }
    cached_value = sixel_kcenter_resolve_algo(parsed);
    cached = 1;
    return cached_value;
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_seed_override(int enabled,
                                uint32_t seed)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_seed_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_seed_override_value = seed;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API uint32_t
sixel_get_kcenter_seed(void)
{
    unsigned int value;

    if (sixel_kcenter_seed_override_enabled) {
        value = (unsigned int)sixel_kcenter_seed_override_value;
        if (value == 0u) {
            value = 1u;
        }
        return (uint32_t)value;
    }

    value = sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_SEED",
                                         0u,
                                         UINT_MAX,
                                         1u,
                                         1);
    if (value == 0u) {
        value = 1u;
    }
    return (uint32_t)value;
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_restarts_override(int enabled,
                                    unsigned int restarts)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_restarts_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_restarts_override_value = restarts;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_restarts(void)
{
    if (sixel_kcenter_restarts_override_enabled) {
        if (sixel_kcenter_restarts_override_value < 1u) {
            return 1u;
        }
        if (sixel_kcenter_restarts_override_value > 32u) {
            return 32u;
        }
        return sixel_kcenter_restarts_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_RESTARTS",
                                        1u,
                                        32u,
                                        1u,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_iter_override(int enabled,
                                unsigned int iter_count)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_iter_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_iter_override_value = iter_count;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_iter(void)
{
    if (sixel_kcenter_iter_override_enabled) {
        if (sixel_kcenter_iter_override_value < 1u) {
            return 1u;
        }
        if (sixel_kcenter_iter_override_value > 64u) {
            return 64u;
        }
        return sixel_kcenter_iter_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_ITER",
                                        1u,
                                        64u,
                                        16u,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_histbits_override(int enabled,
                                    unsigned int histbits)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_histbits_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_histbits_override_value = histbits;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_histbits(void)
{
    if (sixel_kcenter_histbits_override_enabled) {
        if (sixel_kcenter_histbits_override_value < 3u) {
            return 3u;
        }
        if (sixel_kcenter_histbits_override_value > 6u) {
            return 6u;
        }
        return sixel_kcenter_histbits_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_HISTBITS",
                                        3u,
                                        6u,
                                        5u,
                                        0);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_point_budget_override(int enabled,
                                        unsigned int point_budget)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_point_budget_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_point_budget_override_value = point_budget;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API unsigned int
sixel_get_kcenter_point_budget(void)
{
    if (sixel_kcenter_point_budget_override_enabled) {
        if (sixel_kcenter_point_budget_override_value == 0u) {
            return 0u;
        }
        if (sixel_kcenter_point_budget_override_value < 64u) {
            return 64u;
        }
        if (sixel_kcenter_point_budget_override_value > 16384u) {
            return 16384u;
        }
        return sixel_kcenter_point_budget_override_value;
    }

    return sixel_kcenter_parse_env_uint("SIXEL_PALETTE_KCENTER_POINT_BUDGET",
                                        64u,
                                        16384u,
                                        0u,
                                        1);
}

SIXEL_INTERNAL_API void
sixel_set_kcenter_prune_mass_override(int enabled,
                                      double prune_mass)
{
    int lock_acquired;

    lock_acquired = sixel_kcenter_override_lock_acquire();
    sixel_kcenter_prune_mass_override_enabled = enabled ? 1 : 0;
    sixel_kcenter_prune_mass_override_value = prune_mass;
    sixel_kcenter_override_lock_release(lock_acquired);
}

SIXEL_INTERNAL_API double
sixel_get_kcenter_prune_mass(void)
{
    if (sixel_kcenter_prune_mass_override_enabled) {
        if (sixel_kcenter_prune_mass_override_value < 0.900) {
            return 0.900;
        }
        if (sixel_kcenter_prune_mass_override_value > 1.000) {
            return 1.000;
        }
        return sixel_kcenter_prune_mass_override_value;
    }

    return sixel_kcenter_parse_env_double("SIXEL_PALETTE_KCENTER_PRUNE_MASS",
                                          0.900,
                                          1.000,
                                          0.995);
}

static int
sixel_kcenter_compare_bin_desc(void const *lhs,
                               void const *rhs)
{
    sixel_kcenter_bin_t const *a;
    sixel_kcenter_bin_t const *b;

    a = (sixel_kcenter_bin_t const *)lhs;
    b = (sixel_kcenter_bin_t const *)rhs;
    if (a->count > b->count) {
        return -1;
    }
    if (a->count < b->count) {
        return 1;
    }
    if (a->index < b->index) {
        return -1;
    }
    if (a->index > b->index) {
        return 1;
    }
    return 0;
}

static unsigned int
sixel_kcenter_auto_point_budget(unsigned int reqcolors,
                                unsigned int visible_count,
                                int quality_mode)
{
    unsigned int budget;

    budget = reqcolors;
    if (budget == 0u) {
        budget = 1u;
    }
    if (quality_mode == SIXEL_QUALITY_LOW) {
        budget *= 8u;
    } else if (quality_mode == SIXEL_QUALITY_FULL) {
        budget *= 24u;
    } else {
        budget *= 16u;
    }
    if (budget < 64u) {
        budget = 64u;
    }
    if (budget > 16384u) {
        budget = 16384u;
    }
    if (visible_count > 0u && budget > visible_count) {
        budget = visible_count;
    }
    if (budget == 0u) {
        budget = 1u;
    }
    return budget;
}

static SIXELSTATUS
sixel_kcenter_collect_points(double **points_out,
                             double **weights_out,
                             unsigned int *point_count_out,
                             unsigned int *visible_count_out,
                             unsigned char const *data,
                             unsigned int length,
                             unsigned int depth,
                             int pixelformat,
                             int treat_input_as_float32,
                             unsigned int histbits,
                             unsigned int point_budget,
                             double prune_mass,
                             unsigned int reqcolors,
                             int quality_mode,
                             double const *float32_channel_scale,
                             double const *float32_channel_offset,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_stride;
    unsigned int pixel_count;
    unsigned int bin_count;
    unsigned int shift_bits;
    unsigned int active_count;
    unsigned int visible_count;
    unsigned int keep_count;
    unsigned int budget;
    unsigned int index;
    unsigned int ri;
    unsigned int gi;
    unsigned int bi;
    unsigned int bin_index;
    unsigned int offset;
    unsigned int alpha_byte;
    double total_weight;
    double keep_target;
    double accum_weight;
    double red;
    double green;
    double blue;
    double *sums;
    unsigned int *counts;
    sixel_kcenter_bin_t *bins;
    double *points;
    double *weights;
    float const *pixel_float;
    int input_is_float32;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_stride = depth;
    pixel_count = 0u;
    bin_count = 0u;
    shift_bits = 0u;
    active_count = 0u;
    visible_count = 0u;
    keep_count = 0u;
    budget = point_budget;
    index = 0u;
    ri = 0u;
    gi = 0u;
    bi = 0u;
    bin_index = 0u;
    offset = 0u;
    alpha_byte = 0u;
    total_weight = 0.0;
    keep_target = 0.0;
    accum_weight = 0.0;
    red = 0.0;
    green = 0.0;
    blue = 0.0;
    sums = NULL;
    counts = NULL;
    bins = NULL;
    points = NULL;
    weights = NULL;
    pixel_float = NULL;
    input_is_float32 = 0;

    if (points_out == NULL || weights_out == NULL || point_count_out == NULL
            || visible_count_out == NULL || data == NULL || allocator == NULL) {
        return status;
    }
    *points_out = NULL;
    *weights_out = NULL;
    *point_count_out = 0u;
    *visible_count_out = 0u;

    input_is_float32 = (treat_input_as_float32
                        && SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat));
    if (input_is_float32) {
        if (depth == 0u || depth % (unsigned int)sizeof(float) != 0u) {
            return SIXEL_BAD_ARGUMENT;
        }
        channels = depth / (unsigned int)sizeof(float);
        pixel_stride = depth;
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

    shift_bits = 8u - histbits;
    bin_count = 1u << (histbits * 3u);

    counts = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * sizeof(unsigned int));
    sums = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)bin_count * 3u * sizeof(double));
    if (counts == NULL || sums == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memset(counts, 0, (size_t)bin_count * sizeof(unsigned int));
    memset(sums, 0, (size_t)bin_count * 3u * sizeof(double));

    for (index = 0u; index < pixel_count; ++index) {
        offset = index * pixel_stride;
        if (channels == 4u) {
            if (input_is_float32) {
                pixel_float = (float const *)(void const *)(data + offset);
                if (sixel_pixelformat_float_channel_clamp(
                        pixelformat,
                        3,
                        pixel_float[3]) <= 0.0f) {
                    continue;
                }
            } else {
                alpha_byte = data[offset + 3u];
                if (alpha_byte == 0u) {
                    continue;
                }
            }
        }

        if (input_is_float32) {
            pixel_float = (float const *)(void const *)(data + offset);
            red = (double)sixel_pixelformat_float_channel_clamp(
                pixelformat,
                0,
                pixel_float[0]);
            green = (double)sixel_pixelformat_float_channel_clamp(
                pixelformat,
                1,
                pixel_float[1]);
            blue = (double)sixel_pixelformat_float_channel_clamp(
                pixelformat,
                2,
                pixel_float[2]);
            red = red * float32_channel_scale[0] + float32_channel_offset[0];
            green = green * float32_channel_scale[1]
                + float32_channel_offset[1];
            blue = blue * float32_channel_scale[2]
                + float32_channel_offset[2];
        } else {
            red = (double)data[offset + 0u];
            green = (double)data[offset + 1u];
            blue = (double)data[offset + 2u];
        }

        if (red < 0.0) {
            red = 0.0;
        } else if (red > 255.0) {
            red = 255.0;
        }
        if (green < 0.0) {
            green = 0.0;
        } else if (green > 255.0) {
            green = 255.0;
        }
        if (blue < 0.0) {
            blue = 0.0;
        } else if (blue > 255.0) {
            blue = 255.0;
        }

        ri = ((unsigned int)red) >> shift_bits;
        gi = ((unsigned int)green) >> shift_bits;
        bi = ((unsigned int)blue) >> shift_bits;
        bin_index = (ri << (histbits * 2u)) | (gi << histbits) | bi;
        if (bin_index >= bin_count) {
            continue;
        }
        counts[bin_index] += 1u;
        sums[bin_index * 3u + 0u] += red;
        sums[bin_index * 3u + 1u] += green;
        sums[bin_index * 3u + 2u] += blue;
        ++visible_count;
    }

    *visible_count_out = visible_count;
    if (visible_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    for (index = 0u; index < bin_count; ++index) {
        if (counts[index] > 0u) {
            ++active_count;
        }
    }
    if (active_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    bins = (sixel_kcenter_bin_t *)sixel_allocator_malloc(
        allocator,
        (size_t)active_count * sizeof(sixel_kcenter_bin_t));
    if (bins == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    keep_count = 0u;
    total_weight = 0.0;
    for (index = 0u; index < bin_count; ++index) {
        if (counts[index] == 0u) {
            continue;
        }
        bins[keep_count].index = index;
        bins[keep_count].count = counts[index];
        bins[keep_count].r = sums[index * 3u + 0u] / (double)counts[index];
        bins[keep_count].g = sums[index * 3u + 1u] / (double)counts[index];
        bins[keep_count].b = sums[index * 3u + 2u] / (double)counts[index];
        total_weight += (double)counts[index];
        ++keep_count;
    }

    qsort(bins,
          keep_count,
          sizeof(sixel_kcenter_bin_t),
          sixel_kcenter_compare_bin_desc);

    keep_target = prune_mass * total_weight;
    if (keep_target < 1.0) {
        keep_target = 1.0;
    }
    accum_weight = 0.0;
    active_count = 0u;
    for (index = 0u; index < keep_count; ++index) {
        accum_weight += (double)bins[index].count;
        ++active_count;
        if (accum_weight >= keep_target && active_count >= reqcolors) {
            break;
        }
    }
    if (active_count == 0u) {
        active_count = 1u;
    }

    if (budget == 0u) {
        budget = sixel_kcenter_auto_point_budget(reqcolors,
                                                 visible_count,
                                                 quality_mode);
    }
    if (budget > active_count) {
        budget = active_count;
    }
    if (budget == 0u) {
        budget = 1u;
    }

    points = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)budget * 3u * sizeof(double));
    weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)budget * sizeof(double));
    if (points == NULL || weights == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (index = 0u; index < budget; ++index) {
        points[index * 3u + 0u] = bins[index].r;
        points[index * 3u + 1u] = bins[index].g;
        points[index * 3u + 2u] = bins[index].b;
        weights[index] = (double)bins[index].count;
    }

    *points_out = points;
    *weights_out = weights;
    *point_count_out = budget;
    points = NULL;
    weights = NULL;
    status = SIXEL_OK;

end:
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    if (bins != NULL) {
        sixel_allocator_free(allocator, bins);
    }
    if (sums != NULL) {
        sixel_allocator_free(allocator, sums);
    }
    if (counts != NULL) {
        sixel_allocator_free(allocator, counts);
    }
    return status;
}

static double
sixel_kcenter_distance_sq(double const *points,
                          unsigned int left,
                          unsigned int right)
{
    double dr;
    double dg;
    double db;

    dr = points[left * 3u + 0u] - points[right * 3u + 0u];
    dg = points[left * 3u + 1u] - points[right * 3u + 1u];
    db = points[left * 3u + 2u] - points[right * 3u + 2u];
    return dr * dr + dg * dg + db * db;
}

static void
sixel_kcenter_assign_points(double const *points,
                            double const *weights,
                            unsigned int point_count,
                            unsigned int const *centers,
                            unsigned int k,
                            unsigned int *nearest_slot,
                            double *nearest_dist,
                            double *radius2_out,
                            double *sse_out,
                            double *cluster_weights,
                            double *cluster_sums)
{
    unsigned int index;
    unsigned int slot;
    unsigned int best_slot;
    unsigned int center_index;
    double distance;
    double best_distance;
    double radius2;
    double sse;
    double weight_value;

    index = 0u;
    slot = 0u;
    best_slot = 0u;
    center_index = 0u;
    distance = 0.0;
    best_distance = 0.0;
    radius2 = 0.0;
    sse = 0.0;
    weight_value = 0.0;

    if (cluster_weights != NULL) {
        memset(cluster_weights, 0, (size_t)k * sizeof(double));
    }
    if (cluster_sums != NULL) {
        memset(cluster_sums, 0, (size_t)k * 3u * sizeof(double));
    }

    for (index = 0u; index < point_count; ++index) {
        best_slot = 0u;
        best_distance = sixel_kcenter_distance_sq(points, index, centers[0u]);
        for (slot = 1u; slot < k; ++slot) {
            center_index = centers[slot];
            distance = sixel_kcenter_distance_sq(points, index, center_index);
            if (distance < best_distance) {
                best_distance = distance;
                best_slot = slot;
            }
        }

        if (nearest_slot != NULL) {
            nearest_slot[index] = best_slot;
        }
        if (nearest_dist != NULL) {
            nearest_dist[index] = best_distance;
        }
        if (best_distance > radius2) {
            radius2 = best_distance;
        }

        weight_value = 1.0;
        if (weights != NULL) {
            weight_value = weights[index];
            if (weight_value <= 0.0) {
                weight_value = 1.0;
            }
        }
        sse += best_distance * weight_value;

        if (cluster_weights != NULL) {
            cluster_weights[best_slot] += weight_value;
        }
        if (cluster_sums != NULL) {
            cluster_sums[best_slot * 3u + 0u]
                += points[index * 3u + 0u] * weight_value;
            cluster_sums[best_slot * 3u + 1u]
                += points[index * 3u + 1u] * weight_value;
            cluster_sums[best_slot * 3u + 2u]
                += points[index * 3u + 2u] * weight_value;
        }
    }

    if (radius2_out != NULL) {
        *radius2_out = radius2;
    }
    if (sse_out != NULL) {
        *sse_out = sse;
    }
}

static int
sixel_kcenter_is_center(unsigned int const *centers,
                        unsigned int center_count,
                        unsigned int point_index)
{
    unsigned int slot;

    for (slot = 0u; slot < center_count; ++slot) {
        if (centers[slot] == point_index) {
            return 1;
        }
    }
    return 0;
}

static unsigned int
sixel_kcenter_pick_weighted_seed(double const *weights,
                                 unsigned int point_count,
                                 uint32_t *rng_state)
{
    unsigned int index;
    double total;
    double threshold;
    double scale;

    index = 0u;
    total = 0.0;
    threshold = 0.0;
    scale = 0.0;

    if (weights == NULL || point_count == 0u) {
        return 0u;
    }

    for (index = 0u; index < point_count; ++index) {
        if (weights[index] > 0.0) {
            total += weights[index];
        }
    }
    if (total <= 0.0) {
        return sixel_kcenter_rng_bounded(rng_state, point_count);
    }

    scale = (double)sixel_kcenter_rng_next(rng_state)
        / (double)UINT_MAX;
    threshold = scale * total;
    total = 0.0;
    for (index = 0u; index < point_count; ++index) {
        if (weights[index] <= 0.0) {
            continue;
        }
        total += weights[index];
        if (total >= threshold) {
            return index;
        }
    }

    return point_count - 1u;
}

static void
sixel_kcenter_choose_fft(double const *points,
                         double const *weights,
                         unsigned int point_count,
                         unsigned int k,
                         uint32_t *rng_state,
                         unsigned int *centers,
                         double *dist2_cache,
                         unsigned char *center_mask)
{
    unsigned int slot;
    unsigned int index;
    unsigned int selected;
    unsigned int tie_index;
    double distance;
    double best_distance;
    double tie_weight;
    double weight_value;

    slot = 0u;
    index = 0u;
    selected = 0u;
    tie_index = 0u;
    distance = 0.0;
    best_distance = 0.0;
    tie_weight = 0.0;
    weight_value = 0.0;

    if (k == 0u || point_count == 0u) {
        return;
    }

    memset(center_mask, 0, (size_t)point_count);
    centers[0u] = sixel_kcenter_pick_weighted_seed(weights,
                                                    point_count,
                                                    rng_state);
    center_mask[centers[0u]] = 1u;

    for (index = 0u; index < point_count; ++index) {
        dist2_cache[index] = sixel_kcenter_distance_sq(points,
                                                       index,
                                                       centers[0u]);
    }

    for (slot = 1u; slot < k; ++slot) {
        selected = 0u;
        best_distance = -1.0;
        tie_weight = -1.0;
        tie_index = 0u;
        for (index = 0u; index < point_count; ++index) {
            if (center_mask[index]) {
                continue;
            }
            distance = dist2_cache[index];
            if (distance > best_distance + 1.0e-12) {
                best_distance = distance;
                tie_weight = (weights != NULL) ? weights[index] : 1.0;
                tie_index = index;
                selected = 1u;
            } else if (distance >= best_distance - 1.0e-12) {
                weight_value = (weights != NULL) ? weights[index] : 1.0;
                if (weight_value > tie_weight + 1.0e-12) {
                    tie_weight = weight_value;
                    tie_index = index;
                    selected = 1u;
                } else if (weight_value >= tie_weight - 1.0e-12
                        && index < tie_index) {
                    tie_index = index;
                    selected = 1u;
                }
            }
        }
        if (!selected) {
            tie_index = sixel_kcenter_rng_bounded(rng_state, point_count);
        }
        centers[slot] = tie_index;
        center_mask[tie_index] = 1u;

        for (index = 0u; index < point_count; ++index) {
            distance = sixel_kcenter_distance_sq(points, index, tie_index);
            if (distance < dist2_cache[index]) {
                dist2_cache[index] = distance;
            }
        }
    }
}

static void
sixel_kcenter_choose_random(unsigned int point_count,
                            unsigned int k,
                            uint32_t *rng_state,
                            unsigned int *centers,
                            unsigned int *scratch_indices)
{
    unsigned int index;
    unsigned int pick;
    unsigned int remaining;
    unsigned int temp;

    index = 0u;
    pick = 0u;
    remaining = 0u;
    temp = 0u;

    for (index = 0u; index < point_count; ++index) {
        scratch_indices[index] = index;
    }

    for (index = 0u; index < k; ++index) {
        remaining = point_count - index;
        pick = index + sixel_kcenter_rng_bounded(rng_state, remaining);
        temp = scratch_indices[index];
        scratch_indices[index] = scratch_indices[pick];
        scratch_indices[pick] = temp;
        centers[index] = scratch_indices[index];
    }
}

static int
sixel_kcenter_try_worst_swap(sixel_kcenter_swap_ctx_t *ctx)
{
    unsigned int point_count;
    unsigned int k;
    double const *points;
    double const *weights;
    unsigned int *centers;
    unsigned int *nearest_slot;
    double *nearest_dist;
    double *radius2_io;
    double *sse_io;
    unsigned int *scratch_slot;
    double *scratch_dist;
    unsigned int index;
    unsigned int worst_index;
    unsigned int slot;
    unsigned int old_center;
    double radius2;
    double sse;
    double lhs_weight;
    double rhs_weight;

    if (ctx == NULL) {
        return 0;
    }

    point_count = ctx->point_count;
    k = ctx->k;
    points = ctx->points;
    weights = ctx->weights;
    centers = ctx->centers;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    radius2_io = ctx->radius2_io;
    sse_io = ctx->sse_io;
    scratch_slot = ctx->scratch_slot;
    scratch_dist = ctx->scratch_dist;

    index = 0u;
    worst_index = 0u;
    slot = 0u;
    old_center = 0u;
    radius2 = 0.0;
    sse = 0.0;
    lhs_weight = 0.0;
    rhs_weight = 0.0;

    if (point_count == 0u || k == 0u) {
        return 0;
    }

    worst_index = 0u;
    for (index = 1u; index < point_count; ++index) {
        if (nearest_dist[index] > nearest_dist[worst_index] + 1.0e-12) {
            worst_index = index;
        } else if (nearest_dist[index] >= nearest_dist[worst_index]
                - 1.0e-12) {
            lhs_weight = (weights != NULL) ? weights[index] : 1.0;
            rhs_weight = (weights != NULL) ? weights[worst_index] : 1.0;
            if (lhs_weight > rhs_weight + 1.0e-12) {
                worst_index = index;
            } else if (lhs_weight >= rhs_weight - 1.0e-12
                    && index < worst_index) {
                worst_index = index;
            }
        }
    }

    slot = nearest_slot[worst_index];
    if (slot >= k || centers[slot] == worst_index) {
        return 0;
    }

    if (sixel_kcenter_is_center(centers, k, worst_index)) {
        return 0;
    }

    old_center = centers[slot];
    centers[slot] = worst_index;

    sixel_kcenter_assign_points(points,
                                weights,
                                point_count,
                                centers,
                                k,
                                scratch_slot,
                                scratch_dist,
                                &radius2,
                                &sse,
                                NULL,
                                NULL);

    if (radius2 < *radius2_io - 1.0e-12
            || (radius2 <= *radius2_io + 1.0e-12
                && sse < *sse_io - 1.0e-9)) {
        memcpy(nearest_slot,
               scratch_slot,
               (size_t)point_count * sizeof(unsigned int));
        memcpy(nearest_dist,
               scratch_dist,
               (size_t)point_count * sizeof(double));
        *radius2_io = radius2;
        *sse_io = sse;
        return 1;
    }

    centers[slot] = old_center;
    return 0;
}

static SIXELSTATUS
sixel_kcenter_run_solver(sixel_kcenter_solver_ctx_t *ctx)
{
    SIXELSTATUS status;
    double const *points;
    double const *weights;
    unsigned int point_count;
    unsigned int k;
    sixel_kcenter_algo_t resolved_algo;
    unsigned int iter_limit;
    uint32_t *rng_state;
    unsigned int *centers;
    unsigned int *nearest_slot;
    double *nearest_dist;
    unsigned int *scratch_slot;
    double *scratch_dist;
    double *radius2_out;
    double *sse_out;
    unsigned int *iterations_out;
    unsigned int *scratch_indices;
    double *fft_dist_cache;
    unsigned char *center_mask;
    unsigned int iterations;
    double radius2;
    double sse;
    sixel_kcenter_swap_ctx_t swap_ctx;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    points = ctx->points;
    weights = ctx->weights;
    point_count = ctx->point_count;
    k = ctx->k;
    resolved_algo = ctx->resolved_algo;
    iter_limit = ctx->iter_limit;
    rng_state = ctx->rng_state;
    centers = ctx->centers;
    nearest_slot = ctx->nearest_slot;
    nearest_dist = ctx->nearest_dist;
    scratch_slot = ctx->scratch_slot;
    scratch_dist = ctx->scratch_dist;
    radius2_out = ctx->radius2_out;
    sse_out = ctx->sse_out;
    iterations_out = ctx->iterations_out;
    scratch_indices = ctx->scratch_indices;
    fft_dist_cache = ctx->fft_dist_cache;
    center_mask = ctx->center_mask;

    status = SIXEL_OK;
    iterations = 0u;
    radius2 = 0.0;
    sse = 0.0;
    swap_ctx.points = points;
    swap_ctx.weights = weights;
    swap_ctx.point_count = point_count;
    swap_ctx.centers = centers;
    swap_ctx.k = k;
    swap_ctx.nearest_slot = nearest_slot;
    swap_ctx.nearest_dist = nearest_dist;
    swap_ctx.radius2_io = &radius2;
    swap_ctx.sse_io = &sse;
    swap_ctx.scratch_slot = scratch_slot;
    swap_ctx.scratch_dist = scratch_dist;

    if (point_count == 0u || k == 0u) {
        if (radius2_out != NULL) {
            *radius2_out = 0.0;
        }
        if (sse_out != NULL) {
            *sse_out = 0.0;
        }
        if (iterations_out != NULL) {
            *iterations_out = 0u;
        }
        return SIXEL_OK;
    }

    if (resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_SWAP) {
        sixel_kcenter_choose_random(point_count,
                                    k,
                                    rng_state,
                                    centers,
                                    scratch_indices);
    } else {
        sixel_kcenter_choose_fft(points,
                                 weights,
                                 point_count,
                                 k,
                                 rng_state,
                                 centers,
                                 fft_dist_cache,
                                 center_mask);
    }

    sixel_kcenter_assign_points(points,
                                weights,
                                point_count,
                                centers,
                                k,
                                nearest_slot,
                                nearest_dist,
                                &radius2,
                                &sse,
                                NULL,
                                NULL);

    if (resolved_algo != SIXEL_PALETTE_KCENTER_ALGO_FFT) {
        while (iterations < iter_limit) {
            if (!sixel_kcenter_try_worst_swap(&swap_ctx)) {
                break;
            }
            ++iterations;
        }
    }

    if (radius2_out != NULL) {
        *radius2_out = radius2;
    }
    if (sse_out != NULL) {
        *sse_out = sse;
    }
    if (iterations_out != NULL) {
        *iterations_out = iterations;
    }
    return status;
}

static sixel_kcenter_algo_t
sixel_kcenter_choose_auto_algo(int quality_mode,
                               unsigned int point_count)
{
    if (quality_mode == SIXEL_QUALITY_LOW || point_count > 2048u) {
        return SIXEL_PALETTE_KCENTER_ALGO_FFT;
    }
    return SIXEL_PALETTE_KCENTER_ALGO_HYBRID;
}

static SIXELSTATUS
build_palette_kcenter(sixel_kcenter_build_ctx_t *ctx)
{
    unsigned char **result;
    float **result_float32;
    unsigned char const *data;
    unsigned int length;
    unsigned int depth;
    unsigned int reqcolors;
    unsigned int *ncolors;
    unsigned int *origcolors;
    int quality_mode;
    int force_palette;
    int use_reversible;
    int final_merge_mode;
    sixel_allocator_t *allocator;
    int pixelformat;
    int treat_input_as_float32;
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    sixel_palette_telemetry_t *telemetry;
    SIXELSTATUS status;
    double *points;
    double *weights;
    unsigned int point_count;
    unsigned int visible_count;
    unsigned int k;
    unsigned int overshoot;
    unsigned int slot;
    unsigned int index;
    unsigned int channel;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    unsigned int seed;
    unsigned int restarts;
    unsigned int iter_limit;
    unsigned int histbits;
    unsigned int point_budget;
    unsigned int total_iterations;
    unsigned int best_iterations;
    unsigned int run_iterations;
    unsigned int resolved_merge;
    int apply_merge;
    sixel_kcenter_algo_t algo;
    sixel_kcenter_algo_t resolved_algo;
    uint32_t rng_state;
    unsigned int *centers;
    unsigned int *best_centers;
    unsigned int *work_centers;
    unsigned int *nearest_slot;
    unsigned int *scratch_slot;
    double *nearest_dist;
    double *scratch_dist;
    double *cluster_weights;
    double *cluster_sums;
    double *final_centers;
    double *sort_weights;
    unsigned int *order;
    unsigned int *scratch_indices;
    double *fft_dist_cache;
    unsigned char *center_mask;
    double radius2;
    double sse;
    double best_radius2;
    double best_sse;
    double prune_mass;
    unsigned long *merge_weights;
    double *merge_sums;
    int cluster_total;
    unsigned int final_count;
    double weight_value;
    double component;
    double restored_component;
    unsigned char *palette;
    unsigned char *grown_palette;
    float *float_palette;
    float *grown_float;
    int input_is_float32;
    double float32_channel_scale[3];
    double float32_channel_offset[3];
    int job_init;
    int job_iter;
    int job_merge;
    int job_export;
    char log_detail[160];
    double wall_start;
    double init_stop;
    double iterate_start;
    double iterate_stop;
    double merge_start;
    double merge_stop;
    double export_start;
    double export_stop;
    float float_minimum;
    float float_maximum;
    double range;
    double now;
    double init_span;
    double iterate_span;
    double merge_span;
    double export_span;
    int override_lock_acquired;
    sixel_kcenter_solver_ctx_t solver_ctx;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result = ctx->result;
    result_float32 = ctx->result_float32;
    data = ctx->data;
    length = ctx->length;
    depth = ctx->depth;
    reqcolors = ctx->reqcolors;
    ncolors = ctx->ncolors;
    origcolors = ctx->origcolors;
    quality_mode = ctx->quality_mode;
    force_palette = ctx->force_palette;
    use_reversible = ctx->use_reversible;
    final_merge_mode = ctx->final_merge_mode;
    allocator = ctx->allocator;
    pixelformat = ctx->pixelformat;
    treat_input_as_float32 = ctx->treat_input_as_float32;
    logger = ctx->logger;
    job_seq = ctx->job_seq;
    engine_name = ctx->engine_name;
    telemetry = ctx->telemetry;

    status = SIXEL_BAD_ARGUMENT;
    points = NULL;
    weights = NULL;
    point_count = 0u;
    visible_count = 0u;
    k = 0u;
    overshoot = 0u;
    slot = 0u;
    index = 0u;
    channel = 0u;
    fill = 0u;
    source = 0u;
    swap_temp = 0u;
    seed = 1u;
    restarts = 1u;
    iter_limit = 16u;
    histbits = 5u;
    point_budget = 0u;
    total_iterations = 0u;
    best_iterations = 0u;
    run_iterations = 0u;
    resolved_merge = SIXEL_FINAL_MERGE_NONE;
    apply_merge = 0;
    algo = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    resolved_algo = SIXEL_PALETTE_KCENTER_ALGO_AUTO;
    rng_state = 1u;
    centers = NULL;
    best_centers = NULL;
    work_centers = NULL;
    nearest_slot = NULL;
    scratch_slot = NULL;
    nearest_dist = NULL;
    scratch_dist = NULL;
    cluster_weights = NULL;
    cluster_sums = NULL;
    final_centers = NULL;
    sort_weights = NULL;
    order = NULL;
    scratch_indices = NULL;
    fft_dist_cache = NULL;
    center_mask = NULL;
    radius2 = 0.0;
    sse = 0.0;
    best_radius2 = 0.0;
    best_sse = 0.0;
    prune_mass = 0.995;
    merge_weights = NULL;
    merge_sums = NULL;
    cluster_total = 0;
    final_count = 0u;
    weight_value = 0.0;
    component = 0.0;
    restored_component = 0.0;
    palette = NULL;
    grown_palette = NULL;
    float_palette = NULL;
    grown_float = NULL;
    input_is_float32 = 0;
    float32_channel_scale[0] = 0.0;
    float32_channel_scale[1] = 0.0;
    float32_channel_scale[2] = 0.0;
    float32_channel_offset[0] = 0.0;
    float32_channel_offset[1] = 0.0;
    float32_channel_offset[2] = 0.0;
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
    float_minimum = 0.0f;
    float_maximum = 0.0f;
    range = 0.0;
    now = 0.0;
    init_span = 0.0;
    iterate_span = 0.0;
    merge_span = 0.0;
    export_span = 0.0;
    override_lock_acquired = 0;
    memset(&solver_ctx, 0, sizeof(solver_ctx));

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
#if HAVE_FLOAT_H
# define SIXEL_KCENTER_FLOAT_BOUND FLT_MAX
#else
# define SIXEL_KCENTER_FLOAT_BOUND 1.0e9f
#endif
            float_minimum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                -SIXEL_KCENTER_FLOAT_BOUND);
            float_maximum = sixel_pixelformat_float_channel_clamp(
                pixelformat,
                (int)channel,
                SIXEL_KCENTER_FLOAT_BOUND);
#undef SIXEL_KCENTER_FLOAT_BOUND
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
    }

    if (reqcolors == 0u) {
        reqcolors = 1u;
    }

    override_lock_acquired = sixel_kcenter_override_lock_acquire();

    job_init = sixel_palette_kcenter_log_start(logger,
                                               job_seq,
                                               engine_name,
                                               "palette/init",
                                               "init");

    algo = sixel_get_kcenter_algo();
    seed = (unsigned int)sixel_get_kcenter_seed();
    restarts = sixel_get_kcenter_restarts();
    iter_limit = sixel_get_kcenter_iter();
    histbits = sixel_get_kcenter_histbits();
    point_budget = sixel_get_kcenter_point_budget();
    prune_mass = sixel_get_kcenter_prune_mass();

    status = sixel_kcenter_collect_points(&points,
                                          &weights,
                                          &point_count,
                                          &visible_count,
                                          data,
                                          length,
                                          depth,
                                          pixelformat,
                                          treat_input_as_float32,
                                          histbits,
                                          point_budget,
                                          prune_mass,
                                          reqcolors,
                                          quality_mode,
                                          float32_channel_scale,
                                          float32_channel_offset,
                                          allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (origcolors != NULL) {
        *origcolors = visible_count;
    }
    if (point_count == 0u) {
        status = SIXEL_OK;
        goto end;
    }

    resolved_merge = (unsigned int)sixel_resolve_final_merge_mode(
        final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD);
    overshoot = reqcolors;
    if (apply_merge) {
        sixel_final_merge_load_env();
        overshoot = sixel_final_merge_target(reqcolors, (int)resolved_merge);
    }
    if (overshoot > point_count) {
        overshoot = point_count;
    }
    if (overshoot == 0u) {
        overshoot = 1u;
    }
    k = overshoot;

    centers = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    best_centers = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    work_centers = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(unsigned int));
    nearest_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    scratch_slot = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    nearest_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    scratch_dist = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    cluster_weights = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * sizeof(double));
    cluster_sums = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    final_centers = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)k * 3u * sizeof(double));
    scratch_indices = (unsigned int *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(unsigned int));
    fft_dist_cache = (double *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count * sizeof(double));
    center_mask = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)point_count);

    if (centers == NULL || best_centers == NULL || work_centers == NULL
            || nearest_slot == NULL || scratch_slot == NULL
            || nearest_dist == NULL || scratch_dist == NULL
            || cluster_weights == NULL || cluster_sums == NULL
            || final_centers == NULL || scratch_indices == NULL
            || fft_dist_cache == NULL || center_mask == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    resolved_algo = algo;
    if (resolved_algo == SIXEL_PALETTE_KCENTER_ALGO_AUTO) {
        resolved_algo = sixel_kcenter_choose_auto_algo(quality_mode,
                                                       point_count);
    }

    init_stop = sixel_timer_now();
    iterate_start = init_stop;
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "samples=%u k=%u algo=%s/%s seed=%u "
                                "histbits=%u budget=%u prune_mass=%.3f",
                                point_count,
                                k,
                                sixel_kcenter_algo_to_string(algo),
                                sixel_kcenter_algo_to_string(resolved_algo),
                                seed,
                                histbits,
                                point_budget,
                                prune_mass);
    sixel_palette_kcenter_log_finish(logger,
                                     job_init,
                                     engine_name,
                                     "palette/init",
                                     "init",
                                     log_detail);

    job_iter = sixel_palette_kcenter_log_start(logger,
                                               job_seq,
                                               engine_name,
                                               "palette/iterate",
                                               "iterate");

    solver_ctx.points = points;
    solver_ctx.weights = weights;
    solver_ctx.point_count = point_count;
    solver_ctx.k = k;
    solver_ctx.resolved_algo = resolved_algo;
    solver_ctx.iter_limit = iter_limit;
    solver_ctx.rng_state = &rng_state;
    solver_ctx.centers = work_centers;
    solver_ctx.nearest_slot = nearest_slot;
    solver_ctx.nearest_dist = nearest_dist;
    solver_ctx.scratch_slot = scratch_slot;
    solver_ctx.scratch_dist = scratch_dist;
    solver_ctx.radius2_out = &radius2;
    solver_ctx.sse_out = &sse;
    solver_ctx.iterations_out = &run_iterations;
    solver_ctx.scratch_indices = scratch_indices;
    solver_ctx.fft_dist_cache = fft_dist_cache;
    solver_ctx.center_mask = center_mask;

    best_radius2 = -1.0;
    best_sse = 0.0;
    best_iterations = 0u;
    for (slot = 0u; slot < restarts; ++slot) {
        rng_state = (uint32_t)(seed + 0x9e3779b9u * (slot + 1u));
        status = sixel_kcenter_run_solver(&solver_ctx);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (best_radius2 < 0.0
                || radius2 < best_radius2 - 1.0e-12
                || (radius2 <= best_radius2 + 1.0e-12
                    && sse < best_sse - 1.0e-9)) {
            memcpy(best_centers,
                   work_centers,
                   (size_t)k * sizeof(unsigned int));
            best_radius2 = radius2;
            best_sse = sse;
            best_iterations = run_iterations;
        }
    }
    memcpy(centers, best_centers, (size_t)k * sizeof(unsigned int));
    total_iterations = best_iterations;

    sixel_kcenter_assign_points(points,
                                weights,
                                point_count,
                                centers,
                                k,
                                nearest_slot,
                                nearest_dist,
                                &radius2,
                                &sse,
                                cluster_weights,
                                cluster_sums);

    for (slot = 0u; slot < k; ++slot) {
        final_centers[slot * 3u + 0u] = points[centers[slot] * 3u + 0u];
        final_centers[slot * 3u + 1u] = points[centers[slot] * 3u + 1u];
        final_centers[slot * 3u + 2u] = points[centers[slot] * 3u + 2u];
    }
    final_count = k;

    iterate_stop = sixel_timer_now();
    (void)sixel_compat_snprintf(log_detail,
                                sizeof(log_detail),
                                "algo=%s iter=%u radius=%.4f sse=%.4f",
                                sixel_kcenter_algo_to_string(resolved_algo),
                                total_iterations,
                                sqrt(best_radius2),
                                best_sse);
    sixel_palette_kcenter_log_finish(logger,
                                     job_iter,
                                     engine_name,
                                     "palette/iterate",
                                     "iterate",
                                     log_detail);

    merge_start = iterate_stop;
    merge_stop = iterate_stop;
    if (apply_merge && k > reqcolors) {
        merge_start = sixel_timer_now();
        job_merge = sixel_palette_kcenter_log_start(logger,
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
                component = cluster_sums[slot * 3u + channel]
                    / cluster_weights[slot];
                if (component < 0.0) {
                    component = 0.0;
                }
                if (component > 255.0) {
                    component = 255.0;
                }
                merge_sums[slot * 3u + channel]
                    = component * (double)merge_weights[slot];
            }
        }

        cluster_total = sixel_palette_apply_merge(merge_weights,
                                                  merge_sums,
                                                  3u,
                                                  (int)k,
                                                  (int)reqcolors,
                                                  (int)resolved_merge,
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
            for (channel = 0u; channel < 3u; ++channel) {
                final_centers[slot * 3u + channel]
                    = merge_sums[slot * 3u + channel] / weight_value;
            }
            cluster_weights[slot] = weight_value;
        }

        merge_stop = sixel_timer_now();
        (void)sixel_compat_snprintf(log_detail,
                                    sizeof(log_detail),
                                    "clusters=%u merge=%u",
                                    final_count,
                                    resolved_merge);
        sixel_palette_kcenter_log_finish(logger,
                                         job_merge,
                                         engine_name,
                                         "palette/merge",
                                         "merge",
                                         log_detail);
    }

    export_start = sixel_timer_now();
    job_export = sixel_palette_kcenter_log_start(logger,
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
            component = final_centers[slot * 3u + channel];
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            if (float_palette != NULL) {
                if (float32_channel_scale[channel] > 0.0) {
                    restored_component = component;
                    restored_component -= float32_channel_offset[channel];
                    restored_component /= float32_channel_scale[channel];
                } else {
                    restored_component = 0.0;
                }
                float_palette[slot * 3u + channel]
                    = sixel_pixelformat_float_channel_clamp(
                        pixelformat,
                        (int)channel,
                        (float)restored_component);
            }
            palette[slot * 3u + channel] = (unsigned char)(component + 0.5);
        }
    }

    if (force_palette && final_count < reqcolors && final_count > 0u) {
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
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (float_palette != NULL) {
            grown_float = (float *)sixel_allocator_malloc(
                allocator,
                (size_t)reqcolors * 3u * sizeof(float));
            if (grown_float == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        memcpy(grown_palette,
               palette,
               (size_t)final_count * 3u * sizeof(unsigned char));
        if (grown_float != NULL) {
            memcpy(grown_float,
                   float_palette,
                   (size_t)final_count * 3u * sizeof(float));
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
        grown_palette = NULL;
        if (float_palette != NULL) {
            sixel_allocator_free(allocator, float_palette);
            float_palette = grown_float;
            grown_float = NULL;
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
    sixel_palette_kcenter_log_finish(logger,
                                     job_export,
                                     engine_name,
                                     "palette/export",
                                     "export",
                                     log_detail);

    status = SIXEL_OK;

end:
    if (telemetry != NULL) {
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
        telemetry->merge_iterate_count
            = (apply_merge && k > reqcolors) ? 1u : 0u;
        telemetry->merge_mode = (apply_merge && k > reqcolors)
            ? (int)resolved_merge
            : SIXEL_FINAL_MERGE_NONE;
    }

    if (grown_float != NULL) {
        sixel_allocator_free(allocator, grown_float);
    }
    if (grown_palette != NULL) {
        sixel_allocator_free(allocator, grown_palette);
    }
    if (float_palette != NULL) {
        sixel_allocator_free(allocator, float_palette);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (merge_sums != NULL) {
        sixel_allocator_free(allocator, merge_sums);
    }
    if (merge_weights != NULL) {
        sixel_allocator_free(allocator, merge_weights);
    }
    if (center_mask != NULL) {
        sixel_allocator_free(allocator, center_mask);
    }
    if (fft_dist_cache != NULL) {
        sixel_allocator_free(allocator, fft_dist_cache);
    }
    if (scratch_indices != NULL) {
        sixel_allocator_free(allocator, scratch_indices);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (sort_weights != NULL) {
        sixel_allocator_free(allocator, sort_weights);
    }
    if (final_centers != NULL) {
        sixel_allocator_free(allocator, final_centers);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weights != NULL) {
        sixel_allocator_free(allocator, cluster_weights);
    }
    if (scratch_dist != NULL) {
        sixel_allocator_free(allocator, scratch_dist);
    }
    if (nearest_dist != NULL) {
        sixel_allocator_free(allocator, nearest_dist);
    }
    if (scratch_slot != NULL) {
        sixel_allocator_free(allocator, scratch_slot);
    }
    if (nearest_slot != NULL) {
        sixel_allocator_free(allocator, nearest_slot);
    }
    if (work_centers != NULL) {
        sixel_allocator_free(allocator, work_centers);
    }
    if (best_centers != NULL) {
        sixel_allocator_free(allocator, best_centers);
    }
    if (centers != NULL) {
        sixel_allocator_free(allocator, centers);
    }
    if (weights != NULL) {
        sixel_allocator_free(allocator, weights);
    }
    if (points != NULL) {
        sixel_allocator_free(allocator, points);
    }
    sixel_kcenter_override_lock_release(override_lock_acquired);
    return status;
}

static SIXELSTATUS
sixel_palette_build_kcenter_internal(sixel_kcenter_internal_ctx_t *ctx)
{
    sixel_palette_t *palette;
    unsigned char const *data;
    unsigned int length;
    int pixelformat;
    sixel_allocator_t *allocator;
    sixel_logger_t *logger;
    int *job_seq;
    char const *engine_name;
    int treat_input_as_float32;
    sixel_palette_telemetry_t *telemetry;
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
    sixel_kcenter_build_ctx_t *build_ctx;

    if (ctx == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = ctx->palette;
    data = ctx->data;
    length = ctx->length;
    pixelformat = ctx->pixelformat;
    allocator = ctx->allocator;
    logger = ctx->logger;
    job_seq = ctx->job_seq;
    engine_name = ctx->engine_name;
    treat_input_as_float32 = ctx->treat_input_as_float32;
    telemetry = ctx->telemetry;

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
    build_ctx = NULL;

    if (palette == NULL) {
        return status;
    }
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return status;
    }

    build_ctx = (sixel_kcenter_build_ctx_t *)sixel_allocator_malloc(
        work_allocator,
        sizeof(sixel_kcenter_build_ctx_t));
    if (build_ctx == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(build_ctx, 0, sizeof(sixel_kcenter_build_ctx_t));

    depth_result = sixel_helper_compute_depth(pixelformat);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kcenter: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    input_depth = (unsigned int)depth_result;

    depth_result = sixel_helper_compute_depth(SIXEL_PIXELFORMAT_RGB888);
    if (depth_result <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_build_kcenter: rgb888 depth lookup failed.");
        return SIXEL_BAD_ARGUMENT;
    }
    entry_depth = (unsigned int)depth_result;

    build_ctx->result = &entries;
    build_ctx->result_float32 = &entries_float32;
    build_ctx->data = data;
    build_ctx->length = length;
    build_ctx->depth = input_depth;
    build_ctx->reqcolors = palette->requested_colors;
    build_ctx->ncolors = &ncolors;
    build_ctx->origcolors = &origcolors;
    build_ctx->quality_mode = palette->quality_mode;
    build_ctx->force_palette = palette->force_palette;
    build_ctx->use_reversible = palette->use_reversible;
    build_ctx->final_merge_mode = palette->final_merge_mode;
    build_ctx->allocator = work_allocator;
    build_ctx->pixelformat = pixelformat;
    build_ctx->treat_input_as_float32 = treat_input_as_float32;
    build_ctx->logger = logger;
    build_ctx->job_seq = job_seq;
    build_ctx->engine_name = engine_name;
    build_ctx->telemetry = telemetry;

    status = build_palette_kcenter(build_ctx);
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
                "sixel_palette_build_kcenter: palette payload is missing.");
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
    if (build_ctx != NULL) {
        sixel_allocator_free(work_allocator, build_ctx);
    }
    if (entries_float32 != NULL) {
        sixel_allocator_free(work_allocator, entries_float32);
    }
    if (entries != NULL) {
        sixel_allocator_free(work_allocator, entries);
    }
    return status;
}

SIXELSTATUS
sixel_palette_build_kcenter(sixel_palette_t *palette,
                            unsigned char const *data,
                            unsigned int length,
                            int pixelformat,
                            sixel_allocator_t *allocator,
                            sixel_logger_t *logger,
                            int *job_seq,
                            char const *engine_name,
                            sixel_palette_telemetry_t *telemetry)
{
    sixel_kcenter_internal_ctx_t internal_ctx;

    memset(&internal_ctx, 0, sizeof(internal_ctx));
    internal_ctx.palette = palette;
    internal_ctx.data = data;
    internal_ctx.length = length;
    internal_ctx.pixelformat = pixelformat;
    internal_ctx.allocator = allocator;
    internal_ctx.logger = logger;
    internal_ctx.job_seq = job_seq;
    internal_ctx.engine_name = engine_name;
    internal_ctx.treat_input_as_float32 = 0;
    internal_ctx.telemetry = telemetry;
    return sixel_palette_build_kcenter_internal(&internal_ctx);
}

SIXELSTATUS
sixel_palette_build_kcenter_float32(sixel_palette_t *palette,
                                    float const *data,
                                    unsigned int length,
                                    int pixelformat,
                                    sixel_allocator_t *allocator,
                                    sixel_logger_t *logger,
                                    int *job_seq,
                                    char const *engine_name,
                                    sixel_palette_telemetry_t *telemetry)
{
    sixel_kcenter_internal_ctx_t internal_ctx;

    memset(&internal_ctx, 0, sizeof(internal_ctx));
    internal_ctx.palette = palette;
    internal_ctx.data = (unsigned char const *)data;
    internal_ctx.length = length;
    internal_ctx.pixelformat = pixelformat;
    internal_ctx.allocator = allocator;
    internal_ctx.logger = logger;
    internal_ctx.job_seq = job_seq;
    internal_ctx.engine_name = engine_name;
    internal_ctx.treat_input_as_float32 = 1;
    internal_ctx.telemetry = telemetry;
    return sixel_palette_build_kcenter_internal(&internal_ctx);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
