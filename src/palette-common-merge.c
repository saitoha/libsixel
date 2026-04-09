/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Shared final merge utilities for palette quantizers.  The helpers centralise
 * environment handling and the Ward refinement logic so palette-heckbert.c,
 * palette-kmeans.c, and palette.c can share a consistent implementation
 * without depending on each other's internals.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "allocator.h"
#include "compat_stub.h"
#include "lookup-common.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans.h"
#include "status.h"


#if defined(_MSC_VER)
# define SIXEL_TLS __declspec(thread)
# define SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_TLS _Thread_local
# define SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_TLS __thread
# define SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE 1
#else
# define SIXEL_TLS
# define SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE 0
#endif

static SIXEL_TLS int sixel_kmeans_threshold_override_enabled = 0;
static SIXEL_TLS double sixel_kmeans_threshold_override_value = 0.125;
static SIXEL_TLS int sixel_kmeans_iter_max_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_kmeans_iter_max_override_value = 20u;
static SIXEL_TLS int sixel_final_merge_target_factor_override_enabled = 0;
static SIXEL_TLS double sixel_final_merge_target_factor_override_value = 1.81;
static SIXEL_TLS int sixel_final_merge_lloyd_override_enabled = 0;
static SIXEL_TLS unsigned int sixel_final_merge_lloyd_override_value = 3u;

#undef SIXEL_TLS

static float env_final_merge_target_factor = 1.81f;
static unsigned int env_final_merge_additional_lloyd = 3U;
static unsigned int env_kmeans_iter_max = 20U;
static double env_kmeans_threshold = 0.125;
static double env_lumin_factor_r = 0.2989;
static double env_lumin_factor_g = 0.5866;
static double env_lumin_factor_b = 0.1145;
static double env_final_merge_channel_factor_l = 1.0 / 3.0;
static int env_final_merge_additional_lloyd_overridden = 0;
static int env_final_merge_env_loaded = 0;

#if SIXEL_ENABLE_THREADS
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
static CRITICAL_SECTION sixel_final_merge_env_mutex;
static INIT_ONCE sixel_final_merge_env_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_final_merge_env_lock_init_once(PINIT_ONCE once,
                                     PVOID parameter,
                                     PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&sixel_final_merge_env_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
static pthread_mutex_t sixel_final_merge_env_mutex = PTHREAD_MUTEX_INITIALIZER;
# endif
#endif

static int
sixel_final_merge_env_lock_acquire(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&sixel_final_merge_env_once,
                                      sixel_final_merge_env_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        return 0;
    }
    EnterCriticalSection(&sixel_final_merge_env_mutex);
    return 1;
# else
    int rc;

    rc = pthread_mutex_lock(&sixel_final_merge_env_mutex);
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
sixel_final_merge_env_lock_release(int acquired)
{
    if (acquired == 0) {
        return;
    }
#if SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&sixel_final_merge_env_mutex);
# else
    (void)pthread_mutex_unlock(&sixel_final_merge_env_mutex);
# endif
#endif
}

/*
 * Reuse the environment lock when TLS is unavailable.  In that configuration
 * override storage degrades to process globals and requires serialization.
 */
static int
sixel_final_merge_override_lock_acquire(void)
{
#if SIXEL_ENABLE_THREADS && !SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE
    return sixel_final_merge_env_lock_acquire();
#else
    return 0;
#endif
}

static void
sixel_final_merge_override_lock_release(int acquired)
{
#if SIXEL_ENABLE_THREADS && !SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE
    sixel_final_merge_env_lock_release(acquired);
#else
    (void)acquired;
#endif
}

#undef SIXEL_FINAL_MERGE_OVERRIDE_TLS_AVAILABLE

/*
 * Internal statistics accumulator used while clustering provisional
 * palette entries.  The tuple stores RGB centroids along with the
 * aggregate weight gathered during histogram analysis.
 */
struct sixel_final_merge_cluster {
    double r;
    double g;
    double b;
    double count;
};

static double
sixel_final_merge_distance_sq(sixel_final_merge_cluster_t const *lhs,
                              sixel_final_merge_cluster_t const *rhs,
                              int pixelformat,
                              int use_lab_weight);
static int sixel_final_merge_is_lab_colorspace(int pixelformat);
static void sixel_final_merge_clusters(sixel_final_merge_cluster_t *clusters,
                                       int nclusters,
                                       int target_k,
                                       int merge_mode,
                                       int use_reversible,
                                       int pixelformat);
static void sixel_final_merge_ward(sixel_final_merge_cluster_t *clusters,
                                   int nclusters,
                                   int target_k,
                                   int use_reversible,
                                   int pixelformat);

/*
 * Resolve auto merge requests.  The ladder clarifies the mapping:
 *
 *   AUTO -> NONE
 *   NONE -> NONE
 *   WARD -> WARD
 *
 * The default behaviour therefore leaves post-merge reduction disabled unless
 * the caller explicitly opts in.
 */
int
sixel_resolve_final_merge_mode(int final_merge_mode)
{
    int resolved;

    resolved = final_merge_mode;
    if (resolved == SIXEL_FINAL_MERGE_AUTO) {
        resolved = SIXEL_FINAL_MERGE_NONE;
    }

    return resolved;
}

/*
 * Inspect palette environment overrides exactly once so both the median-cut
 * and k-means pipelines see the same runtime configuration.
 */
void
sixel_final_merge_load_env(void)
{
    char const *env_value;
    char *endptr;
    double parsed_factor;
    double parsed_threshold;
    double parsed_component;
    double parsed_channel_factor;
    double candidate_r;
    double candidate_g;
    double candidate_b;
    long parsed_iters;
    long parsed_limit;
    int r_overridden;
    int g_overridden;
    int lock_acquired;

    env_value = NULL;
    endptr = NULL;
    parsed_factor = 0.0;
    parsed_threshold = 0.0;
    parsed_component = 0.0;
    parsed_channel_factor = 1.0 / 3.0;
    candidate_r = env_lumin_factor_r;
    candidate_g = env_lumin_factor_g;
    candidate_b = env_lumin_factor_b;
    parsed_iters = 0L;
    parsed_limit = 0L;
    r_overridden = 0;
    g_overridden = 0;
    lock_acquired = 0;
    lock_acquired = sixel_final_merge_env_lock_acquire();
    if (env_final_merge_env_loaded) {
        sixel_final_merge_env_lock_release(lock_acquired);
        return;
    }
    env_final_merge_env_loaded = 1;

    env_value = sixel_compat_getenv("SIXEL_PALETTE_OVERSPLIT_FACTOR");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_factor = strtod(env_value, &endptr);
        if (endptr != env_value && errno == 0) {
            if (parsed_factor < 1.0) {
                parsed_factor = 1.0;
            }
            if (parsed_factor > 3.0) {
                parsed_factor = 3.0;
            }
            env_final_merge_target_factor = (float)parsed_factor;
        }
    }

    env_value = sixel_compat_getenv(
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_iters = strtol(env_value, &endptr, 10);
        if (endptr != env_value && errno == 0) {
            if (parsed_iters < 0L) {
                parsed_iters = 0L;
            }
            if (parsed_iters > 30L) {
                parsed_iters = 30L;
            }
            env_final_merge_additional_lloyd
                = (unsigned int)parsed_iters;
            env_final_merge_additional_lloyd_overridden = 1;
        }
    }

    env_value = sixel_compat_getenv(
        "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_limit = strtol(env_value, &endptr, 10);
        if (endptr != env_value && errno == 0) {
            if (parsed_limit < 1L) {
                parsed_limit = 1L;
            }
            if (parsed_limit > 100L) {
                parsed_limit = 100L;
            }
            env_kmeans_iter_max = (unsigned int)parsed_limit;
        }
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_KMEANS_THRESHOLD");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_threshold = strtod(env_value, &endptr);
        if (endptr != env_value && errno == 0) {
            if (parsed_threshold < 0.0) {
                parsed_threshold = 0.0;
            }
            if (parsed_threshold > 0.5) {
                parsed_threshold = 0.5;
            }
            env_kmeans_threshold = parsed_threshold;
        }
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_LUMIN_FACTOR_R");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_component = strtod(env_value, &endptr);
        if (endptr != env_value && errno == 0) {
            if (parsed_component < 0.0) {
                parsed_component = 0.0;
            }
            if (parsed_component > 1.0) {
                parsed_component = 1.0;
            }
            candidate_r = parsed_component;
            r_overridden = 1;
        }
    }

    env_value = sixel_compat_getenv("SIXEL_PALETTE_LUMIN_FACTOR_G");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_component = strtod(env_value, &endptr);
        if (endptr != env_value && errno == 0) {
            if (parsed_component < 0.0) {
                parsed_component = 0.0;
            }
            if (parsed_component > 1.0) {
                parsed_component = 1.0;
            }
            candidate_g = parsed_component;
            g_overridden = 1;
        }
    }

    if (r_overridden || g_overridden) {
        candidate_b = 1.0 - candidate_r - candidate_g;
        if (candidate_b >= 0.0) {
            env_lumin_factor_r = candidate_r;
            env_lumin_factor_g = candidate_g;
            env_lumin_factor_b = candidate_b;
        } else {
            env_lumin_factor_r = 0.2989;
            env_lumin_factor_g = 0.5866;
            env_lumin_factor_b = 0.1145;
        }
    }

    /*
     * Lab-family distances can emphasise lightness with
     * SIXEL_PALETTE_CHANNEL_FACTOR_L.  Final-merge-specific overrides are
     * taken from SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L so snap and merge can
     * be tuned independently.
     */
    env_value = sixel_compat_getenv("SIXEL_PALETTE_CHANNEL_FACTOR_L");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_channel_factor = strtod(env_value, &endptr);
        if (endptr == env_value || errno != 0) {
            parsed_channel_factor = 1.0 / 3.0;
        }
    }
    env_value = sixel_compat_getenv("SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_channel_factor = strtod(env_value, &endptr);
        if (endptr == env_value || errno != 0) {
            parsed_channel_factor = 1.0 / 3.0;
        }
    }
    if (parsed_channel_factor < 0.0) {
        parsed_channel_factor = 0.0;
    }
    if (parsed_channel_factor > 1.0) {
        parsed_channel_factor = 1.0;
    }
    env_final_merge_channel_factor_l = parsed_channel_factor;
    sixel_final_merge_env_lock_release(lock_acquired);
}

/*
 * Provide accessor functions for k-means tuning knobs.  The helpers guarantee
 * that environment overrides are processed exactly once before the values are
 * observed by palette-kmeans.c.
 */
unsigned int
sixel_palette_kmeans_iter_max(void)
{
    unsigned int override_iter_max;
    int override_enabled;
    int lock_acquired;

    override_iter_max = 0u;
    override_enabled = 0;
    lock_acquired = sixel_final_merge_override_lock_acquire();
    override_enabled = sixel_kmeans_iter_max_override_enabled;
    override_iter_max = sixel_kmeans_iter_max_override_value;
    sixel_final_merge_override_lock_release(lock_acquired);

    if (override_enabled) {
        if (override_iter_max < 1u) {
            return 1u;
        }
        if (override_iter_max > 100u) {
            return 100u;
        }
        return override_iter_max;
    }

    sixel_final_merge_load_env();

    return env_kmeans_iter_max;
}

void
sixel_set_kmeans_iter_max_override(int enabled,
                                   unsigned int iter_max)
{
    int lock_acquired;

    lock_acquired = sixel_final_merge_override_lock_acquire();
    sixel_kmeans_iter_max_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_iter_max_override_value = iter_max;
    sixel_final_merge_override_lock_release(lock_acquired);
}

void
sixel_set_kmeans_threshold_override(int enabled,
                                   double threshold)
{
    int lock_acquired;

    lock_acquired = sixel_final_merge_override_lock_acquire();
    sixel_kmeans_threshold_override_enabled = enabled ? 1 : 0;
    sixel_kmeans_threshold_override_value = threshold;
    sixel_final_merge_override_lock_release(lock_acquired);
}

double
sixel_palette_kmeans_threshold(void)
{
    double override_threshold;
    int override_enabled;
    int lock_acquired;

    override_threshold = 0.0;
    override_enabled = 0;
    lock_acquired = sixel_final_merge_override_lock_acquire();
    override_enabled = sixel_kmeans_threshold_override_enabled;
    override_threshold = sixel_kmeans_threshold_override_value;
    sixel_final_merge_override_lock_release(lock_acquired);

    if (override_enabled) {
        return override_threshold;
    }

    sixel_final_merge_load_env();

    return env_kmeans_threshold;
}

void
sixel_set_final_merge_target_factor_override(int enabled,
                                             double factor)
{
    int lock_acquired;

    lock_acquired = sixel_final_merge_override_lock_acquire();
    sixel_final_merge_target_factor_override_enabled = enabled ? 1 : 0;
    sixel_final_merge_target_factor_override_value = factor;
    sixel_final_merge_override_lock_release(lock_acquired);
}

void
sixel_set_final_merge_lloyd_iterations_override(int enabled,
                                                unsigned int iterations)
{
    int lock_acquired;

    lock_acquired = sixel_final_merge_override_lock_acquire();
    sixel_final_merge_lloyd_override_enabled = enabled ? 1 : 0;
    sixel_final_merge_lloyd_override_value = iterations;
    sixel_final_merge_override_lock_release(lock_acquired);
}

unsigned int
sixel_final_merge_lloyd_iterations(int merge_mode)
{
    unsigned int override_iterations;
    int override_enabled;
    int lock_acquired;

    (void)merge_mode;

    override_iterations = 0u;
    override_enabled = 0;
    lock_acquired = sixel_final_merge_override_lock_acquire();
    override_enabled = sixel_final_merge_lloyd_override_enabled;
    override_iterations = sixel_final_merge_lloyd_override_value;
    sixel_final_merge_override_lock_release(lock_acquired);

    if (override_enabled) {
        if (override_iterations > 30u) {
            return 30u;
        }
        return override_iterations;
    }

    sixel_final_merge_load_env();
    if (env_final_merge_additional_lloyd_overridden) {
        return env_final_merge_additional_lloyd;
    }

    return 3U;
}

static double
sixel_final_merge_distance_sq(sixel_final_merge_cluster_t const *lhs,
                              sixel_final_merge_cluster_t const *rhs,
                              int pixelformat,
                              int use_lab_weight)
{
    double dr;
    double dg;
    double db;
    double channel_factor;
    double chroma_weight;
    double distance;
    int lab_family;

    dr = 0.0;
    dg = 0.0;
    db = 0.0;
    channel_factor = 0.0;
    chroma_weight = 0.0;
    distance = 0.0;
    lab_family = 0;
    if (lhs == NULL || rhs == NULL) {
        return 0.0;
    }
    dr = lhs->r - rhs->r;
    dg = lhs->g - rhs->g;
    db = lhs->b - rhs->b;
    if (use_lab_weight) {
        lab_family = sixel_final_merge_is_lab_colorspace(pixelformat);
    }
    if (use_lab_weight && lab_family) {
        sixel_final_merge_load_env();
        channel_factor = env_final_merge_channel_factor_l;
        chroma_weight = 1.0 - channel_factor;
        distance = channel_factor * dr * dr
                   + chroma_weight * 0.5 * dg * dg
                   + chroma_weight * 0.5 * db * db;
    } else {
        distance = dr * dr + dg * dg + db * db;
    }

    return distance;
}

static int
sixel_final_merge_is_lab_colorspace(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return 1;
    default:
        return 0;
    }
}

static void
sixel_final_merge_clusters(sixel_final_merge_cluster_t *clusters,
                           int nclusters,
                           int target_k,
                           int merge_mode,
                           int use_reversible,
                           int pixelformat)
{
    if (clusters == NULL || nclusters <= 0) {
        return;
    }
    switch (merge_mode) {
    case SIXEL_FINAL_MERGE_DISPATCH_WARD:
        sixel_final_merge_ward(
            clusters, nclusters, target_k, use_reversible, pixelformat);
        break;
    case SIXEL_FINAL_MERGE_DISPATCH_NONE:
    default:
        break;
    }
}

static void
sixel_final_merge_ward(sixel_final_merge_cluster_t *clusters,
                       int nclusters,
                       int target_k,
                       int use_reversible,
                       int pixelformat)
{
    int desired;
    int i;
    int j;
    int k;
    int best_i;
    int best_j;
    int active;
    double wi;
    double wj;
    double merged_weight;
    double cost;
    double distance_sq;
    sixel_final_merge_cluster_t merged;

    (void)use_reversible;
    desired = target_k;
    i = 0;
    j = 0;
    k = 0;
    best_i = -1;
    best_j = -1;
    active = 0;
    wi = 0.0;
    wj = 0.0;
    merged_weight = 0.0;
    cost = 0.0;
    distance_sq = 0.0;
    merged.r = 0.0;
    merged.g = 0.0;
    merged.b = 0.0;
    merged.count = 0.0;
    if (clusters == NULL || nclusters <= 0) {
        return;
    }
    if (desired < 1) {
        desired = 1;
    }
    for (i = 0; i < nclusters; ++i) {
        if (clusters[i].count > 0.0) {
            ++active;
        }
    }
    if (active <= desired) {
        for (i = active; i < nclusters; ++i) {
            clusters[i].r = 0.0;
            clusters[i].g = 0.0;
            clusters[i].b = 0.0;
            clusters[i].count = 0.0;
        }
        return;
    }
    while (active > desired) {
        best_i = -1;
        best_j = -1;
        cost = DBL_MAX;
        for (i = 0; i < nclusters; ++i) {
            wi = clusters[i].count;
            if (wi <= 0.0) {
                continue;
            }
            for (j = i + 1; j < nclusters; ++j) {
                wj = clusters[j].count;
                if (wj <= 0.0) {
                    continue;
                }
                distance_sq = sixel_final_merge_distance_sq(
                    &clusters[i], &clusters[j], pixelformat, 1);
                merged_weight = wi + wj;
                if (merged_weight <= 0.0) {
                    continue;
                }
                distance_sq *= wi * wj / merged_weight;
                if (distance_sq < cost) {
                    cost = distance_sq;
                    best_i = i;
                    best_j = j;
                }
            }
        }
        if (best_i < 0 || best_j < 0) {
            break;
        }
        wi = clusters[best_i].count;
        wj = clusters[best_j].count;
        merged_weight = wi + wj;
        if (merged_weight <= 0.0) {
            merged_weight = 1.0;
        }
        merged.count = merged_weight;
        merged.r = (clusters[best_i].r * wi + clusters[best_j].r * wj)
            / merged_weight;
        merged.g = (clusters[best_i].g * wi + clusters[best_j].g * wj)
            / merged_weight;
        merged.b = (clusters[best_i].b * wi + clusters[best_j].b * wj)
            / merged_weight;
        clusters[best_i] = merged;
        clusters[best_j].count = 0.0;
        clusters[best_j].r = 0.0;
        clusters[best_j].g = 0.0;
        clusters[best_j].b = 0.0;
        --active;
    }
    for (k = 0; k < nclusters; ++k) {
        if (clusters[k].count <= 0.0) {
            clusters[k].r = 0.0;
            clusters[k].g = 0.0;
            clusters[k].b = 0.0;
        }
    }

    (void)use_reversible;
    (void)pixelformat;
}

/* Determine how many clusters to create before the final merge step. */
unsigned int
sixel_final_merge_target(unsigned int reqcolors, int final_merge_mode)
{
    double factor;
    double override_factor;
    unsigned int scaled;
    int resolved;
    int override_enabled;
    int lock_acquired;

    sixel_final_merge_load_env();
    resolved = sixel_resolve_final_merge_mode(final_merge_mode);
    if (resolved != SIXEL_FINAL_MERGE_WARD) {
        return reqcolors;
    }
    factor = env_final_merge_target_factor;
    override_factor = 0.0;
    override_enabled = 0;
    lock_acquired = sixel_final_merge_override_lock_acquire();
    override_enabled = sixel_final_merge_target_factor_override_enabled;
    override_factor = sixel_final_merge_target_factor_override_value;
    sixel_final_merge_override_lock_release(lock_acquired);

    if (override_enabled) {
        factor = override_factor;
        if (factor < 1.0) {
            factor = 1.0;
        }
        if (factor > 3.0) {
            factor = 3.0;
        }
    }
    scaled = (unsigned int)((double)reqcolors * factor);
    if (scaled <= reqcolors) {
        scaled = reqcolors;
    }
    if (scaled < 1U) {
        scaled = 1U;
    }

    return scaled;
}

/* Merge clusters using either Ward linkage or hierarchical k-means. */
int
sixel_palette_apply_merge(unsigned long *weights,
                          double *sums,
                          unsigned int depth,
                          int cluster_count,
                          int target,
                          int final_merge_mode,
                          int use_reversible,
                          int pixelformat,
                          sixel_allocator_t *allocator)
{
    sixel_final_merge_cluster_t *clusters;
    double component;
    double component_b;
    double component_g;
    double component_r;
    double scaled;
    double used_weight;
    double weight;
    int index;
    int limit;
    int plane;
    int resolved_mode;
    sixel_final_merge_dispatch_t dispatch_mode;
    int result;
    /*
     * The caller supplies population counts alongside channel sums expressed
     * in the standard 0-255 domain (but kept in double precision).  Retaining
     * the fractional parts lets the float32 pipeline keep its extra accuracy
     * until the very last step that snaps to the SIXEL tone grid.
     */
    if (weights == NULL || sums == NULL) {
        return cluster_count;
    }
    if (cluster_count <= 0) {
        return 0;
    }
    clusters = (sixel_final_merge_cluster_t *)sixel_allocator_malloc(
        allocator,
        (size_t)cluster_count * sizeof(sixel_final_merge_cluster_t));
    if (clusters == NULL) {
        return cluster_count;
    }
    for (index = 0; index < cluster_count; ++index) {
        weight = (double)weights[index];
        clusters[index].count = weight;
        if (weight <= 0.0) {
            clusters[index].r = 0.0;
            clusters[index].g = 0.0;
            clusters[index].b = 0.0;
            continue;
        }
        component_r = 0.0;
        component_g = 0.0;
        component_b = 0.0;
        if (depth > 0U) {
            component_r = sums[(size_t)index * (size_t)depth] / weight;
        }
        if (depth > 1U) {
            component_g = sums[(size_t)index * (size_t)depth + 1U] / weight;
        } else {
            component_g = component_r;
        }
        if (depth > 2U) {
            component_b = sums[(size_t)index * (size_t)depth + 2U] / weight;
        } else if (depth > 1U) {
            component_b = component_g;
        } else {
            component_b = component_r;
        }
        clusters[index].r = component_r;
        clusters[index].g = component_g;
        clusters[index].b = component_b;
    }
    resolved_mode = sixel_resolve_final_merge_mode(final_merge_mode);
    dispatch_mode = SIXEL_FINAL_MERGE_DISPATCH_NONE;
    if (resolved_mode == SIXEL_FINAL_MERGE_WARD) {
        dispatch_mode = SIXEL_FINAL_MERGE_DISPATCH_WARD;
    }
    sixel_final_merge_clusters(clusters,
                               cluster_count,
                               target,
                               dispatch_mode,
                               use_reversible,
                               pixelformat);
    result = 0;
    limit = target;
    if (limit > cluster_count) {
        limit = cluster_count;
    }
    for (index = 0; index < cluster_count && result < limit; ++index) {
        if (clusters[index].count <= 0.0) {
            continue;
        }
        weight = clusters[index].count;
        if (weight < 1.0) {
            weight = 1.0;
        }
        weights[result] = (unsigned long)(weight + 0.5);
        if (weights[result] == 0UL) {
            weights[result] = 1UL;
        }
        used_weight = (double)weights[result];
        component_r = clusters[index].r;
        component_g = clusters[index].g;
        component_b = clusters[index].b;
        for (plane = 0; plane < (int)depth; ++plane) {
            component = component_b;
            if (plane == 0) {
                component = component_r;
            } else if (plane == 1) {
                component = component_g;
            }
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            scaled = component * used_weight;
            if (scaled < 0.0) {
                scaled = 0.0;
            }
            sums[(size_t)result * (size_t)depth + (size_t)plane] = scaled;
        }
        ++result;
    }
    if (result == 0 && cluster_count > 0) {
        weight = clusters[0].count;
        if (weight < 1.0) {
            weight = 1.0;
        }
        weights[0] = (unsigned long)(weight + 0.5);
        if (weights[0] == 0UL) {
            weights[0] = 1UL;
        }
        used_weight = (double)weights[0];
        component_r = clusters[0].r;
        component_g = clusters[0].g;
        component_b = clusters[0].b;
        for (plane = 0; plane < (int)depth; ++plane) {
            component = component_b;
            if (plane == 0) {
                component = component_r;
            } else if (plane == 1) {
                component = component_g;
            }
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            scaled = component * used_weight;
            if (scaled < 0.0) {
                scaled = 0.0;
            }
            sums[(size_t)0 * (size_t)depth + (size_t)plane] = scaled;
        }
        result = 1;
    }
    for (index = result; index < cluster_count; ++index) {
        weights[index] = 0UL;
        for (plane = 0; plane < (int)depth; ++plane) {
            sums[(size_t)index * (size_t)depth + (size_t)plane] = 0.0;
        }
    }
    sixel_allocator_free(allocator, clusters);

    return result;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
