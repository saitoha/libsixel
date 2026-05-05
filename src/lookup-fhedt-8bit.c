/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Voronoi + 3D EDT lookup implementation for 8bit pixel buffers.  This module
 * builds a dense 3D grid using the Felzenszwalb–Huttenlocher 1D EDT in three
 * passes (X -> Y -> Z).  Each voxel stores the index of the nearest palette
 * entry plus a boundary bitmask so callers can cheaply decide whether a
 * refinement pass is required.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_FLOAT_H
# include <float.h>
#endif  /* HAVE_FLOAT_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_MATH_H
#include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_STDDEF_H
# include <stddef.h>
#endif  /* HAVE_STDDEF_H */
#if HAVE_STDINT_H
# include <stdint.h>
#endif  /* HAVE_STDINT_H */
#include <sixel.h>

#include "cpu.h"
#include "allocator.h"
#include "compat_stub.h"
#include "lookup-fhedt-8bit.h"
#include "timeline-logger.h"
#include "threading.h"
#include "threadpool.h"
#include "sixel_atomic.h"
#include "status.h"

#ifndef SIXEL_FHEDT_TLS
# if defined(SIXEL_ENABLE_THREADS)
   /*
    * TinyCC reports TLS keywords but does not provide stable semantics for
    * the FHEDT worker scratch buffers. Keep it on the non-TLS fallback path
    * so resolve_threads() forces a single worker and avoids data races.
    */
#  if defined(_MSC_VER)
#   define SIXEL_FHEDT_TLS __declspec(thread)
#   define SIXEL_FHEDT_TLS_AVAILABLE 1
#  elif !defined(__STDC_NO_THREADS__) && !defined(__PCC__) \
      && !defined(__TINYC__)
#   define SIXEL_FHEDT_TLS _Thread_local
#   define SIXEL_FHEDT_TLS_AVAILABLE 1
#  elif defined(__GNUC__) && !defined(__PCC__) && !defined(__TINYC__)
#   define SIXEL_FHEDT_TLS __thread
#   define SIXEL_FHEDT_TLS_AVAILABLE 1
#  else
#   define SIXEL_FHEDT_TLS
#   define SIXEL_FHEDT_TLS_AVAILABLE 0
#  endif
# else
#  define SIXEL_FHEDT_TLS
#  define SIXEL_FHEDT_TLS_AVAILABLE 0
# endif
#endif

/*
 * The shared object is immutable after construction so workers can reference
 * the same LUT concurrently.  A simple reference counter governs lifetime; the
 * parent FHEDT handle owns one reference while any shared handoff can bump the
 * count before publishing the pointer.
 */
struct sixel_lookup_fhedt_shared_8bit {
    sixel_atomic_u32_t refcount;
    int resolution;
    int refine;
    int use_dist2;
    int weights[3];
    int ncolors;
    int depth;
    int res_shift;
    double safe_radius2;
    int use_u16;
    unsigned char *palette;
    float *dist2;
    uint8_t *indices8;
    uint16_t *indices16;
    unsigned char *boundary;
    uint32_t signature;
};

static int const sixel_lookup_fhedt_resolution_min_8bit = 64;
static int const sixel_lookup_fhedt_resolution_max_8bit = 256;
static int const sixel_lookup_fhedt_tile_xy_default_8bit = 8;
static int const sixel_lookup_fhedt_tile_depth_default_8bit = 8;

/*
 * Pick FHEDT tile defaults based on palette complexity so dense color sets
 * prefer smaller tiles (better cache reuse) while biased palettes can run
 * with fewer, larger tiles to reduce scheduling overhead.  Environment
 * variables still override the result.
 */
static void
sixel_lookup_fhedt_choose_tile_defaults_8bit(unsigned char const *palette,
                                       int ncolors,
                                       int depth,
                                       int *tile_xy_default,
                                       int *tile_depth_default)
{
    int component;
    int color;
    int offset;
    int min_component[3];
    int max_component[3];
    long sum_component[3];
    long deviation_sum[3];
    int tile_xy;
    int tile_depth;
    int span_sum;
    double mean_component[3];
    double mean_deviation;

    tile_xy = sixel_lookup_fhedt_tile_xy_default_8bit;
    tile_depth = sixel_lookup_fhedt_tile_depth_default_8bit;

    if (palette == NULL || ncolors <= 0 || depth <= 0) {
        *tile_xy_default = tile_xy;
        *tile_depth_default = tile_depth;

        return;
    }

    for (component = 0; component < 3; ++component) {
        min_component[component] = 255;
        max_component[component] = 0;
        sum_component[component] = 0L;
        deviation_sum[component] = 0L;
    }

    for (color = 0; color < ncolors; ++color) {
        offset = color * depth;
        for (component = 0; component < 3 && component < depth;
             ++component) {
            int value;

            value = palette[offset + component];
            if (value < min_component[component]) {
                min_component[component] = value;
            }
            if (value > max_component[component]) {
                max_component[component] = value;
            }
            sum_component[component] += (long)value;
        }
    }

    span_sum = 0;
    for (component = 0; component < 3 && component < depth; ++component) {
        mean_component[component] = (double)sum_component[component]
                                    / (double)ncolors;
        span_sum += max_component[component] - min_component[component];
    }

    for (color = 0; color < ncolors; ++color) {
        offset = color * depth;
        for (component = 0; component < 3 && component < depth;
             ++component) {
            int value;

            value = palette[offset + component];
            deviation_sum[component] += (long)labs(
                value - (long)mean_component[component]);
        }
    }

    mean_deviation = 0.0;
    for (component = 0; component < 3 && component < depth; ++component) {
        mean_deviation += (double)deviation_sum[component]
                          / (double)ncolors;
    }
    mean_deviation /= (double)(component > 0 ? component : 1);

    if (span_sum > 480 || ncolors > 512) {
        tile_xy = 6;
        tile_depth = 6;
    } else if (span_sum < 96 && ncolors < 64) {
        tile_xy = 12;
        tile_depth = 10;
    } else if (span_sum < 192 && ncolors < 128) {
        tile_xy = 10;
        tile_depth = 9;
    } else if (ncolors > 256) {
        tile_xy = 7;
        tile_depth = 7;
    }

    if (mean_deviation < 20.0 && tile_xy < 12) {
        tile_xy += 1;
        tile_depth += 1;
    }

    *tile_xy_default = tile_xy;
    *tile_depth_default = tile_depth;
}

static int
sixel_lookup_fhedt_pow2_log_8bit(int value)
{
    int shift;

    shift = 0;
    while ((1 << shift) < value && shift < 8) {
        ++shift;
    }

    return shift;
}

static int
sixel_lookup_fhedt_validate_resolution_8bit(int resolution)
{
    int shift;
    int pow2;

    if (resolution < sixel_lookup_fhedt_resolution_min_8bit
        || resolution > sixel_lookup_fhedt_resolution_max_8bit) {
        return 0;
    }

    shift = sixel_lookup_fhedt_pow2_log_8bit(resolution);
    pow2 = 1 << shift;

    return pow2 == resolution;
}

/*
 * Prefer the new SIXEL_LOOKUP_* knobs while still honoring the previous
 * SIXEL_FHEDT_* names for compatibility.
 */
static char const *
sixel_lookup_fhedt_getenv_8bit(char const *primary, char const *legacy)
{
    char const *value;

    value = sixel_compat_getenv(primary);
    if (value != NULL && value[0] != '\0') {
        return value;
    }

    value = sixel_compat_getenv(legacy);
    if (value != NULL && value[0] != '\0') {
        return value;
    }

    return NULL;
}

/*
 * Accept tile dimensions from the environment so profiling runs can adjust
 * cache locality without recompiling. Values outside [1, resolution] fall
 * back to the default 8x8x8 layout.
 */
static int
sixel_lookup_fhedt_parse_positive_8bit(char const *env_name,
                                 char const *legacy_name,
                                 int fallback)
{
    char const *env;
    char *endptr;
    long value;

    env = sixel_lookup_fhedt_getenv_8bit(env_name, legacy_name);
    if (env == NULL) {
        return fallback;
    }

    errno = 0;
    value = strtol(env, &endptr, 10);
    if (errno != 0 || endptr == env || value < 1 || value > 1024) {
        return fallback;
    }

    return (int)value;
}

static void
sixel_lookup_fhedt_resolve_tiles_8bit(unsigned char const *palette,
                                int ncolors,
                                int depth,
                                int res,
                                int *tile_xy,
                                int *tile_depth)
{
    int resolved_xy;
    int resolved_depth;
    int default_xy;
    int default_depth;

    sixel_lookup_fhedt_choose_tile_defaults_8bit(palette,
                                           ncolors,
                                           depth,
                                           &default_xy,
                                           &default_depth);

    resolved_xy = sixel_lookup_fhedt_parse_positive_8bit(
        "SIXEL_LOOKUP_FHEDT_TILE_XY",
        "SIXEL_FHEDT_TILE_XY",
        default_xy);
    resolved_depth = sixel_lookup_fhedt_parse_positive_8bit(
        "SIXEL_LOOKUP_FHEDT_TILE_DEPTH",
        "SIXEL_FHEDT_TILE_DEPTH",
        default_depth);
    if (resolved_xy > res) {
        resolved_xy = res;
    }
    if (resolved_depth > res) {
        resolved_depth = res;
    }

    *tile_xy = resolved_xy;
    *tile_depth = resolved_depth;
}

/* Keep intentional modulo hash arithmetic out of unsigned-overflow reports. */
static uint32_t
sixel_lookup_fhedt_add_u32_low_8bit(uint32_t left, uint32_t right)
{
    return (uint32_t)((uint64_t)left + right);
}

static uint32_t
sixel_lookup_fhedt_mix_u32_8bit(uint32_t state, uint32_t value)
{
    uint32_t mixed;

    mixed = 0U;
    mixed = sixel_lookup_fhedt_add_u32_low_8bit(value, 0x9e3779b9U);
    mixed = sixel_lookup_fhedt_add_u32_low_8bit(mixed, state << 6);
    mixed = sixel_lookup_fhedt_add_u32_low_8bit(mixed, state >> 2);
    state ^= mixed;

    return state;
}

typedef struct sixel_lookup_fhedt_cache_set_8bit {
    uint32_t key[4];
    int value[4];
    uint8_t hand;
} sixel_lookup_fhedt_cache_set_8bit_t;

typedef struct sixel_lookup_fhedt_cache_8bit {
    sixel_lookup_fhedt_cache_set_8bit_t sets[16];
    uint32_t signature;
    sixel_lookup_fhedt_shared_8bit_t const *shared;
} sixel_lookup_fhedt_cache_8bit_t;

typedef struct sixel_lookup_fhedt_timeline_8bit {
    int initialized;
    int log_lines;
    int line_stride;
    sixel_timeline_logger_t *logger;
} sixel_lookup_fhedt_timeline_8bit_t;

typedef void (*sixel_lookup_fhedt_edt1d_fn_8bit)(double *, int *, int, double);

/*
 * Thread-local gather/scatter buffer that keeps line data contiguous for
 * upcoming SIMD work.  The buffer is shared across passes to avoid repeated
 * allocation and to keep the hot paths short.
 */
typedef struct sixel_lookup_fhedt_line_buffer_8bit {
    double dist[256];
    int src[256];
} sixel_lookup_fhedt_line_buffer_8bit_t;

typedef struct sixel_lookup_fhedt_first_touch_plan_8bit {
    double *distances;
    int *sources;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_y;
    int tile_z;
    int tiles_y;
    int tiles_z;
} sixel_lookup_fhedt_first_touch_plan_8bit_t;

#if SIXEL_FHEDT_TLS_AVAILABLE
static SIXEL_FHEDT_TLS sixel_lookup_fhedt_line_buffer_8bit_t
    sixel_lookup_fhedt_tls_line_buffer_8bit;
#else
static sixel_lookup_fhedt_line_buffer_8bit_t
    sixel_lookup_fhedt_tls_line_buffer_8bit;
#endif

static SIXEL_FHEDT_TLS sixel_lookup_fhedt_cache_8bit_t
    sixel_lookup_fhedt_thread_cache_8bit;

/*
 * Resolve the number of worker threads available for FHEDT construction while
 * refusing to run parallel jobs when thread-local storage is unavailable.
 */
static int
sixel_lookup_fhedt_resolve_threads_8bit(void)
{
#if SIXEL_ENABLE_THREADS
    int threads;

    threads = sixel_threads_resolve();
    if (threads < 1) {
        threads = 1;
    }
# if SIXEL_FHEDT_TLS_AVAILABLE == 0
    threads = 1;
# endif

    return threads;
#else
    return 1;
#endif  /* SIXEL_ENABLE_THREADS */
}

static int
sixel_lookup_fhedt_pin_threads_enabled_8bit(void)
{
    char const *env;

    env = sixel_lookup_fhedt_getenv_8bit("SIXEL_LOOKUP_FHEDT_PIN_THREADS",
                                   "SIXEL_FHEDT_PIN_THREADS");
    if (env == NULL) {
        return 0;
    }

    return env[0] != '0';
}

static int
sixel_lookup_fhedt_first_touch_enabled_8bit(void)
{
    char const *env;

    env = sixel_lookup_fhedt_getenv_8bit("SIXEL_LOOKUP_FHEDT_FIRST_TOUCH",
                                   "SIXEL_FHEDT_FIRST_TOUCH");
    if (env == NULL) {
        return 0;
    }

    return env[0] != '0';
}

static void sixel_lookup_fhedt_dispatch_tiles_8bit(int total_tiles,
                                             int threads,
                                             int pin_threads,
                                             tp_worker_fn worker,
                                             void *plan);

static uint32_t
sixel_lookup_fhedt_cache_hash_8bit(size_t offset)
{
    uint32_t state;
    uint64_t offset64;

    /*
     * Hash the full pointer-sized offset using a 64bit staging value so that
     * the high-half mix stays well-defined on 32bit builds.
     */
    offset64 = (uint64_t)offset;

    state = sixel_lookup_fhedt_mix_u32_8bit(0x811c9dc5U,
                                      (uint32_t)offset64);
    state = sixel_lookup_fhedt_mix_u32_8bit(state,
                                      (uint32_t)(offset64 >> 32));

    return state;
}

static void
sixel_lookup_fhedt_cache_clear_8bit(sixel_lookup_fhedt_cache_8bit_t *cache)
{
    size_t set;
    size_t way;

    cache->signature = 0U;
    cache->shared = NULL;
    for (set = 0U; set < 16U; ++set) {
        cache->sets[set].hand = 0U;
        for (way = 0U; way < 4U; ++way) {
            cache->sets[set].key[way] = UINT32_MAX;
            cache->sets[set].value[way] = -1;
        }
    }
}

static void
sixel_lookup_fhedt_cache_prepare_8bit(
    sixel_lookup_fhedt_shared_8bit_t const *shared)
{
    if (sixel_lookup_fhedt_thread_cache_8bit.shared != shared
        || sixel_lookup_fhedt_thread_cache_8bit.signature != shared->signature) {
        sixel_lookup_fhedt_cache_clear_8bit(
            &sixel_lookup_fhedt_thread_cache_8bit);
        sixel_lookup_fhedt_thread_cache_8bit.shared = shared;
        sixel_lookup_fhedt_thread_cache_8bit.signature = shared->signature;
    }
}

static void
sixel_lookup_fhedt_timeline_open_8bit(
    sixel_lookup_fhedt_timeline_8bit_t *timeline);

static int
sixel_lookup_fhedt_timeline_lines_enabled_8bit(
    sixel_lookup_fhedt_timeline_8bit_t *timeline)
{
#if SIXEL_ENABLE_THREADS
    if (timeline == NULL || !timeline->initialized) {
        return 0;
    }
    if (timeline->logger == NULL ||
            !timeline->log_lines) {
        return 0;
    }
    return 1;
#else
    (void)timeline;
    return 0;
#endif  /* SIXEL_ENABLE_THREADS */
}

static void
sixel_lookup_fhedt_timeline_open_8bit(
    sixel_lookup_fhedt_timeline_8bit_t *timeline)
{
#if SIXEL_ENABLE_THREADS
    char const *line_env;
    long stride;

    if (timeline == NULL || timeline->initialized) {
        return;
    }
    timeline->logger = NULL;
    (void)sixel_timeline_logger_prepare_env(NULL, &timeline->logger);
    timeline->log_lines = 0;
    timeline->line_stride = 1;
    line_env = sixel_compat_getenv("SIXEL_LOG_LINES");
    if (line_env != NULL && line_env[0] != '\0') {
        stride = strtol(line_env, NULL, 10);
        if (stride < 1L) {
            stride = 1L;
        }
        timeline->log_lines = 1;
        timeline->line_stride = (int)stride;
    }
    timeline->initialized = 1;
#else
    (void)timeline;
#endif  /* SIXEL_ENABLE_THREADS */
}

static void
sixel_lookup_fhedt_timeline_close_8bit(
    sixel_lookup_fhedt_timeline_8bit_t *timeline)
{
#if SIXEL_ENABLE_THREADS
    if (timeline == NULL || !timeline->initialized) {
        return;
    }
    sixel_timeline_logger_unref(timeline->logger);
    timeline->logger = NULL;
#else
    (void)timeline;
#endif  /* SIXEL_ENABLE_THREADS */
}

static void
sixel_lookup_fhedt_timeline_log_8bit(
    sixel_lookup_fhedt_timeline_8bit_t *timeline,
    char const *worker,
    char const *event,
    int tile,
    int line,
    char const *message)
{
#if SIXEL_ENABLE_THREADS
    int skip_line;

    if (timeline == NULL || !timeline->initialized
            || timeline->logger == NULL) {
        return;
    }

    skip_line = 0;
    if (event != NULL && (strcmp(event, "line-start") == 0
            || strcmp(event, "line-end") == 0)) {
        if (!timeline->log_lines) {
            skip_line = 1;
        } else if (timeline->line_stride > 1 && tile >= 0
                   && (tile % timeline->line_stride) != 0) {
            skip_line = 1;
        }
    }
    if (skip_line) {
        return;
    }
    /*
     * The FHEDT builder does not use row ranges; we pass zero to keep the
     * JSON layout stable for tools/timeline.py consumers.
     */
    sixel_timeline_logger_logf(timeline->logger,
                      "fhedt",
                      worker,
                      event,
                      tile,
                      line,
                      0,
                      0,
                      0,
                      0,
                      "%s",
                      message != NULL ? message : "");
#else
    (void)timeline;
    (void)worker;
    (void)event;
    (void)tile;
    (void)line;
    (void)message;
#endif  /* SIXEL_ENABLE_THREADS */
}

static sixel_lookup_fhedt_line_buffer_8bit_t *
sixel_lookup_fhedt_line_buffer_get_8bit(void)
{
    sixel_lookup_fhedt_line_buffer_8bit_t *buffer;

    buffer = &sixel_lookup_fhedt_tls_line_buffer_8bit;

    return buffer;
}

/*
 * Prefetch the next line along the current axis so the gather phase hits a
 * warm cache.  When timeline logging is active, emit an explicit marker so
 * tools/timeline.py can visualize where prefetching happens.
 */
static void
sixel_lookup_fhedt_prefetch_line_8bit(double *distances,
                                int *sources,
                                size_t offset,
                                sixel_lookup_fhedt_timeline_8bit_t *timeline,
                                char const *worker,
                                int tile,
                                int line)
{
#if SIXEL_ENABLE_THREADS
    int skip_line;
    char message[64];
#endif
#if HAVE_BUILTIN_PREFETCH
    __builtin_prefetch(distances + offset, 0, 3);
    __builtin_prefetch(sources + offset, 0, 3);
#else
    (void)distances;
    (void)sources;
    (void)offset;
#endif
#if SIXEL_ENABLE_THREADS
    if (timeline != NULL && timeline->initialized
            && timeline->logger != NULL) {
        skip_line = 0;
        /*
         * Skip logging unless line-level events are explicitly enabled.
         * This keeps the prefetch hook out of the hot loop when only
         * coarse timeline markers are needed.
         */
        if (!timeline->log_lines) {
            skip_line = 1;
        } else if (timeline->line_stride > 1 && tile >= 0
                   && (tile % timeline->line_stride) != 0) {
            skip_line = 1;
        }
        if (skip_line) {
            return;
        }

        (void)snprintf(message, sizeof(message),
                       "prefetch@%zu", offset);
        sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                       worker,
                                       "prefetch",
                                       tile,
                                       line,
                                       message);
    }
#else
    (void)timeline;
    (void)worker;
    (void)tile;
    (void)line;
#endif
}

static int
sixel_lookup_fhedt_cache_get_8bit(sixel_lookup_fhedt_cache_8bit_t *cache,
                            size_t offset,
                            int *value_out)
{
    uint32_t key;
    size_t set;
    size_t way;

    key = sixel_lookup_fhedt_cache_hash_8bit(offset);
    set = (size_t)(key & 15U);
    for (way = 0U; way < 4U; ++way) {
        if (cache->sets[set].key[way] == key) {
            *value_out = cache->sets[set].value[way];

            return 1;
        }
    }

    return 0;
}

static void
sixel_lookup_fhedt_cache_put_8bit(sixel_lookup_fhedt_cache_8bit_t *cache,
                            size_t offset,
                            int value)
{
    uint32_t key;
    size_t set;
    size_t way;

    key = sixel_lookup_fhedt_cache_hash_8bit(offset);
    set = (size_t)(key & 15U);
    way = cache->sets[set].hand;
    cache->sets[set].key[way] = key;
    cache->sets[set].value[way] = value;
    cache->sets[set].hand = (uint8_t)((way + 1U) & 3U);
}

uint32_t
sixel_lookup_fhedt_8bit_signature(unsigned char const *palette,
                                 int ncolors,
                                 int resolution,
                                 int refine,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int depth)
{
    uint32_t hash;
    size_t total;
    size_t index;

    hash = 0x811c9dc5U;
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)resolution);
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)refine);
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)ncolors);
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)depth);
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)wcomp1);
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)wcomp2);
    hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)wcomp3);

    total = (size_t)ncolors * (size_t)depth;
    for (index = 0U; index < total; ++index) {
        hash = sixel_lookup_fhedt_mix_u32_8bit(hash, (uint32_t)palette[index]);
    }

    return hash;
}

uint32_t
sixel_lookup_fhedt_8bit_shared_signature(
    sixel_lookup_fhedt_shared_8bit_t const *shared)
{
    if (shared == NULL) {
        return 0U;
    }

    return shared->signature;
}

void
sixel_lookup_fhedt_8bit_shared_set_signature(
    sixel_lookup_fhedt_shared_8bit_t *shared,
    uint32_t signature)
{
    if (shared == NULL) {
        return;
    }

    shared->signature = signature;
}

static void
sixel_lookup_fhedt_shared_release_palette_8bit(
    sixel_allocator_t *allocator,
    sixel_lookup_fhedt_shared_8bit_t *shared)
{
    if (shared->palette != NULL) {
        sixel_allocator_free(allocator, shared->palette);
        shared->palette = NULL;
    }
}

static void
sixel_lookup_fhedt_shared_release_indices_8bit(
    sixel_allocator_t *allocator,
    sixel_lookup_fhedt_shared_8bit_t *shared)
{
    if (shared->dist2 != NULL) {
        sixel_allocator_free(allocator, shared->dist2);
        shared->dist2 = NULL;
    }
    if (shared->indices8 != NULL) {
        sixel_allocator_free(allocator, shared->indices8);
        shared->indices8 = NULL;
    }
    if (shared->indices16 != NULL) {
        sixel_allocator_free(allocator, shared->indices16);
        shared->indices16 = NULL;
    }
    if (shared->boundary != NULL) {
        sixel_allocator_free(allocator, shared->boundary);
        shared->boundary = NULL;
    }
}

static void
sixel_lookup_fhedt_shared_destroy_8bit(sixel_allocator_t *allocator,
                                 sixel_lookup_fhedt_shared_8bit_t *shared)
{
    if (shared == NULL) {
        return;
    }

    sixel_lookup_fhedt_shared_release_palette_8bit(allocator, shared);
    sixel_lookup_fhedt_shared_release_indices_8bit(allocator, shared);
    sixel_allocator_free(allocator, shared);
}

static void
sixel_lookup_fhedt_shared_unref_8bit(sixel_allocator_t *allocator,
                               sixel_lookup_fhedt_shared_8bit_t *shared)
{
    unsigned int previous;

    if (shared == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&shared->refcount, 1U);
    if (previous == 1U) {
        sixel_fence_acquire();
        sixel_lookup_fhedt_shared_destroy_8bit(allocator, shared);
    }
}

static void
sixel_lookup_fhedt_shared_ref_8bit(sixel_lookup_fhedt_shared_8bit_t *shared)
{
    if (shared == NULL) {
        return;
    }

    sixel_atomic_fetch_add_u32(&shared->refcount, 1U);
}

static int
sixel_lookup_fhedt_palette_index_8bit(int depth, int index, int component)
{
    return index * depth + component;
}

static void
sixel_lookup_fhedt_quantize_palette_8bit(unsigned char const *palette,
                                   sixel_lookup_fhedt_shared_8bit_t *shared)
{
    int index;
    int component;
    int res_shift;
    int channel;

    res_shift = shared->res_shift;
    for (index = 0; index < shared->ncolors; ++index) {
        for (component = 0; component < shared->depth; ++component) {
            channel = palette[sixel_lookup_fhedt_palette_index_8bit(
                                   shared->depth,
                                   index,
                                   component)];
            channel >>= res_shift;
            if (channel >= shared->resolution) {
                channel = shared->resolution - 1;
            }
            shared->palette[sixel_lookup_fhedt_palette_index_8bit(
                                   shared->depth,
                                   index,
                                   component)]
                = (unsigned char)channel;
        }
    }
}

static int
sixel_lookup_fhedt_first_touch_worker_8bit(tp_job_t job,
                                     void *userdata,
                                     void *workspace)
{
    sixel_lookup_fhedt_first_touch_plan_8bit_t *plan;
    int tile_index;
    int tile_z_index;
    int tile_y_index;
    int z_start;
    int z_end;
    int y_start;
    int y_end;
    int z;
    int y;
    int x;
    size_t offset;

    (void)workspace;

    plan = (sixel_lookup_fhedt_first_touch_plan_8bit_t *)userdata;
    tile_index = job.band_index;
    tile_z_index = tile_index / plan->tiles_y;
    tile_y_index = tile_index - (tile_z_index * plan->tiles_y);
    z_start = tile_z_index * plan->tile_z;
    z_end = z_start + plan->tile_z;
    if (z_end > plan->res) {
        z_end = plan->res;
    }
    y_start = tile_y_index * plan->tile_y;
    y_end = y_start + plan->tile_y;
    if (y_end > plan->res) {
        y_end = plan->res;
    }

    for (z = z_start; z < z_end; ++z) {
        for (y = y_start; y < y_end; ++y) {
            offset = ((size_t)z * plan->stride_z)
                   + ((size_t)y * plan->stride_y);
            for (x = 0; x < plan->res; ++x) {
                plan->distances[offset + (size_t)x] = DBL_MAX / 4.0;
                plan->sources[offset + (size_t)x] = -1;
            }
        }
    }

    return SIXEL_OK;
}

static void
sixel_lookup_fhedt_first_touch_8bit(double *distances,
                              int *sources,
                              int res,
                              int threads,
                              int pin_threads,
                              int tile_xy,
                              int tile_depth)
{
    sixel_lookup_fhedt_first_touch_plan_8bit_t plan;
    int tiles_y;
    int tiles_z;

    plan.distances = distances;
    plan.sources = sources;
    plan.res = res;
    plan.stride_y = (size_t)res;
    plan.stride_z = (size_t)res * (size_t)res;
    plan.tile_y = tile_xy;
    plan.tile_z = tile_depth;
    tiles_y = (res + plan.tile_y - 1) / plan.tile_y;
    tiles_z = (res + plan.tile_z - 1) / plan.tile_z;
    plan.tiles_y = tiles_y;
    plan.tiles_z = tiles_z;

    sixel_lookup_fhedt_dispatch_tiles_8bit(tiles_y * tiles_z,
                                     threads,
                                     pin_threads,
                                     sixel_lookup_fhedt_first_touch_worker_8bit,
                                     &plan);
}

static void
sixel_lookup_fhedt_seed_grid_8bit(int resolution,
                            int depth,
                            int ncolors,
                            unsigned char const *palette,
                            double *distances,
                            int *sources)
{
    size_t plane;
    size_t grid;
    int index;
    int component;
    size_t offset;
    int x;
    int y;
    int z;

    grid = (size_t)resolution * (size_t)resolution * (size_t)resolution;
    for (offset = 0; offset < grid; ++offset) {
        distances[offset] = DBL_MAX / 4.0;
        sources[offset] = -1;
    }

    for (index = 0; index < ncolors; ++index) {
        x = palette[sixel_lookup_fhedt_palette_index_8bit(depth, index, 0)];
        y = palette[sixel_lookup_fhedt_palette_index_8bit(depth, index, 1)];
        z = palette[sixel_lookup_fhedt_palette_index_8bit(depth, index, 2)];
        plane = (size_t)resolution * (size_t)resolution;
        offset = ((size_t)z * plane) + ((size_t)y * (size_t)resolution)
               + (size_t)x;
        distances[offset] = 0.0;
        sources[offset] = index;
        /*
         * Cells that collide pick the last palette entry.  This stable choice
         * keeps the construction deterministic without extra deduplication.
         */
        for (component = 0; component < depth; ++component) {
            (void)component;
        }
    }
}

static void
sixel_lookup_fhedt_edt1d_scalar_8bit(double *line_dist,
                               int *line_src,
                               int length,
                               double weight)
{
    double zbuf[257];
    int vbuf[256];
    double scratch[256];
    int k;
    int q;
    int i;
    int hull_end;
    double s;
    double candidate;
    double denom;

    if (length <= 0) {
        return;
    }
    if (length > 256) {
        length = 256;
    }

    vbuf[0] = 0;
    zbuf[0] = -DBL_MAX;
    zbuf[1] = DBL_MAX;
    k = 0;

    for (q = 1; q < length; ++q) {
        denom = 2.0 * weight * (double)(q - vbuf[k]);
        if (denom == 0.0) {
            denom = 1.0;
        }
        candidate = (line_dist[q] + weight * (double)(q * q))
                  - (line_dist[vbuf[k]]
                     + weight * (double)(vbuf[k] * vbuf[k]));
        s = candidate / denom;
        while (k > 0 && s <= zbuf[k]) {
            --k;
            denom = 2.0 * weight * (double)(q - vbuf[k]);
            if (denom == 0.0) {
                denom = 1.0;
            }
            candidate = (line_dist[q] + weight * (double)(q * q))
                      - (line_dist[vbuf[k]]
                         + weight * (double)(vbuf[k] * vbuf[k]));
            s = candidate / denom;
        }
        ++k;
        vbuf[k] = q;
        zbuf[k] = s;
        zbuf[k + 1] = DBL_MAX;
    }

    hull_end = k;
    k = 0;
    for (i = 0; i < length; ++i) {
        int idx;
        while (k < hull_end && zbuf[k + 1] < (double)i) {
            ++k;
        }
        scratch[i] = line_dist[i];
        idx = vbuf[k];
        if (idx < 0 || idx >= length) {
            continue;
        }
        scratch[i] = line_dist[idx]
                   + weight * (double)((i - idx) * (i - idx));
        line_src[i] = line_src[idx];
    }

    for (i = 0; i < length; ++i) {
        line_dist[i] = scratch[i];
    }
}

static sixel_lookup_fhedt_edt1d_fn_8bit
sixel_lookup_fhedt_edt1d_resolve_8bit(void)
{
    static sixel_lookup_fhedt_edt1d_fn_8bit selected;

    if (selected != NULL) {
        return selected;
    }
    selected = sixel_lookup_fhedt_edt1d_scalar_8bit;

    return selected;
}

typedef struct sixel_lookup_fhedt_pass_x_plan_8bit {
    sixel_lookup_fhedt_shared_8bit_t *shared;
    double *distances;
    int *sources;
    sixel_lookup_fhedt_timeline_8bit_t *timeline;
    sixel_lookup_fhedt_edt1d_fn_8bit edt1d;
    double weight;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_y;
    int tile_z;
    int tiles_y;
    int tiles_z;
    int log_lines;
} sixel_lookup_fhedt_pass_x_plan_8bit_t;

typedef struct sixel_lookup_fhedt_pass_y_plan_8bit {
    sixel_lookup_fhedt_shared_8bit_t *shared;
    double *distances;
    int *sources;
    sixel_lookup_fhedt_timeline_8bit_t *timeline;
    sixel_lookup_fhedt_edt1d_fn_8bit edt1d;
    double weight;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_x;
    int tile_z;
    int tiles_x;
    int tiles_z;
    int log_lines;
} sixel_lookup_fhedt_pass_y_plan_8bit_t;

typedef struct sixel_lookup_fhedt_pass_z_plan_8bit {
    sixel_lookup_fhedt_shared_8bit_t *shared;
    double *distances;
    int *sources;
    sixel_lookup_fhedt_timeline_8bit_t *timeline;
    sixel_lookup_fhedt_edt1d_fn_8bit edt1d;
    double weight;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_x;
    int tile_y;
    int tiles_x;
    int tiles_y;
    int log_lines;
} sixel_lookup_fhedt_pass_z_plan_8bit_t;

/*
 * Each worker processes one tile worth of x-lines.  Tiles are laid out in a
 * z-major order so adjacent jobs stay close in memory.  Timeline markers keep
 * the visualization aligned with the new tiling.
 */
static int
sixel_lookup_fhedt_pass_x_worker_8bit(tp_job_t job,
                                void *userdata,
                                void *workspace)
{
    sixel_lookup_fhedt_pass_x_plan_8bit_t *plan;
    sixel_lookup_fhedt_line_buffer_8bit_t *line_buffer;
    int tile_index;
    int tile_z_index;
    int tile_y_index;
    int y_start;
    int y_end;
    int z_start;
    int z_end;
    int y;
    int z;
    int x;
    size_t offset;
    size_t next_offset;
    char message[32];

    (void)workspace;

    plan = (sixel_lookup_fhedt_pass_x_plan_8bit_t *)userdata;
    line_buffer = sixel_lookup_fhedt_line_buffer_get_8bit();
    tile_index = job.band_index;
    tile_z_index = tile_index / plan->tiles_y;
    tile_y_index = tile_index - (tile_z_index * plan->tiles_y);
    z_start = tile_z_index * plan->tile_z;
    z_end = z_start + plan->tile_z;
    if (z_end > plan->res) {
        z_end = plan->res;
    }
    y_start = tile_y_index * plan->tile_y;
    y_end = y_start + plan->tile_y;
    if (y_end > plan->res) {
        y_end = plan->res;
    }

    for (z = z_start; z < z_end; ++z) {
        for (y = y_start; y < y_end; ++y) {
            offset = ((size_t)z * plan->stride_z)
                   + ((size_t)y * plan->stride_y);
            if (plan->log_lines != 0) {
                snprintf(message, sizeof(message), "z=%d", z);
                sixel_lookup_fhedt_timeline_log_8bit(plan->timeline,
                                               "fhedt-x",
                                               "line-start",
                                               z,
                                               y,
                                               message);
            }
            if (y + 1 < plan->res) {
                next_offset = offset + plan->stride_y;
                sixel_lookup_fhedt_prefetch_line_8bit(plan->distances,
                                                plan->sources,
                                                next_offset,
                                                plan->timeline,
                                                "fhedt-x",
                                                z,
                                                y + 1);
            }
            for (x = 0; x < plan->res; ++x) {
                line_buffer->dist[x] = plan->distances[offset + (size_t)x];
                line_buffer->src[x] = plan->sources[offset + (size_t)x];
            }
            plan->edt1d(line_buffer->dist,
                        line_buffer->src,
                        plan->res,
                        plan->weight);
            for (x = 0; x < plan->res; ++x) {
                plan->distances[offset + (size_t)x] = line_buffer->dist[x];
                plan->sources[offset + (size_t)x] = line_buffer->src[x];
            }
            if (plan->log_lines != 0) {
                sixel_lookup_fhedt_timeline_log_8bit(plan->timeline,
                                               "fhedt-x",
                                               "line-end",
                                               z,
                                               y,
                                               message);
            }
        }
    }

    return SIXEL_OK;
}

static int
sixel_lookup_fhedt_pass_y_worker_8bit(tp_job_t job,
                                void *userdata,
                                void *workspace)
{
    sixel_lookup_fhedt_pass_y_plan_8bit_t *plan;
    sixel_lookup_fhedt_line_buffer_8bit_t *line_buffer;
    int tile_index;
    int tile_z_index;
    int tile_x_index;
    int x_start;
    int x_end;
    int z_start;
    int z_end;
    int x;
    int y;
    int z;
    size_t offset;
    size_t next_offset;
    char message[32];

    (void)workspace;

    plan = (sixel_lookup_fhedt_pass_y_plan_8bit_t *)userdata;
    line_buffer = sixel_lookup_fhedt_line_buffer_get_8bit();
    tile_index = job.band_index;
    tile_z_index = tile_index / plan->tiles_x;
    tile_x_index = tile_index - (tile_z_index * plan->tiles_x);
    z_start = tile_z_index * plan->tile_z;
    z_end = z_start + plan->tile_z;
    if (z_end > plan->res) {
        z_end = plan->res;
    }
    x_start = tile_x_index * plan->tile_x;
    x_end = x_start + plan->tile_x;
    if (x_end > plan->res) {
        x_end = plan->res;
    }

    for (z = z_start; z < z_end; ++z) {
        for (x = x_start; x < x_end; ++x) {
            if (plan->log_lines != 0) {
                snprintf(message, sizeof(message), "z=%d", z);
                sixel_lookup_fhedt_timeline_log_8bit(plan->timeline,
                                               "fhedt-y",
                                               "line-start",
                                               z,
                                               x,
                                               message);
            }
            for (y = 0; y < plan->res; ++y) {
                offset = ((size_t)z * plan->stride_z)
                       + ((size_t)y * plan->stride_y)
                       + (size_t)x;
                if (y + 1 < plan->res) {
                    next_offset = offset + plan->stride_y;
                    sixel_lookup_fhedt_prefetch_line_8bit(plan->distances,
                                                    plan->sources,
                                                    next_offset,
                                                    plan->timeline,
                                                    "fhedt-y",
                                                    z,
                                                    x);
                }
                line_buffer->dist[y] = plan->distances[offset];
                line_buffer->src[y] = plan->sources[offset];
            }
            plan->edt1d(line_buffer->dist,
                        line_buffer->src,
                        plan->res,
                        plan->weight);
            for (y = 0; y < plan->res; ++y) {
                offset = ((size_t)z * plan->stride_z)
                       + ((size_t)y * plan->stride_y)
                       + (size_t)x;
                plan->distances[offset] = line_buffer->dist[y];
                plan->sources[offset] = line_buffer->src[y];
            }
            if (plan->log_lines != 0) {
                sixel_lookup_fhedt_timeline_log_8bit(plan->timeline,
                                               "fhedt-y",
                                               "line-end",
                                               z,
                                               x,
                                               message);
            }
        }
    }

    return SIXEL_OK;
}

static int
sixel_lookup_fhedt_pass_z_worker_8bit(tp_job_t job,
                                void *userdata,
                                void *workspace)
{
    sixel_lookup_fhedt_pass_z_plan_8bit_t *plan;
    sixel_lookup_fhedt_line_buffer_8bit_t *line_buffer;
    int tile_index;
    int tile_y_index;
    int tile_x_index;
    int x_start;
    int x_end;
    int y_start;
    int y_end;
    int x;
    int y;
    int z;
    size_t offset;
    size_t next_offset;
    char message[32];

    (void)workspace;

    plan = (sixel_lookup_fhedt_pass_z_plan_8bit_t *)userdata;
    line_buffer = sixel_lookup_fhedt_line_buffer_get_8bit();
    tile_index = job.band_index;
    tile_y_index = tile_index / plan->tiles_x;
    tile_x_index = tile_index - (tile_y_index * plan->tiles_x);
    y_start = tile_y_index * plan->tile_y;
    y_end = y_start + plan->tile_y;
    if (y_end > plan->res) {
        y_end = plan->res;
    }
    x_start = tile_x_index * plan->tile_x;
    x_end = x_start + plan->tile_x;
    if (x_end > plan->res) {
        x_end = plan->res;
    }

    for (y = y_start; y < y_end; ++y) {
        for (x = x_start; x < x_end; ++x) {
            if (plan->log_lines != 0) {
                snprintf(message, sizeof(message), "y=%d", y);
                sixel_lookup_fhedt_timeline_log_8bit(plan->timeline,
                                               "fhedt-z",
                                               "line-start",
                                               y,
                                               x,
                                               message);
            }
            for (z = 0; z < plan->res; ++z) {
                offset = ((size_t)z * plan->stride_z)
                       + ((size_t)y * plan->stride_y)
                       + (size_t)x;
                if (z + 1 < plan->res) {
                    next_offset = offset + plan->stride_z;
                    sixel_lookup_fhedt_prefetch_line_8bit(plan->distances,
                                                    plan->sources,
                                                    next_offset,
                                                    plan->timeline,
                                                    "fhedt-z",
                                                    y,
                                                    x);
                }
                line_buffer->dist[z] = plan->distances[offset];
                line_buffer->src[z] = plan->sources[offset];
            }
            plan->edt1d(line_buffer->dist,
                        line_buffer->src,
                        plan->res,
                        plan->weight);
            for (z = 0; z < plan->res; ++z) {
                offset = ((size_t)z * plan->stride_z)
                       + ((size_t)y * plan->stride_y)
                       + (size_t)x;
                plan->distances[offset] = line_buffer->dist[z];
                plan->sources[offset] = line_buffer->src[z];
            }
            if (plan->log_lines != 0) {
                sixel_lookup_fhedt_timeline_log_8bit(plan->timeline,
                                               "fhedt-z",
                                               "line-end",
                                               y,
                                               x,
                                               message);
            }
        }
    }

    return SIXEL_OK;
}

static void
sixel_lookup_fhedt_dispatch_tiles_8bit(int total_tiles,
                                 int threads,
                                 int pin_threads,
                                 tp_worker_fn worker,
                                 void *plan)
{
#if SIXEL_ENABLE_THREADS
    threadpool_t *pool;
    int queue_depth;
    int job_index;
    tp_job_t job;

    if (threads < 2 || total_tiles < 2) {
        for (job_index = 0; job_index < total_tiles; ++job_index) {
            job.band_index = job_index;
            (void)worker(job, plan, NULL);
        }

        return;
    }

    if (threads > total_tiles) {
        threads = total_tiles;
    }
    queue_depth = threads * 3;
    if (queue_depth > total_tiles) {
        queue_depth = total_tiles;
    }
    pool = threadpool_create(threads,
                             queue_depth,
                             0,
                             worker,
                             plan,
                             NULL);
    if (pool == NULL) {
        for (job_index = 0; job_index < total_tiles; ++job_index) {
            job.band_index = job_index;
            (void)worker(job, plan, NULL);
        }

        return;
    }
    threadpool_set_affinity(pool, pin_threads);
    for (job_index = 0; job_index < total_tiles; ++job_index) {
        job.band_index = job_index;
        threadpool_push(pool, job);
    }
    threadpool_finish(pool);
    (void)threadpool_get_error(pool);
    threadpool_destroy(pool);
#else
    tp_job_t job;
    int job_index;

    (void)threads;
    (void)pin_threads;
    for (job_index = 0; job_index < total_tiles; ++job_index) {
        job.band_index = job_index;
        (void)worker(job, plan, NULL);
    }
#endif  /* SIXEL_ENABLE_THREADS */
}

static void
sixel_lookup_fhedt_apply_edt_8bit(sixel_lookup_fhedt_shared_8bit_t *shared,
                            double *distances,
                            int *sources,
                            sixel_lookup_fhedt_timeline_8bit_t *timeline,
                            int threads,
                            int pin_threads,
                            int tile_xy,
                            int tile_depth)
{
    sixel_lookup_fhedt_pass_x_plan_8bit_t plan_x;
    sixel_lookup_fhedt_pass_y_plan_8bit_t plan_y;
    sixel_lookup_fhedt_pass_z_plan_8bit_t plan_z;
    int res;
    size_t plane;
    size_t stride_y;
    size_t stride_z;
    int tiles_y;
    int tiles_z;
    int tiles_x;
    int i;
    int log_lines;
    sixel_lookup_fhedt_edt1d_fn_8bit edt1d;

    res = shared->resolution;
    plane = (size_t)res * (size_t)res;
    stride_y = (size_t)res;
    stride_z = plane;
    log_lines = sixel_lookup_fhedt_timeline_lines_enabled_8bit(timeline);
    edt1d = sixel_lookup_fhedt_edt1d_resolve_8bit();

    tiles_y = (res + tile_xy - 1) / tile_xy;
    tiles_z = (res + tile_depth - 1) / tile_depth;

    plan_x.shared = shared;
    plan_x.distances = distances;
    plan_x.sources = sources;
    plan_x.timeline = timeline;
    plan_x.edt1d = edt1d;
    plan_x.weight = (double)shared->weights[0];
    plan_x.stride_y = stride_y;
    plan_x.stride_z = stride_z;
    plan_x.res = res;
    plan_x.tile_y = tile_xy;
    plan_x.tile_z = tile_depth;
    plan_x.tiles_y = tiles_y;
    plan_x.tiles_z = tiles_z;
    plan_x.log_lines = log_lines;

    sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                   "fhedt-x",
                                   "pass-start",
                                   -1,
                                   -1,
                                   "x-pass");
    sixel_lookup_fhedt_dispatch_tiles_8bit(tiles_y * tiles_z,
                                     threads,
                                     pin_threads,
                                     sixel_lookup_fhedt_pass_x_worker_8bit,
                                     &plan_x);
    sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                   "fhedt-x",
                                   "pass-end",
                                   -1,
                                   -1,
                                   "x-pass");

    tiles_x = tiles_y;

    plan_y.shared = shared;
    plan_y.distances = distances;
    plan_y.sources = sources;
    plan_y.timeline = timeline;
    plan_y.edt1d = edt1d;
    plan_y.weight = (double)shared->weights[1];
    plan_y.stride_y = stride_y;
    plan_y.stride_z = stride_z;
    plan_y.res = res;
    plan_y.tile_x = tile_xy;
    plan_y.tile_z = tile_depth;
    plan_y.tiles_x = tiles_x;
    plan_y.tiles_z = tiles_z;
    plan_y.log_lines = log_lines;

    sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                   "fhedt-y",
                                   "pass-start",
                                   -1,
                                   -1,
                                   "y-pass");
    sixel_lookup_fhedt_dispatch_tiles_8bit(tiles_x * tiles_z,
                                     threads,
                                     pin_threads,
                                     sixel_lookup_fhedt_pass_y_worker_8bit,
                                     &plan_y);
    sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                   "fhedt-y",
                                   "pass-end",
                                   -1,
                                   -1,
                                   "y-pass");

    plan_z.shared = shared;
    plan_z.distances = distances;
    plan_z.sources = sources;
    plan_z.timeline = timeline;
    plan_z.edt1d = edt1d;
    plan_z.weight = (double)shared->weights[2];
    plan_z.stride_y = stride_y;
    plan_z.stride_z = stride_z;
    plan_z.res = res;
    plan_z.tile_x = tile_xy;
    plan_z.tile_y = tile_xy;
    plan_z.tiles_x = tiles_x;
    plan_z.tiles_y = tiles_y;
    plan_z.log_lines = log_lines;

    sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                   "fhedt-z",
                                   "pass-start",
                                   -1,
                                   -1,
                                   "z-pass");
    sixel_lookup_fhedt_dispatch_tiles_8bit(tiles_x * tiles_y,
                                     threads,
                                     pin_threads,
                                     sixel_lookup_fhedt_pass_z_worker_8bit,
                                     &plan_z);
    sixel_lookup_fhedt_timeline_log_8bit(timeline,
                                   "fhedt-z",
                                   "pass-end",
                                   -1,
                                   -1,
                                   "z-pass");

    for (i = 0; i < shared->ncolors; ++i) {
        (void)i;
    }
}

static void
sixel_lookup_fhedt_fill_indices_8bit(sixel_lookup_fhedt_shared_8bit_t *shared,
                               int *sources)
{
    size_t total;
    size_t index;

    total = (size_t)shared->resolution * (size_t)shared->resolution
          * (size_t)shared->resolution;
    if (!shared->use_u16) {
        for (index = 0; index < total; ++index) {
            shared->indices8[index] = (uint8_t)sources[index];
        }
    } else {
        for (index = 0; index < total; ++index) {
            shared->indices16[index] = (uint16_t)sources[index];
        }
    }
}

static void
sixel_lookup_fhedt_mark_boundaries_8bit(sixel_lookup_fhedt_shared_8bit_t *shared,
                                  int *sources)
{
    size_t plane;
    size_t total;
    size_t offset;
    int res;
    int x;
    int y;
    int z;
    int dx;
    int dy;
    int dz;
    int neighbor;
    int current;
    size_t bit_index;
    size_t byte_index;
    size_t neighbor_offset;

    res = shared->resolution;
    plane = (size_t)res * (size_t)res;
    total = (size_t)res * plane;

    for (offset = 0; offset < (total + 7U) / 8U; ++offset) {
        shared->boundary[offset] = 0U;
    }

    for (z = 0; z < res; ++z) {
        for (y = 0; y < res; ++y) {
            for (x = 0; x < res; ++x) {
                offset = ((size_t)z * plane)
                       + ((size_t)y * (size_t)res)
                       + (size_t)x;
                current = sources[offset];
                for (dz = -1; dz <= 1; dz += 2) {
                    if (z + dz < 0 || z + dz >= res) {
                        continue;
                    }
                    neighbor_offset = dz < 0 ? offset - plane
                                             : offset + plane;
                    neighbor = sources[neighbor_offset];
                    if (neighbor != current) {
                        bit_index = offset;
                        byte_index = bit_index / 8U;
                        shared->boundary[byte_index]
                            |= (unsigned char)(1U
                                               << (bit_index % 8U));
                        break;
                    }
                }
                for (dy = -1; dy <= 1; dy += 2) {
                    if (y + dy < 0 || y + dy >= res) {
                        continue;
                    }
                    neighbor_offset = dy < 0 ? offset - (size_t)res
                                             : offset + (size_t)res;
                    neighbor = sources[neighbor_offset];
                    if (neighbor != current) {
                        bit_index = offset;
                        byte_index = bit_index / 8U;
                        shared->boundary[byte_index]
                            |= (unsigned char)(1U
                                               << (bit_index % 8U));
                        break;
                    }
                }
                for (dx = -1; dx <= 1; dx += 2) {
                    if (x + dx < 0 || x + dx >= res) {
                        continue;
                    }
                    neighbor_offset = dx < 0 ? offset - 1U
                                             : offset + 1U;
                    neighbor = sources[neighbor_offset];
                    if (neighbor != current) {
                        bit_index = offset;
                        byte_index = bit_index / 8U;
                        shared->boundary[byte_index]
                            |= (unsigned char)(1U
                                               << (bit_index % 8U));
                        break;
                    }
                }
            }
        }
    }
}

static SIXELSTATUS
sixel_lookup_fhedt_build_8bit(sixel_lookup_fhedt_8bit_t *fhedt,
                        unsigned char const *palette,
                        int ncolors,
                        int resolution,
                        int refine,
                        int use_dist2,
                        int wcomp1,
                        int wcomp2,
                        int wcomp3,
                        int depth)
{
    sixel_lookup_fhedt_shared_8bit_t *shared;
    double *distances;
    int *sources;
    size_t total;
    size_t palette_size;
    size_t offset;
    int threads;
    int pin_threads;
    int first_touch;
    int tile_xy;
    int tile_depth;
    sixel_lookup_fhedt_timeline_8bit_t timeline;
    char timeline_message[128];

    timeline.initialized = 0;
    shared = sixel_allocator_malloc(fhedt->allocator, sizeof(*shared));
    if (shared == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_build_8bit: allocation failed (shared).");
        sixel_lookup_fhedt_timeline_close_8bit(&timeline);
        return SIXEL_BAD_ALLOCATION;
    }

    shared->refcount = 1U;
    shared->resolution = resolution;
    shared->refine = refine;
    shared->use_dist2 = use_dist2;
    shared->weights[0] = wcomp1;
    shared->weights[1] = wcomp2;
    shared->weights[2] = wcomp3;
    shared->ncolors = ncolors;
    shared->depth = depth;
    shared->use_u16 = ncolors > 256 ? 1 : 0;
    shared->safe_radius2 = 0.0;
    shared->indices8 = NULL;
    shared->indices16 = NULL;
    shared->boundary = NULL;
    shared->dist2 = NULL;
    shared->palette = NULL;
    shared->res_shift = 8 - sixel_lookup_fhedt_pow2_log_8bit(resolution);

    palette_size = (size_t)ncolors * (size_t)depth;
    shared->palette = (unsigned char *)sixel_allocator_malloc(fhedt->allocator,
                                                             palette_size);
    if (shared->palette == NULL) {
        sixel_lookup_fhedt_shared_destroy_8bit(fhedt->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_build_8bit: palette allocation failed.");
        sixel_lookup_fhedt_timeline_close_8bit(&timeline);
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_lookup_fhedt_quantize_palette_8bit(palette, shared);

    threads = sixel_lookup_fhedt_resolve_threads_8bit();
    pin_threads = sixel_lookup_fhedt_pin_threads_enabled_8bit();
    first_touch = sixel_lookup_fhedt_first_touch_enabled_8bit();
    sixel_lookup_fhedt_resolve_tiles_8bit(palette,
                                    ncolors,
                                    depth,
                                    resolution,
                                    &tile_xy,
                                    &tile_depth);

    total = (size_t)resolution * (size_t)resolution * (size_t)resolution;
    if (!shared->use_u16) {
        shared->indices8 = (uint8_t *)sixel_allocator_malloc(fhedt->allocator,
                                                             total);
    } else {
        shared->indices16 = (uint16_t *)sixel_allocator_malloc(
            fhedt->allocator,
            total * sizeof(uint16_t));
    }
    if (use_dist2 != 0) {
        shared->dist2 = (float *)sixel_allocator_malloc(fhedt->allocator,
                                                        total
                                                        * sizeof(float));
    }
    shared->boundary = (unsigned char *)sixel_allocator_malloc(
        fhedt->allocator,
        (total + 7U) / 8U);
    if ((shared->use_u16 && shared->indices16 == NULL)
        || (!shared->use_u16 && shared->indices8 == NULL)
        || shared->boundary == NULL
        || (use_dist2 != 0 && shared->dist2 == NULL)) {
        sixel_lookup_fhedt_shared_destroy_8bit(fhedt->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_build_8bit: LUT allocation failed.");
        sixel_lookup_fhedt_timeline_close_8bit(&timeline);
        return SIXEL_BAD_ALLOCATION;
    }

    distances = (double *)sixel_allocator_malloc(
        fhedt->allocator,
        total * sizeof(double));
    sources = (int *)sixel_allocator_malloc(fhedt->allocator,
                                            total * sizeof(int));
    if (distances == NULL || sources == NULL) {
        sixel_allocator_free(fhedt->allocator, distances);
        sixel_allocator_free(fhedt->allocator, sources);
        sixel_lookup_fhedt_shared_destroy_8bit(fhedt->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_build_8bit: temporary buffer "
            "allocation failed.");
        sixel_lookup_fhedt_timeline_close_8bit(&timeline);
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_lookup_fhedt_timeline_open_8bit(&timeline);
    /* Tag the FHEDT build so timeline.py surfaces backend selection. */
    (void)snprintf(timeline_message,
                   sizeof(timeline_message),
                   "res=%d colors=%d refine=%d dist2=%d",
                   resolution,
                   ncolors,
                   refine,
                   use_dist2);
    sixel_lookup_fhedt_timeline_log_8bit(&timeline,
                                   "fhedt",
                                   "builder-start",
                                   resolution,
                                   ncolors,
                                   timeline_message);
    if (first_touch != 0) {
        sixel_lookup_fhedt_first_touch_8bit(distances,
                                      sources,
                                      resolution,
                                      threads,
                                      pin_threads,
                                      tile_xy,
                                      tile_depth);
    }
    sixel_lookup_fhedt_seed_grid_8bit(resolution,
                                shared->depth,
                                shared->ncolors,
                                shared->palette,
                                distances,
                                sources);
    sixel_lookup_fhedt_apply_edt_8bit(shared,
                                distances,
                                sources,
                                &timeline,
                                threads,
                                pin_threads,
                                tile_xy,
                                tile_depth);
    sixel_lookup_fhedt_fill_indices_8bit(shared, sources);
    sixel_lookup_fhedt_mark_boundaries_8bit(shared, sources);
    if (shared->dist2 != NULL) {
        for (offset = 0U; offset < total; ++offset) {
            shared->dist2[offset] = (float)distances[offset];
        }
    }

    sixel_lookup_fhedt_timeline_log_8bit(&timeline,
                                   "fhedt",
                                   "builder-end",
                                   resolution,
                                   ncolors,
                                   timeline_message);
    sixel_allocator_free(fhedt->allocator, distances);
    sixel_allocator_free(fhedt->allocator, sources);
    sixel_lookup_fhedt_timeline_close_8bit(&timeline);

    /*
     * The quantized lattice maps every voxel to a unit cube.
     * Any pixel inside the cell can move at most 0.5 units per axis,
     * so this radius bounds the maximum squared displacement within the cell.
     */
    shared->safe_radius2 = ((double)wcomp1 * 0.25)
                         + ((double)wcomp2 * 0.25)
                         + ((double)wcomp3 * 0.25);

    fhedt->shared = shared;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_fhedt_8bit_create(sixel_allocator_t *allocator,
                              sixel_lookup_fhedt_8bit_t **fhedt_out)
{
    sixel_lookup_fhedt_8bit_t *fhedt;

    if (allocator == NULL || fhedt_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    fhedt = sixel_allocator_malloc(allocator, sizeof(*fhedt));
    if (fhedt == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    fhedt->allocator = allocator;
    fhedt->shared = NULL;
    fhedt->shared_published = 0;
    fhedt->use_cache = 0;
    *fhedt_out = fhedt;

    return SIXEL_OK;
}

static void
sixel_lookup_fhedt_8bit_release_shared(sixel_lookup_fhedt_8bit_t *fhedt)
{
    sixel_allocator_t *allocator;

    if (fhedt == NULL || fhedt->shared == NULL) {
        return;
    }

    allocator = fhedt->allocator;
    sixel_lookup_fhedt_shared_unref_8bit(allocator, fhedt->shared);
    if (fhedt->shared_published != 0) {
        sixel_lookup_fhedt_shared_unref_8bit(allocator, fhedt->shared);
    }
    fhedt->shared = NULL;
    fhedt->shared_published = 0;
}

void
sixel_lookup_fhedt_8bit_unref(sixel_lookup_fhedt_8bit_t *fhedt)
{
    if (fhedt == NULL) {
        return;
    }

    sixel_lookup_fhedt_8bit_release_shared(fhedt);
    sixel_allocator_free(fhedt->allocator, fhedt);
}

SIXELSTATUS
sixel_lookup_fhedt_8bit_configure(sixel_lookup_fhedt_8bit_t *fhedt,
                                 unsigned char const *palette,
                                 int ncolors,
                                 int resolution,
                                 int refine,
                                 int use_dist2,
                                 int use_cache,
                                 int shared_flag,
                                 int wcomp1,
                                 int wcomp2,
                                 int wcomp3,
                                 int pixelformat,
                                 int depth,
                                 int parallel_dither_active)
{
    SIXELSTATUS status;

    (void)pixelformat;

    if (fhedt == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_lookup_fhedt_validate_resolution_8bit(resolution)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_8bit_configure: resolution must be 64/128/256.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (fhedt->shared != NULL) {
        sixel_lookup_fhedt_8bit_release_shared(fhedt);
    }

    status = sixel_lookup_fhedt_build_8bit(fhedt,
                                     palette,
                                     ncolors,
                                     resolution,
                                     refine,
                                     use_dist2,
                                     wcomp1,
                                     wcomp2,
                                     wcomp3,
                                     depth);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (shared_flag != 0) {
        sixel_lookup_fhedt_shared_ref_8bit(fhedt->shared);
        fhedt->shared_published = 1;
    } else {
        fhedt->shared_published = 0;
    }

    fhedt->parallel_dither_active = (parallel_dither_active != 0);

#if SIXEL_FHEDT_TLS_AVAILABLE == 0
    if (fhedt->parallel_dither_active != 0) {
        /*
         * Thread-local storage is not supported and parallel dithering is
         * active.  Disable the FHEDT cache to avoid sharing a single cache
         * instance across worker threads.
         */
        use_cache = 0;
    }
#endif

    fhedt->use_cache = use_cache;

    return SIXEL_OK;
}

static int
sixel_lookup_fhedt_read_index_8bit(sixel_lookup_fhedt_shared_8bit_t const *shared,
                             size_t offset)
{
    if (!shared->use_u16) {
        return (int)shared->indices8[offset];
    }

    return (int)shared->indices16[offset];
}

static int
sixel_lookup_fhedt_boundary_bit_8bit(
    sixel_lookup_fhedt_shared_8bit_t const *shared,
    size_t offset)
{
    size_t byte_index;
    size_t bit_index;

    byte_index = offset / 8U;
    bit_index = offset % 8U;

    return (shared->boundary[byte_index] >> bit_index) & 1U;
}

static int
sixel_lookup_fhedt_refine_needed_8bit(
    sixel_lookup_fhedt_shared_8bit_t const *shared,
    size_t offset)
{
    double dist2;

    if (shared->use_dist2 == 0 || shared->dist2 == NULL) {
        return 1;
    }

    dist2 = (double)shared->dist2[offset];
    if (dist2 <= shared->safe_radius2) {
        return 0;
    }

    return 1;
}

static int
sixel_lookup_fhedt_refine_candidates_8bit(
    sixel_lookup_fhedt_shared_8bit_t const *shared,
    unsigned char const *pixel,
    int x,
    int y,
    int z)
{
    int best_index;
    double best_distance;
    int corner_x[2];
    int corner_y[2];
    int corner_z[2];
    int cx;
    int cy;
    int cz;
    int idx;
    int candidate;
    double dx;
    double dy;
    double dz;
    double distance;
    int weight1;
    int weight2;
    int weight3;
    unsigned char p1;
    unsigned char p2;
    unsigned char p3;
    int used[8];
    int used_count;
    size_t offset;
    size_t plane;

    corner_x[0] = x;
    corner_x[1] = x + 1;
    if (corner_x[1] >= shared->resolution) {
        corner_x[1] = shared->resolution - 1;
    }
    corner_y[0] = y;
    corner_y[1] = y + 1;
    if (corner_y[1] >= shared->resolution) {
        corner_y[1] = shared->resolution - 1;
    }
    corner_z[0] = z;
    corner_z[1] = z + 1;
    if (corner_z[1] >= shared->resolution) {
        corner_z[1] = shared->resolution - 1;
    }

    plane = (size_t)shared->resolution * (size_t)shared->resolution;
    weight1 = shared->weights[0];
    weight2 = shared->weights[1];
    weight3 = shared->weights[2];
    best_index = -1;
    best_distance = DBL_MAX;
    used_count = 0;

    for (cz = 0; cz < 2; ++cz) {
        for (cy = 0; cy < 2; ++cy) {
            for (cx = 0; cx < 2; ++cx) {
                offset = ((size_t)corner_z[cz] * plane)
                       + ((size_t)corner_y[cy] * (size_t)shared->resolution)
                       + (size_t)corner_x[cx];
                candidate = sixel_lookup_fhedt_read_index_8bit(shared, offset);

                for (idx = 0; idx < used_count; ++idx) {
                    if (used[idx] == candidate) {
                        candidate = -1;
                        break;
                    }
                }
                if (candidate < 0) {
                    continue;
                }
                used[used_count] = candidate;
                ++used_count;

                p1 = shared->palette[sixel_lookup_fhedt_palette_index_8bit(
                    shared->depth,
                    candidate,
                    0)];
                p2 = shared->palette[sixel_lookup_fhedt_palette_index_8bit(
                    shared->depth,
                    candidate,
                    1)];
                p3 = shared->palette[sixel_lookup_fhedt_palette_index_8bit(
                    shared->depth,
                    candidate,
                    2)];
                dx = (double)((int)pixel[0] - (int)(p1 << shared->res_shift));
                dy = (double)((int)pixel[1] - (int)(p2 << shared->res_shift));
                dz = (double)((int)pixel[2] - (int)(p3 << shared->res_shift));
                distance = (double)weight1 * dx * dx
                         + (double)weight2 * dy * dy
                         + (double)weight3 * dz * dz;
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = candidate;
                }
            }
        }
    }

    return best_index;
}

int
sixel_lookup_fhedt_8bit_map(sixel_lookup_fhedt_8bit_t *fhedt,
                           unsigned char const *pixel)
{
    int x;
    int y;
    int z;
    int cached_value;
    int should_refine;
    int cache_active;
    int index;
    size_t offset;
    size_t plane;

    if (fhedt == NULL || pixel == NULL || fhedt->shared == NULL) {
        return -1;
    }

    x = (int)(pixel[0] >> fhedt->shared->res_shift);
    y = (int)(pixel[1] >> fhedt->shared->res_shift);
    z = (int)(pixel[2] >> fhedt->shared->res_shift);
    if (x >= fhedt->shared->resolution) {
        x = fhedt->shared->resolution - 1;
    }
    if (y >= fhedt->shared->resolution) {
        y = fhedt->shared->resolution - 1;
    }
    if (z >= fhedt->shared->resolution) {
        z = fhedt->shared->resolution - 1;
    }

    plane = (size_t)fhedt->shared->resolution * (size_t)fhedt->shared->resolution;
    offset = ((size_t)z * plane)
           + ((size_t)y * (size_t)fhedt->shared->resolution)
           + (size_t)x;

    cache_active = fhedt->use_cache;
#if SIXEL_FHEDT_TLS_AVAILABLE == 0
    if (cache_active != 0 && fhedt->parallel_dither_active != 0) {
        cache_active = 0;
    }
#endif
    if (cache_active != 0) {
        sixel_lookup_fhedt_cache_prepare_8bit(fhedt->shared);
        if (sixel_lookup_fhedt_cache_get_8bit(
                &sixel_lookup_fhedt_thread_cache_8bit,
                offset,
                &cached_value)) {
            return cached_value;
        }
    }

    index = sixel_lookup_fhedt_read_index_8bit(fhedt->shared, offset);
    if (fhedt->shared->refine != 0
        && sixel_lookup_fhedt_boundary_bit_8bit(fhedt->shared, offset) != 0) {
        should_refine = sixel_lookup_fhedt_refine_needed_8bit(
                            fhedt->shared,
                            offset);
        if (should_refine != 0) {
            index = sixel_lookup_fhedt_refine_candidates_8bit(fhedt->shared,
                                                        pixel,
                                                        x,
                                                        y,
                                                        z);
        }
    }
    if (cache_active != 0) {
        sixel_lookup_fhedt_cache_put_8bit(&sixel_lookup_fhedt_thread_cache_8bit,
                                    offset,
                                    index);
    }

    return index;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
