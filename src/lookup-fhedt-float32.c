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
 * Voronoi + 3D EDT lookup implementation for float32 pixel buffers.  The
 * structure mirrors the 8bit variant while operating on normalized palette and
 * pixel samples.  A boundary bitset triggers optional refinement using the
 * eight surrounding lattice vertices.
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
#include "lookup-fhedt-float32.h"
#include "pixelformat.h"
#include "timeline-logger.h"
#include "threading.h"
#include <6cells.h>
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

struct sixel_lookup_fhedt_shared_float32 {
    sixel_atomic_u32_t refcount;
    int resolution;
    int refine;
    int use_dist2;
    float weights[3];
    float channel_min[3];
    float channel_scale[3];
    int ncolors;
    int depth;
    int pixelformat;
    double safe_radius2;
    int use_u16;
    float *palette;
    unsigned char *palette_quant;
    float *dist2;
    uint8_t *indices8;
    uint16_t *indices16;
    unsigned char *boundary;
    uint32_t signature;
};

static int const sixel_lookup_fhedt_resolution_min_float32 = 64;
static int const sixel_lookup_fhedt_resolution_max_float32 = 256;
static int const sixel_lookup_fhedt_tile_xy_default_float32 = 8;
static int const sixel_lookup_fhedt_tile_depth_default_float32 = 8;

/*
 * FHEDT builds a unit lattice, so palette and sample components must be
 * normalized to [0, 1].  For float32 working color spaces with asymmetric
 * channel ranges (Lab/DIN99d), normalize using:
 *
 *   normalized = (clamp(value) - min) / (max - min)
 *
 * The clamped, unnormalized values are still stored for distance refinement.
 */
static float
sixel_lookup_fhedt_normalize_component_float32(
    sixel_lookup_fhedt_shared_float32_t const *shared,
    float value,
    int component)
{
    float clamped;
    float normalized;

    clamped = sixel_pixelformat_float_channel_clamp(shared->pixelformat,
                                                    component,
                                                    value);
    normalized = (clamped - shared->channel_min[component])
                 * shared->channel_scale[component];
    if (normalized < 0.0f) {
        normalized = 0.0f;
    }
    if (normalized > 1.0f) {
        normalized = 1.0f;
    }

    return normalized;
}

static void
sixel_lookup_fhedt_prepare_ranges_float32(
    sixel_lookup_fhedt_shared_float32_t *shared,
    int pixelformat)
{
    float minimum;
    float range;
    int component;

    shared->pixelformat = pixelformat;
    for (component = 0; component < 3; ++component) {
        minimum = sixel_pixelformat_float_channel_min(pixelformat,
                                                      component);
        range = sixel_pixelformat_float_channel_range(pixelformat,
                                                      component);
        shared->channel_min[component] = minimum;
        if (range <= 0.0f) {
            range = 1.0f;
        }
        shared->channel_scale[component] = 1.0f / range;
    }
}

/*
 * Choose FHEDT tile defaults using palette diversity.  Highly varied palettes
 * keep tiles small for cache locality while biased sets combine work into
 * larger tiles to trim scheduling overhead.  Environment variables still take
 * precedence.
 */
static void
sixel_lookup_fhedt_choose_tile_defaults_float32(float const *palette,
                                       int ncolors,
                                       int depth,
                                       int *tile_xy_default,
                                       int *tile_depth_default)
{
    int component;
    int color;
    int offset;
    float min_component[3];
    float max_component[3];
    double sum_component[3];
    double deviation_sum[3];
    int tile_xy;
    int tile_depth;
    double span_sum;
    double mean_component[3];
    double mean_deviation;

    tile_xy = sixel_lookup_fhedt_tile_xy_default_float32;
    tile_depth = sixel_lookup_fhedt_tile_depth_default_float32;

    if (palette == NULL || ncolors <= 0 || depth <= 0) {
        *tile_xy_default = tile_xy;
        *tile_depth_default = tile_depth;

        return;
    }

    for (component = 0; component < 3; ++component) {
        min_component[component] = 1.0f;
        max_component[component] = 0.0f;
        sum_component[component] = 0.0;
        deviation_sum[component] = 0.0;
    }

    for (color = 0; color < ncolors; ++color) {
        offset = color * depth;
        for (component = 0; component < 3 && component < depth;
             ++component) {
            float value;

            value = palette[offset + component];
            if (value < min_component[component]) {
                min_component[component] = value;
            }
            if (value > max_component[component]) {
                max_component[component] = value;
            }
            sum_component[component] += (double)value;
        }
    }

    span_sum = 0.0;
    for (component = 0; component < 3 && component < depth; ++component) {
        mean_component[component] = sum_component[component]
                                    / (double)ncolors;
        span_sum += (double)(max_component[component] -
                             min_component[component]);
    }

    for (color = 0; color < ncolors; ++color) {
        offset = color * depth;
        for (component = 0; component < 3 && component < depth;
             ++component) {
            double value;

            value = (double)palette[offset + component];
            deviation_sum[component] += fabs(value
                                             - mean_component[component]);
        }
    }

    mean_deviation = 0.0;
    for (component = 0; component < 3 && component < depth; ++component) {
        mean_deviation += deviation_sum[component] / (double)ncolors;
    }
    mean_deviation /= (double)(component > 0 ? component : 1);

    if (span_sum > 1.8 || ncolors > 512) {
        tile_xy = 6;
        tile_depth = 6;
    } else if (span_sum < 0.35 && ncolors < 64) {
        tile_xy = 12;
        tile_depth = 10;
    } else if (span_sum < 0.75 && ncolors < 128) {
        tile_xy = 10;
        tile_depth = 9;
    } else if (ncolors > 256) {
        tile_xy = 7;
        tile_depth = 7;
    }

    if (mean_deviation < 0.08 && tile_xy < 12) {
        tile_xy += 1;
        tile_depth += 1;
    }

    *tile_xy_default = tile_xy;
    *tile_depth_default = tile_depth;
}

static int
sixel_lookup_fhedt_pow2_log_float32(int value)
{
    int shift;

    shift = 0;
    while ((1 << shift) < value && shift < 8) {
        ++shift;
    }

    return shift;
}

static int
sixel_lookup_fhedt_validate_resolution_float32(int resolution)
{
    int shift;
    int pow2;

    if (resolution < sixel_lookup_fhedt_resolution_min_float32
        || resolution > sixel_lookup_fhedt_resolution_max_float32) {
        return 0;
    }

    shift = sixel_lookup_fhedt_pow2_log_float32(resolution);
    pow2 = 1 << shift;

    return pow2 == resolution;
}

/*
 * Prefer the new SIXEL_LOOKUP_* knobs while still honoring the previous
 * SIXEL_FHEDT_* names for compatibility.
 */
static char const *
sixel_lookup_fhedt_getenv_float32(char const *primary, char const *legacy)
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
sixel_lookup_fhedt_parse_positive_float32(char const *env_name,
                                 char const *legacy_name,
                                 int fallback)
{
    char const *env;
    char *endptr;
    long value;

    env = sixel_lookup_fhedt_getenv_float32(env_name, legacy_name);
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
sixel_lookup_fhedt_resolve_tiles_float32(float const *palette,
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

    sixel_lookup_fhedt_choose_tile_defaults_float32(palette,
                                           ncolors,
                                           depth,
                                           &default_xy,
                                           &default_depth);

    resolved_xy = sixel_lookup_fhedt_parse_positive_float32(
        "SIXEL_LOOKUP_FHEDT_TILE_XY",
        "SIXEL_FHEDT_TILE_XY",
        default_xy);
    resolved_depth = sixel_lookup_fhedt_parse_positive_float32(
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

static uint32_t
sixel_lookup_fhedt_mix_u32_float32(uint32_t state, uint32_t value)
{
    state ^= value + 0x9e3779b9U + (state << 6) + (state >> 2);

    return state;
}

typedef struct sixel_lookup_fhedt_cache_set_float32 {
    uint32_t key[4];
    int value[4];
    uint8_t hand;
} sixel_lookup_fhedt_cache_set_float32_t;

typedef struct sixel_lookup_fhedt_cache_float32 {
    sixel_lookup_fhedt_cache_set_float32_t sets[16];
    uint32_t signature;
    sixel_lookup_fhedt_shared_float32_t const *shared;
} sixel_lookup_fhedt_cache_float32_t;

typedef struct sixel_lookup_fhedt_timeline_float32 {
    int initialized;
    int log_lines;
    int line_stride;
    sixel_timeline_logger_t *logger;
} sixel_lookup_fhedt_timeline_float32_t;

/*
 * Resolve the number of worker threads available for FHEDT construction.  The
 * float path does not rely on TLS buffers, so parallel work is allowed whenever
 * threading support is built.
 */
static int
sixel_lookup_fhedt_resolve_threads_float32(void)
{
#if SIXEL_ENABLE_THREADS
    int threads;

    threads = sixel_threads_resolve();
    if (threads < 1) {
        threads = 1;
    }

    return threads;
#else
    return 1;
#endif  /* SIXEL_ENABLE_THREADS */
}

static int
sixel_lookup_fhedt_pin_threads_enabled_float32(void)
{
    char const *env;

    env = sixel_lookup_fhedt_getenv_float32("SIXEL_LOOKUP_FHEDT_PIN_THREADS",
                                   "SIXEL_FHEDT_PIN_THREADS");
    if (env == NULL) {
        return 0;
    }

    return env[0] != '0';
}

static int
sixel_lookup_fhedt_first_touch_enabled_float32(void)
{
    char const *env;

    env = sixel_lookup_fhedt_getenv_float32("SIXEL_LOOKUP_FHEDT_FIRST_TOUCH",
                                   "SIXEL_FHEDT_FIRST_TOUCH");
    if (env == NULL) {
        return 0;
    }

    return env[0] != '0';
}

static void sixel_lookup_fhedt_dispatch_tiles_float32(int total_tiles,
                                             int threads,
                                             int pin_threads,
                                             sixel_thread_pool_worker_function_t
                                                worker,
                                             void *plan);

typedef struct sixel_lookup_fhedt_first_touch_plan_float32 {
    double *distances;
    int *sources;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_y;
    int tile_z;
    int tiles_y;
    int tiles_z;
} sixel_lookup_fhedt_first_touch_plan_float32_t;

typedef void (*sixel_lookup_fhedt_edt1d_float32_fn)(double *,
                                                   int *,
                                                   int,
                                                   double);

static SIXEL_FHEDT_TLS sixel_lookup_fhedt_cache_float32_t
    sixel_lookup_fhedt_thread_cache_float32;

static uint32_t
sixel_lookup_fhedt_cache_hash_float32(size_t offset)
{
    uint32_t state;
    uint64_t offset64;

    /*
     * Hash the full pointer-sized offset using a 64bit staging value so that
     * the high-half mix stays well-defined on 32bit builds.
     */
    offset64 = (uint64_t)offset;

    state = sixel_lookup_fhedt_mix_u32_float32(0x811c9dc5U,
                                      (uint32_t)offset64);
    state = sixel_lookup_fhedt_mix_u32_float32(state,
                                      (uint32_t)(offset64 >> 32));

    return state;
}

static void
sixel_lookup_fhedt_cache_clear_float32(sixel_lookup_fhedt_cache_float32_t *cache)
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
sixel_lookup_fhedt_cache_prepare_float32(
    sixel_lookup_fhedt_shared_float32_t const *shared)
{
    if (sixel_lookup_fhedt_thread_cache_float32.shared != shared
        || sixel_lookup_fhedt_thread_cache_float32.signature
            != shared->signature) {
        sixel_lookup_fhedt_cache_clear_float32(
            &sixel_lookup_fhedt_thread_cache_float32);
        sixel_lookup_fhedt_thread_cache_float32.shared = shared;
        sixel_lookup_fhedt_thread_cache_float32.signature = shared->signature;
    }
}

static void
sixel_lookup_fhedt_timeline_open_float32(
    sixel_lookup_fhedt_timeline_float32_t *timeline);

static int
sixel_lookup_fhedt_timeline_lines_enabled_float32(
    sixel_lookup_fhedt_timeline_float32_t *timeline)
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
sixel_lookup_fhedt_timeline_open_float32(
    sixel_lookup_fhedt_timeline_float32_t *timeline)
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
sixel_lookup_fhedt_timeline_close_float32(
    sixel_lookup_fhedt_timeline_float32_t *timeline)
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
sixel_lookup_fhedt_timeline_log_float32(
    sixel_lookup_fhedt_timeline_float32_t *timeline,
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

static void
sixel_lookup_fhedt_prefetch_line_float32(double *distances,
                                int *sources,
                                size_t offset,
                                sixel_lookup_fhedt_timeline_float32_t *timeline,
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
        sixel_lookup_fhedt_timeline_log_float32(timeline,
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
sixel_lookup_fhedt_cache_get_float32(sixel_lookup_fhedt_cache_float32_t *cache,
                            size_t offset,
                            int *value_out)
{
    uint32_t key;
    size_t set;
    size_t way;

    key = sixel_lookup_fhedt_cache_hash_float32(offset);
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
sixel_lookup_fhedt_cache_put_float32(sixel_lookup_fhedt_cache_float32_t *cache,
                            size_t offset,
                            int value)
{
    uint32_t key;
    size_t set;
    size_t way;

    key = sixel_lookup_fhedt_cache_hash_float32(offset);
    set = (size_t)(key & 15U);
    way = cache->sets[set].hand;
    cache->sets[set].key[way] = key;
    cache->sets[set].value[way] = value;
    cache->sets[set].hand = (uint8_t)((way + 1U) & 3U);
}

uint32_t
sixel_lookup_fhedt_float32_signature(float const *palette,
                                    int ncolors,
                                    int resolution,
                                    int refine,
                                    float wcomp1,
                                    float wcomp2,
                                    float wcomp3,
                                    int depth,
                                    int pixelformat)
{
    uint32_t hash;
    uint32_t bits;
    size_t total;
    size_t index;
    float normalized;
    float minimum[3];
    float range[3];
    int component;

    hash = 0x811c9dc5U;
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, (uint32_t)resolution);
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, (uint32_t)refine);
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, (uint32_t)ncolors);
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, (uint32_t)depth);
    memcpy(&bits, &wcomp1, sizeof(bits));
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, bits);
    memcpy(&bits, &wcomp2, sizeof(bits));
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, bits);
    memcpy(&bits, &wcomp3, sizeof(bits));
    hash = sixel_lookup_fhedt_mix_u32_float32(hash, bits);

    for (component = 0; component < 3; ++component) {
        minimum[component] = sixel_pixelformat_float_channel_min(pixelformat,
                                                                 component);
        range[component] = sixel_pixelformat_float_channel_range(pixelformat,
                                                                 component);
        if (range[component] <= 0.0f) {
            range[component] = 1.0f;
        }
    }

    total = (size_t)ncolors * (size_t)depth;
    for (index = 0U; index < total; ++index) {
        component = (int)(index % (size_t)depth);
        normalized = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                           component,
                                                           palette[index]);
        normalized = (normalized - minimum[component]) / range[component];
        if (normalized < 0.0f) {
            normalized = 0.0f;
        } else if (normalized > 1.0f) {
            normalized = 1.0f;
        }
        memcpy(&bits, &normalized, sizeof(bits));
        hash = sixel_lookup_fhedt_mix_u32_float32(hash, bits);
    }

    return hash;
}

uint32_t
sixel_lookup_fhedt_float32_shared_signature(
    sixel_lookup_fhedt_shared_float32_t const *shared)
{
    if (shared == NULL) {
        return 0U;
    }

    return shared->signature;
}

void
sixel_lookup_fhedt_float32_shared_set_signature(
    sixel_lookup_fhedt_shared_float32_t *shared,
    uint32_t signature)
{
    if (shared == NULL) {
        return;
    }

    shared->signature = signature;
}

static void
sixel_lookup_fhedt_shared_release_palette_float32(
    sixel_allocator_t *allocator,
    sixel_lookup_fhedt_shared_float32_t *shared)
{
    if (shared->palette != NULL) {
        sixel_allocator_free(allocator, shared->palette);
        shared->palette = NULL;
    }
    if (shared->palette_quant != NULL) {
        sixel_allocator_free(allocator, shared->palette_quant);
        shared->palette_quant = NULL;
    }
}

static void
sixel_lookup_fhedt_shared_release_indices_float32(
    sixel_allocator_t *allocator,
    sixel_lookup_fhedt_shared_float32_t *shared)
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
sixel_lookup_fhedt_shared_destroy_float32(sixel_allocator_t *allocator,
                                 sixel_lookup_fhedt_shared_float32_t *shared)
{
    if (shared == NULL) {
        return;
    }

    sixel_lookup_fhedt_shared_release_palette_float32(allocator, shared);
    sixel_lookup_fhedt_shared_release_indices_float32(allocator, shared);
    sixel_allocator_free(allocator, shared);
}

static void
sixel_lookup_fhedt_shared_unref_float32(sixel_allocator_t *allocator,
                               sixel_lookup_fhedt_shared_float32_t *shared)
{
    unsigned int previous;

    if (shared == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&shared->refcount, 1U);
    if (previous == 1U) {
        sixel_fence_acquire();
        sixel_lookup_fhedt_shared_destroy_float32(allocator, shared);
    }
}

static void
sixel_lookup_fhedt_shared_ref_float32(sixel_lookup_fhedt_shared_float32_t *shared)
{
    if (shared == NULL) {
        return;
    }

    sixel_atomic_fetch_add_u32(&shared->refcount, 1U);
}

static int
sixel_lookup_fhedt_palette_index_float32(int depth, int index, int component)
{
    return index * depth + component;
}

static void
sixel_lookup_fhedt_quantize_palette_float32(float const *palette,
                                   sixel_lookup_fhedt_shared_float32_t *shared)
{
    int index;
    int component;
    float sample;
    float normalized;
    int quantized;
    int limit;

    limit = shared->resolution - 1;
    for (index = 0; index < shared->ncolors; ++index) {
        for (component = 0; component < shared->depth; ++component) {
            sample = palette[sixel_lookup_fhedt_palette_index_float32(
                                   shared->depth,
                                   index,
                                   component)];
            normalized = sixel_lookup_fhedt_normalize_component_float32(
                shared,
                sample,
                component);
            quantized = (int)lroundf(normalized * (float)limit);
            if (quantized < 0) {
                quantized = 0;
            }
            if (quantized > limit) {
                quantized = limit;
            }
            shared->palette_quant[sixel_lookup_fhedt_palette_index_float32(
                                        shared->depth,
                                        index,
                                        component)]
                = (unsigned char)quantized;
            shared->palette[sixel_lookup_fhedt_palette_index_float32(
                                     shared->depth,
                                     index,
                                     component)]
                = sixel_pixelformat_float_channel_clamp(
                    shared->pixelformat,
                    component,
                    sample);
        }
    }
}

static int
sixel_lookup_fhedt_first_touch_worker_float32(sixel_thread_pool_job_t job,
                                     void *userdata,
                                     void *workspace)
{
    sixel_lookup_fhedt_first_touch_plan_float32_t *plan;
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

    plan = (sixel_lookup_fhedt_first_touch_plan_float32_t *)userdata;
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
sixel_lookup_fhedt_first_touch_float32(double *distances,
                              int *sources,
                              int res,
                              int threads,
                              int pin_threads,
                              int tile_xy,
                              int tile_depth)
{
    sixel_lookup_fhedt_first_touch_plan_float32_t plan;
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

    sixel_lookup_fhedt_dispatch_tiles_float32(
        tiles_y * tiles_z,
        threads,
        pin_threads,
        sixel_lookup_fhedt_first_touch_worker_float32,
        &plan);
}

static void
sixel_lookup_fhedt_seed_grid_float32(int resolution,
                            int depth,
                            int ncolors,
                            unsigned char const *palette_quant,
                            double *distances,
                            int *sources)
{
    size_t plane;
    size_t grid;
    int index;
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
        x = palette_quant[sixel_lookup_fhedt_palette_index_float32(
                                 depth,
                                 index,
                                 0)];
        y = palette_quant[sixel_lookup_fhedt_palette_index_float32(
                                 depth,
                                 index,
                                 1)];
        z = palette_quant[sixel_lookup_fhedt_palette_index_float32(
                                 depth,
                                 index,
                                 2)];
        plane = (size_t)resolution * (size_t)resolution;
        offset = ((size_t)z * plane) + ((size_t)y * (size_t)resolution)
               + (size_t)x;
        distances[offset] = 0.0;
        sources[offset] = index;
    }
}

static void
sixel_lookup_fhedt_edt1d_scalar_float32(double *line_dist,
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

static sixel_lookup_fhedt_edt1d_float32_fn
sixel_lookup_fhedt_edt1d_resolve_float32(void)
{
    static sixel_lookup_fhedt_edt1d_float32_fn selected;

    if (selected != NULL) {
        return selected;
    }
    selected = sixel_lookup_fhedt_edt1d_scalar_float32;

    return selected;
}

typedef struct sixel_lookup_fhedt_pass_x_plan_float32 {
    sixel_lookup_fhedt_shared_float32_t *shared;
    double *distances;
    int *sources;
    sixel_lookup_fhedt_timeline_float32_t *timeline;
    sixel_lookup_fhedt_edt1d_float32_fn edt1d;
    double weight;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_y;
    int tile_z;
    int tiles_y;
    int tiles_z;
    int log_lines;
} sixel_lookup_fhedt_pass_x_plan_float32_t;

typedef struct sixel_lookup_fhedt_pass_y_plan_float32 {
    sixel_lookup_fhedt_shared_float32_t *shared;
    double *distances;
    int *sources;
    sixel_lookup_fhedt_timeline_float32_t *timeline;
    sixel_lookup_fhedt_edt1d_float32_fn edt1d;
    double weight;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_x;
    int tile_z;
    int tiles_x;
    int tiles_z;
    int log_lines;
} sixel_lookup_fhedt_pass_y_plan_float32_t;

typedef struct sixel_lookup_fhedt_pass_z_plan_float32 {
    sixel_lookup_fhedt_shared_float32_t *shared;
    double *distances;
    int *sources;
    sixel_lookup_fhedt_timeline_float32_t *timeline;
    sixel_lookup_fhedt_edt1d_float32_fn edt1d;
    double weight;
    size_t stride_y;
    size_t stride_z;
    int res;
    int tile_x;
    int tile_y;
    int tiles_x;
    int tiles_y;
    int log_lines;
} sixel_lookup_fhedt_pass_z_plan_float32_t;

static int
sixel_lookup_fhedt_pass_x_worker_float32(sixel_thread_pool_job_t job,
                                void *userdata,
                                void *workspace)
{
    sixel_lookup_fhedt_pass_x_plan_float32_t *plan;
    double line_dist[256];
    int line_src[256];
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

    plan = (sixel_lookup_fhedt_pass_x_plan_float32_t *)userdata;
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
                sixel_lookup_fhedt_timeline_log_float32(plan->timeline,
                                               "fhedt-x",
                                               "line-start",
                                               z,
                                               y,
                                               message);
            }
            if (y + 1 < plan->res) {
                next_offset = offset + plan->stride_y;
                sixel_lookup_fhedt_prefetch_line_float32(plan->distances,
                                                plan->sources,
                                                next_offset,
                                                plan->timeline,
                                                "fhedt-x",
                                                z,
                                                y + 1);
            }
            for (x = 0; x < plan->res; ++x) {
                line_dist[x] = plan->distances[offset + (size_t)x];
                line_src[x] = plan->sources[offset + (size_t)x];
            }
            plan->edt1d(line_dist,
                        line_src,
                        plan->res,
                        plan->weight);
            for (x = 0; x < plan->res; ++x) {
                plan->distances[offset + (size_t)x] = line_dist[x];
                plan->sources[offset + (size_t)x] = line_src[x];
            }
            if (plan->log_lines != 0) {
                sixel_lookup_fhedt_timeline_log_float32(plan->timeline,
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
sixel_lookup_fhedt_pass_y_worker_float32(sixel_thread_pool_job_t job,
                                void *userdata,
                                void *workspace)
{
    sixel_lookup_fhedt_pass_y_plan_float32_t *plan;
    double line_dist[256];
    int line_src[256];
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

    plan = (sixel_lookup_fhedt_pass_y_plan_float32_t *)userdata;
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
                sixel_lookup_fhedt_timeline_log_float32(plan->timeline,
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
                    sixel_lookup_fhedt_prefetch_line_float32(plan->distances,
                                                    plan->sources,
                                                    next_offset,
                                                    plan->timeline,
                                                    "fhedt-y",
                                                    z,
                                                    x);
                }
                line_dist[y] = plan->distances[offset];
                line_src[y] = plan->sources[offset];
            }
            plan->edt1d(line_dist,
                        line_src,
                        plan->res,
                        plan->weight);
            for (y = 0; y < plan->res; ++y) {
                offset = ((size_t)z * plan->stride_z)
                       + ((size_t)y * plan->stride_y)
                       + (size_t)x;
                plan->distances[offset] = line_dist[y];
                plan->sources[offset] = line_src[y];
            }
            if (plan->log_lines != 0) {
                sixel_lookup_fhedt_timeline_log_float32(plan->timeline,
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
sixel_lookup_fhedt_pass_z_worker_float32(sixel_thread_pool_job_t job,
                                void *userdata,
                                void *workspace)
{
    sixel_lookup_fhedt_pass_z_plan_float32_t *plan;
    double line_dist[256];
    int line_src[256];
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

    plan = (sixel_lookup_fhedt_pass_z_plan_float32_t *)userdata;
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
                sixel_lookup_fhedt_timeline_log_float32(plan->timeline,
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
                    sixel_lookup_fhedt_prefetch_line_float32(plan->distances,
                                                    plan->sources,
                                                    next_offset,
                                                    plan->timeline,
                                                    "fhedt-z",
                                                    y,
                                                    x);
                }
                line_dist[z] = plan->distances[offset];
                line_src[z] = plan->sources[offset];
            }
            plan->edt1d(line_dist,
                        line_src,
                        plan->res,
                        plan->weight);
            for (z = 0; z < plan->res; ++z) {
                offset = ((size_t)z * plan->stride_z)
                       + ((size_t)y * plan->stride_y)
                       + (size_t)x;
                plan->distances[offset] = line_dist[z];
                plan->sources[offset] = line_src[z];
            }
            if (plan->log_lines != 0) {
                sixel_lookup_fhedt_timeline_log_float32(plan->timeline,
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

#if SIXEL_ENABLE_THREADS
static SIXELSTATUS
sixel_lookup_fhedt_create_pool_float32(
    sixel_thread_pool_t **pool,
    int threads,
    int queue_depth,
    sixel_thread_pool_worker_function_t worker,
    void *userdata)
{
    sixel_threadpool_service_t *service;
    sixel_thread_pool_create_request_t request;
    void *service_object;
    SIXELSTATUS status;

    if (pool != NULL) {
        *pool = NULL;
    }
    if (pool == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    service = NULL;
    service_object = NULL;
    status = sixel_components_getservice("services/threadpool",
                                         &service_object);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    service = (sixel_threadpool_service_t *)service_object;
    if (service == NULL || service->vtbl == NULL ||
        service->vtbl->create_pool == NULL) {
        if (service != NULL && service->vtbl != NULL &&
            service->vtbl->unref != NULL) {
            service->vtbl->unref(service);
        }
        return SIXEL_BAD_ARGUMENT;
    }

    request.threads = threads;
    request.queue_size = queue_depth;
    request.workspace_size = 0U;
    request.worker = worker;
    request.userdata = userdata;
    request.workspace_cleanup = NULL;
    status = service->vtbl->create_pool(service, &request, pool);
    if (service->vtbl->unref != NULL) {
        service->vtbl->unref(service);
    }

    return status;
}
#endif  /* SIXEL_ENABLE_THREADS */

static void
sixel_lookup_fhedt_dispatch_tiles_float32(int total_tiles,
                                 int threads,
                                 int pin_threads,
                                 sixel_thread_pool_worker_function_t worker,
                                 void *plan)
{
#if SIXEL_ENABLE_THREADS
    sixel_thread_pool_t *pool;
    int queue_depth;
    int job_index;
    sixel_thread_pool_job_t job;
    int status;

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
    status = sixel_lookup_fhedt_create_pool_float32(&pool,
                                                    threads,
                                                    queue_depth,
                                                    worker,
                                                    plan);
    if (SIXEL_FAILED(status)) {
        for (job_index = 0; job_index < total_tiles; ++job_index) {
            job.band_index = job_index;
            (void)worker(job, plan, NULL);
        }

        return;
    }
    pool->vtbl->set_affinity(pool, pin_threads);
    for (job_index = 0; job_index < total_tiles; ++job_index) {
        job.band_index = job_index;
        status = pool->vtbl->push(pool, job);
        if (SIXEL_FAILED(status)) {
            break;
        }
    }
    pool->vtbl->finish(pool);
    (void)pool->vtbl->get_error(pool);
    pool->vtbl->unref(pool);
#else
    sixel_thread_pool_job_t job;
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
sixel_lookup_fhedt_apply_edt_float32(sixel_lookup_fhedt_shared_float32_t *shared,
                            double *distances,
                            int *sources,
                            sixel_lookup_fhedt_timeline_float32_t *timeline,
                            int threads,
                            int pin_threads,
                            int tile_xy,
                            int tile_depth)
{
    sixel_lookup_fhedt_pass_x_plan_float32_t plan_x;
    sixel_lookup_fhedt_pass_y_plan_float32_t plan_y;
    sixel_lookup_fhedt_pass_z_plan_float32_t plan_z;
    int res;
    size_t plane;
    size_t stride_y;
    size_t stride_z;
    int tiles_y;
    int tiles_z;
    int tiles_x;
    sixel_lookup_fhedt_edt1d_float32_fn edt1d;
    int log_lines;

    res = shared->resolution;
    plane = (size_t)res * (size_t)res;
    stride_y = (size_t)res;
    stride_z = plane;
    edt1d = sixel_lookup_fhedt_edt1d_resolve_float32();
    log_lines = sixel_lookup_fhedt_timeline_lines_enabled_float32(timeline);

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

    sixel_lookup_fhedt_timeline_log_float32(timeline,
                                   "fhedt-x",
                                   "pass-start",
                                   -1,
                                   -1,
                                   "x-pass");
    sixel_lookup_fhedt_dispatch_tiles_float32(tiles_y * tiles_z,
                                     threads,
                                     pin_threads,
                                     sixel_lookup_fhedt_pass_x_worker_float32,
                                     &plan_x);
    sixel_lookup_fhedt_timeline_log_float32(timeline,
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

    sixel_lookup_fhedt_timeline_log_float32(timeline,
                                   "fhedt-y",
                                   "pass-start",
                                   -1,
                                   -1,
                                   "y-pass");
    sixel_lookup_fhedt_dispatch_tiles_float32(tiles_x * tiles_z,
                                     threads,
                                     pin_threads,
                                     sixel_lookup_fhedt_pass_y_worker_float32,
                                     &plan_y);
    sixel_lookup_fhedt_timeline_log_float32(timeline,
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

    sixel_lookup_fhedt_timeline_log_float32(timeline,
                                   "fhedt-z",
                                   "pass-start",
                                   -1,
                                   -1,
                                   "z-pass");
    sixel_lookup_fhedt_dispatch_tiles_float32(
        tiles_x * tiles_y,
        threads,
        pin_threads,
        sixel_lookup_fhedt_pass_z_worker_float32,
        &plan_z);
    sixel_lookup_fhedt_timeline_log_float32(timeline,
                                   "fhedt-z",
                                   "pass-end",
                                   -1,
                                   -1,
                                   "z-pass");
}

static void
sixel_lookup_fhedt_fill_indices_float32(
    sixel_lookup_fhedt_shared_float32_t *shared,
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
sixel_lookup_fhedt_mark_boundaries_float32(
    sixel_lookup_fhedt_shared_float32_t *shared,
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
                    neighbor = sources[offset + (size_t)dz * plane];
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
                    neighbor = sources[offset + (size_t)dy * (size_t)res];
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
                    neighbor = sources[offset + (size_t)dx];
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
sixel_lookup_fhedt_float32_build(sixel_lookup_fhedt_float32_t *fhedt,
                                float const *palette,
                                int ncolors,
                                int resolution,
                                int refine,
                                int use_dist2,
                                float wcomp1,
                                float wcomp2,
                                float wcomp3,
                                int depth,
                                int pixelformat)
{
    sixel_lookup_fhedt_shared_float32_t *shared;
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
    sixel_lookup_fhedt_timeline_float32_t timeline;
    char timeline_message[128];

    timeline.initialized = 0;
    shared = sixel_allocator_malloc(fhedt->allocator, sizeof(*shared));
    if (shared == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_float32_build: allocation failed (shared).");
        sixel_lookup_fhedt_timeline_close_float32(&timeline);
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
    shared->pixelformat = pixelformat;
    shared->safe_radius2 = 0.0;
    shared->use_u16 = ncolors > 256 ? 1 : 0;
    shared->dist2 = NULL;
    shared->indices8 = NULL;
    shared->indices16 = NULL;
    shared->boundary = NULL;
    shared->palette = NULL;
    shared->palette_quant = NULL;

    palette_size = (size_t)ncolors * (size_t)depth;
    shared->palette = (float *)sixel_allocator_malloc(fhedt->allocator,
                                                      palette_size
                                                      * sizeof(float));
    shared->palette_quant = (unsigned char *)sixel_allocator_malloc(
        fhedt->allocator,
        palette_size);
    if (shared->palette == NULL || shared->palette_quant == NULL) {
        sixel_lookup_fhedt_shared_destroy_float32(fhedt->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_float32_build: palette allocation failed.");
        sixel_lookup_fhedt_timeline_close_float32(&timeline);
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_lookup_fhedt_prepare_ranges_float32(shared, pixelformat);
    sixel_lookup_fhedt_quantize_palette_float32(palette, shared);

    threads = sixel_lookup_fhedt_resolve_threads_float32();
    pin_threads = sixel_lookup_fhedt_pin_threads_enabled_float32();
    first_touch = sixel_lookup_fhedt_first_touch_enabled_float32();
    sixel_lookup_fhedt_resolve_tiles_float32(palette,
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
        sixel_lookup_fhedt_shared_destroy_float32(fhedt->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_float32_build: LUT allocation failed.");
        sixel_lookup_fhedt_timeline_close_float32(&timeline);
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
        sixel_lookup_fhedt_shared_destroy_float32(fhedt->allocator, shared);
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_float32_build: temporary buffer "
            "allocation failed.");
        sixel_lookup_fhedt_timeline_close_float32(&timeline);
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_lookup_fhedt_timeline_open_float32(&timeline);
    /* Tag the FHEDT build so timeline.py surfaces backend selection. */
    (void)snprintf(timeline_message,
                   sizeof(timeline_message),
                   "res=%d colors=%d refine=%d dist2=%d",
                   resolution,
                   ncolors,
                   refine,
                   use_dist2);
    sixel_lookup_fhedt_timeline_log_float32(&timeline,
                                   "fhedt",
                                   "builder-start",
                                   resolution,
                                   ncolors,
                                   timeline_message);
    if (first_touch != 0) {
        sixel_lookup_fhedt_first_touch_float32(distances,
                                      sources,
                                      resolution,
                                      threads,
                                      pin_threads,
                                      tile_xy,
                                      tile_depth);
    }
    sixel_lookup_fhedt_seed_grid_float32(resolution,
                                shared->depth,
                                shared->ncolors,
                                shared->palette_quant,
                                distances,
                                sources);
    sixel_lookup_fhedt_apply_edt_float32(shared,
                                distances,
                                sources,
                                &timeline,
                                threads,
                                pin_threads,
                                tile_xy,
                                tile_depth);
    sixel_lookup_fhedt_fill_indices_float32(shared, sources);
    sixel_lookup_fhedt_mark_boundaries_float32(shared, sources);
    if (shared->dist2 != NULL) {
        for (offset = 0U; offset < total; ++offset) {
            shared->dist2[offset] = (float)distances[offset];
        }
    }

    sixel_lookup_fhedt_timeline_log_float32(&timeline,
                                   "fhedt",
                                   "builder-end",
                                   resolution,
                                   ncolors,
                                   timeline_message);
    sixel_allocator_free(fhedt->allocator, distances);
    sixel_allocator_free(fhedt->allocator, sources);
    sixel_lookup_fhedt_timeline_close_float32(&timeline);

    /*
     * Each voxel spans a unit cube in lattice space.
     * The farthest point from the center sits 0.5 units away on every axis,
     * so this radius bounds the squared displacement a pixel can accumulate
     * inside the cell.
     */
    shared->safe_radius2 = ((double)wcomp1 * 0.25)
                         + ((double)wcomp2 * 0.25)
                         + ((double)wcomp3 * 0.25);

    fhedt->shared = shared;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_fhedt_float32_create(sixel_allocator_t *allocator,
                                 sixel_lookup_fhedt_float32_t **fhedt_out)
{
    sixel_lookup_fhedt_float32_t *fhedt;

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
sixel_lookup_fhedt_float32_release_shared(sixel_lookup_fhedt_float32_t *fhedt)
{
    sixel_allocator_t *allocator;

    if (fhedt == NULL || fhedt->shared == NULL) {
        return;
    }

    allocator = fhedt->allocator;
    sixel_lookup_fhedt_shared_unref_float32(allocator, fhedt->shared);
    if (fhedt->shared_published != 0) {
        sixel_lookup_fhedt_shared_unref_float32(allocator, fhedt->shared);
    }
    fhedt->shared = NULL;
    fhedt->shared_published = 0;
}

void
sixel_lookup_fhedt_float32_unref(sixel_lookup_fhedt_float32_t *fhedt)
{
    if (fhedt == NULL) {
        return;
    }

    sixel_lookup_fhedt_float32_release_shared(fhedt);
    sixel_allocator_free(fhedt->allocator, fhedt);
}

SIXELSTATUS
sixel_lookup_fhedt_float32_configure(sixel_lookup_fhedt_float32_t *fhedt,
                                    float const *palette,
                                    int ncolors,
                                    int resolution,
                                    int refine,
                                    int use_dist2,
                                    int use_cache,
                                    int shared_flag,
                                    float wcomp1,
                                    float wcomp2,
                                    float wcomp3,
                                    int pixelformat,
                                    int parallel_dither_active)
{
    SIXELSTATUS status;

    (void)pixelformat;

    if (fhedt == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_lookup_fhedt_validate_resolution_float32(resolution)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_fhedt_float32_configure: resolution must "
            "be 64/128/256.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (fhedt->shared != NULL) {
        sixel_lookup_fhedt_float32_release_shared(fhedt);
    }

    status = sixel_lookup_fhedt_float32_build(fhedt,
                                             palette,
                                             ncolors,
                                             resolution,
                                             refine,
                                             use_dist2,
                                             wcomp1,
                                             wcomp2,
                                             wcomp3,
                                             3,
                                             pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (shared_flag != 0) {
        sixel_lookup_fhedt_shared_ref_float32(fhedt->shared);
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
sixel_lookup_fhedt_read_index_float32(
    sixel_lookup_fhedt_shared_float32_t const *shared,
    size_t offset)
{
    if (!shared->use_u16) {
        return (int)shared->indices8[offset];
    }

    return (int)shared->indices16[offset];
}

static int
sixel_lookup_fhedt_boundary_bit_float32(
    sixel_lookup_fhedt_shared_float32_t const *shared,
    size_t offset)
{
    size_t byte_index;
    size_t bit_index;

    byte_index = offset / 8U;
    bit_index = offset % 8U;

    return (shared->boundary[byte_index] >> bit_index) & 1U;
}

static int
sixel_lookup_fhedt_refine_needed_float32(
    sixel_lookup_fhedt_shared_float32_t const *shared,
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
sixel_lookup_fhedt_refine_candidates_float32(
    sixel_lookup_fhedt_shared_float32_t const *shared,
    float const *pixel,
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
    double weight1;
    double weight2;
    double weight3;
    float p1;
    float p2;
    float p3;
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
    weight1 = (double)shared->weights[0];
    weight2 = (double)shared->weights[1];
    weight3 = (double)shared->weights[2];
    best_index = -1;
    best_distance = DBL_MAX;
    used_count = 0;
    for (cz = 0; cz < 2; ++cz) {
        for (cy = 0; cy < 2; ++cy) {
            for (cx = 0; cx < 2; ++cx) {
                offset = ((size_t)corner_z[cz] * plane)
                       + ((size_t)corner_y[cy] * (size_t)shared->resolution)
                       + (size_t)corner_x[cx];
                candidate = sixel_lookup_fhedt_read_index_float32(shared, offset);

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

                p1 = shared->palette[sixel_lookup_fhedt_palette_index_float32(
                    shared->depth,
                    candidate,
                    0)];
                p2 = shared->palette[sixel_lookup_fhedt_palette_index_float32(
                    shared->depth,
                    candidate,
                    1)];
                p3 = shared->palette[sixel_lookup_fhedt_palette_index_float32(
                    shared->depth,
                    candidate,
                    2)];
                dx = (double)(pixel[0] - p1);
                dy = (double)(pixel[1] - p2);
                dz = (double)(pixel[2] - p3);
                distance = weight1 * dx * dx
                         + weight2 * dy * dy
                         + weight3 * dz * dz;
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
sixel_lookup_fhedt_float32_map(sixel_lookup_fhedt_float32_t *fhedt,
                              float const *pixel)
{
    int x;
    int y;
    int z;
    int limit;
    int cached_value;
    int should_refine;
    int cache_active;
    int index;
    size_t offset;
    size_t plane;
    float scaled;

    if (fhedt == NULL || pixel == NULL || fhedt->shared == NULL) {
        return -1;
    }

    limit = fhedt->shared->resolution - 1;
    scaled = sixel_lookup_fhedt_normalize_component_float32(fhedt->shared,
                                                           pixel[0],
                                                           0)
             * (float)limit;
    x = (int)lroundf(scaled);
    if (x > limit) {
        x = limit;
    }
    scaled = sixel_lookup_fhedt_normalize_component_float32(fhedt->shared,
                                                           pixel[1],
                                                           1)
             * (float)limit;
    y = (int)lroundf(scaled);
    if (y > limit) {
        y = limit;
    }
    scaled = sixel_lookup_fhedt_normalize_component_float32(fhedt->shared,
                                                           pixel[2],
                                                           2)
             * (float)limit;
    z = (int)lroundf(scaled);
    if (z > limit) {
        z = limit;
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
        sixel_lookup_fhedt_cache_prepare_float32(fhedt->shared);
        if (sixel_lookup_fhedt_cache_get_float32(&sixel_lookup_fhedt_thread_cache_float32,
                                         offset,
                                         &cached_value)) {
            return cached_value;
        }
    }

    index = sixel_lookup_fhedt_read_index_float32(fhedt->shared, offset);
    if (fhedt->shared->refine != 0
        && sixel_lookup_fhedt_boundary_bit_float32(fhedt->shared, offset) != 0) {
        should_refine = sixel_lookup_fhedt_refine_needed_float32(fhedt->shared, offset);
        if (should_refine != 0) {
            index = sixel_lookup_fhedt_refine_candidates_float32(fhedt->shared,
                                                        pixel,
                                                        x,
                                                        y,
                                                        z);
        }
    }

    if (cache_active != 0) {
        sixel_lookup_fhedt_cache_put_float32(&sixel_lookup_fhedt_thread_cache_float32,
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
