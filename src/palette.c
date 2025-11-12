/*
 * SPDX-License-Identifier: MIT
 *
 * mediancut algorithm implementation is imported from pnmcolormap.c
 * in netpbm library.
 * http://netpbm.sourceforge.net/
 *
 * *******************************************************************************
 *                  original license block of pnmcolormap.c
 * *******************************************************************************
 *
 *   Derived from ppmquant, originally by Jef Poskanzer.
 *
 *   Copyright (C) 1989, 1991 by Jef Poskanzer.
 *   Copyright (C) 2001 by Bryan Henderson.
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted, provided
 *   that the above copyright notice appear in all copies and that both that
 *   copyright notice and this permission notice appear in supporting
 *   documentation.  This software is provided "as is" without express or
 *   implied warranty.
 *
 * ******************************************************************************
 *
 * Copyright (c) 2014-2018 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include <errno.h>

#include "lut.h"
#include "palette.h"
#include "allocator.h"
#include "status.h"
#include "compat_stub.h"

#include <stdarg.h>

#if HAVE_STRING_H
# include <string.h>
#endif

static int palette_default_lut_policy = SIXEL_LUT_POLICY_AUTO;
static int palette_method_for_largest = SIXEL_LARGE_NORM;

static float env_final_merge_target_factor = 1.81f;
static unsigned int env_final_merge_additional_lloyd = 3U;
static unsigned int env_final_merge_hkmeans_iter_max = 20U;
static double env_final_merge_hkmeans_threshold = 0.125;
static unsigned int env_kmeans_iter_max = 20U;
static double env_kmeans_threshold = 0.125;
static double env_lumin_factor_r = 0.2989;
static double env_lumin_factor_g = 0.5866;
static double env_lumin_factor_b = 0.1145;
static int env_final_merge_additional_lloyd_overridden = 0;
static int env_final_merge_env_loaded = 0;

/*
 * Resolve auto merge requests. The ladder clarifies the mapping:
 *
 *   AUTO    -> NONE
 *   NONE    -> NONE
 *   WARD    -> WARD
 *   HKMEANS -> HKMEANS
 *
 * The default behaviour therefore leaves post-merge reduction disabled
 * unless the caller explicitly opts in.
 */
static int
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
 * Median-cut and k-means helpers now live within palette.c so palette
 * generation remains self-contained.  The routines are still based on
 * the original Netpbm sources, and additional commentary clarifies the
 * extended merge behaviour.
 */

#ifndef LOG_DEBUG
#define LOG_DEBUG 7
#endif

#ifndef sixel_log_printf
#define sixel_log_printf(level, ...) \
    do { \
        (void)(level); \
        sixel_debugf(__VA_ARGS__); \
    } while (0)
#endif

static double sixel_final_merge_distance_sq(
    sixel_final_merge_cluster_t const *lhs,
    sixel_final_merge_cluster_t const *rhs);
static double sixel_final_merge_snap(double value, int use_reversible);
static void sixel_final_merge_clusters(sixel_final_merge_cluster_t *clusters,
                                       int nclusters,
                                       int target_k,
                                       int merge_mode,
                                       int use_reversible);
static void sixel_final_merge_ward(sixel_final_merge_cluster_t *clusters,
                                   int nclusters,
                                   int target_k,
                                   int use_reversible);
static void sixel_final_merge_hkmeans(
    sixel_final_merge_cluster_t *clusters,
    int nclusters,
    int target_k,
    int use_reversible);
static void sixel_final_merge_load_env(void);

static double
sixel_final_merge_distance_sq(sixel_final_merge_cluster_t const *lhs,
                              sixel_final_merge_cluster_t const *rhs)
{
    double dr;
    double dg;
    double db;
    double distance;

    dr = 0.0;
    dg = 0.0;
    db = 0.0;
    distance = 0.0;
    if (lhs == NULL || rhs == NULL) {
        return 0.0;
    }
    dr = lhs->r - rhs->r;
    dg = lhs->g - rhs->g;
    db = lhs->b - rhs->b;
    distance = dr * dr + dg * dg + db * db;

    return distance;
}

/*
 * Clamp a channel to the 0-255 range and optionally snap to the reversible
 * SIXEL tone grid when the caller enabled -6/--6reversible.
 */
static double
sixel_final_merge_snap(double value, int use_reversible)
{
    double clamped;
    double snapped;
    int sample;

    clamped = value;
    snapped = value;
    sample = 0;
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 255.0) {
        clamped = 255.0;
    }
    if (!use_reversible) {
        return clamped;
    }
    sample = (int)(clamped + 0.5);
    if (sample < 0) {
        sample = 0;
    }
    if (sample > 255) {
        sample = 255;
    }
    snapped
        = (double)sixel_palette_reversible_value((unsigned int)sample);

    return snapped;
}

/*
 * Inspect palette environment overrides exactly once so both the median-cut
 * and k-means pipelines see the same runtime configuration.
 */
static void
sixel_final_merge_load_env(void)
{
    char const *env_value;
    char *endptr;
    double parsed_factor;
    double parsed_threshold;
    double parsed_component;
    double candidate_r;
    double candidate_g;
    double candidate_b;
    long parsed_iters;
    long parsed_limit;
    int r_overridden;
    int g_overridden;

    env_value = NULL;
    endptr = NULL;
    parsed_factor = 0.0;
    parsed_threshold = 0.0;
    parsed_component = 0.0;
    candidate_r = env_lumin_factor_r;
    candidate_g = env_lumin_factor_g;
    candidate_b = env_lumin_factor_b;
    parsed_iters = 0L;
    parsed_limit = 0L;
    r_overridden = 0;
    g_overridden = 0;
    if (env_final_merge_env_loaded) {
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
        "SIXEL_PALETTE_FINAL_MERGE_HKMEANS_ITER_COUNT_MAX");
    if (env_value != NULL && env_value[0] != '\0') {
        errno = 0;
        parsed_limit = strtol(env_value, &endptr, 10);
        if (endptr != env_value && errno == 0) {
            if (parsed_limit < 1L) {
                parsed_limit = 1L;
            }
            if (parsed_limit > 30L) {
                parsed_limit = 30L;
            }
            env_final_merge_hkmeans_iter_max
                = (unsigned int)parsed_limit;
        }
    }

    env_value = sixel_compat_getenv(
        "SIXEL_PALETTE_FINAL_MERGE_HKMEANS_THRESHOLD");
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
            env_final_merge_hkmeans_threshold = parsed_threshold;
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
            if (parsed_limit > 30L) {
                parsed_limit = 30L;
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
}

static unsigned int
sixel_final_merge_lloyd_iterations(int merge_mode)
{
    unsigned int resolved;

    sixel_final_merge_load_env();
    if (env_final_merge_additional_lloyd_overridden) {
        return env_final_merge_additional_lloyd;
    }
    resolved = (unsigned int)merge_mode;
    if (resolved == (unsigned int)SIXEL_FINAL_MERGE_HKMEANS) {
        return 0U;
    }

    return 3U;
}

static void
sixel_final_merge_clusters(sixel_final_merge_cluster_t *clusters,
                           int nclusters,
                           int target_k,
                           int merge_mode,
                           int use_reversible)
{
    /* Dispatch final merge processing according to the requested mode. */
    if (clusters == NULL || nclusters <= 0) {
        return;
    }
    switch (merge_mode) {
    case SIXEL_FINAL_MERGE_DISPATCH_WARD:
        sixel_final_merge_ward(clusters, nclusters, target_k, use_reversible);
        break;
    case SIXEL_FINAL_MERGE_DISPATCH_HKMEANS:
        sixel_final_merge_hkmeans(
            clusters, nclusters, target_k, use_reversible);
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
                       int use_reversible)
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
                    &clusters[i], &clusters[j]);
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
        for (k = best_j; k < nclusters - 1; ++k) {
            clusters[k] = clusters[k + 1];
        }
        clusters[nclusters - 1].r = 0.0;
        clusters[nclusters - 1].g = 0.0;
        clusters[nclusters - 1].b = 0.0;
        clusters[nclusters - 1].count = 0.0;
    }
    for (i = 0; i < nclusters; ++i) {
        if (i >= desired && clusters[i].count > 0.0) {
            clusters[i].r = 0.0;
            clusters[i].g = 0.0;
            clusters[i].b = 0.0;
            clusters[i].count = 0.0;
        }
    }
}

static void
sixel_final_merge_hkmeans(sixel_final_merge_cluster_t *clusters,
                          int nclusters,
                          int target_k,
                          int use_reversible)
{
    /*
     * Perform weighted k-means refinement for final palette merging.
     * Steps:
     *   1. Identify active clusters and seed k centers by even sampling.
     *   2. Assign each cluster to the nearest center using RGB distance.
     *   3. Recompute centers as weighted means of their assignments.
     *   4. Optionally snap updated centers to the reversible SIXEL tone set.
     *   5. Repeat until convergence or the iteration limit is reached.
     */
    int active_count;
    int i;
    int j;
    int k;
    int max_iter;
    int iter;
    int best_index;
    int reseed_index;
    int *active_indices;
    int *assignment;
    double *weights;
    double *sums;
    double diff_r;
    double diff_g;
    double diff_b;
    double shift;
    double delta;
    double threshold;
    double weight;
    double total;
    double fraction;
    sixel_final_merge_cluster_t *centers;

    active_count = 0;
    i = 0;
    j = 0;
    k = 0;
    max_iter = 0;
    iter = 0;
    best_index = 0;
    reseed_index = 0;
    active_indices = NULL;
    assignment = NULL;
    weights = NULL;
    sums = NULL;
    diff_r = 0.0;
    diff_g = 0.0;
    diff_b = 0.0;
    shift = 0.0;
    delta = 0.0;
    threshold = 0.0;
    weight = 0.0;
    total = 0.0;
    fraction = 0.0;
    centers = NULL;
    if (clusters == NULL || nclusters <= 0) {
        return;
    }
    for (i = 0; i < nclusters; ++i) {
        if (clusters[i].count > 0.0) {
            ++active_count;
        }
    }
    if (active_count == 0) {
        clusters[0].r = 0.0;
        clusters[0].g = 0.0;
        clusters[0].b = 0.0;
        clusters[0].count = 1.0;
        for (i = 1; i < nclusters; ++i) {
            clusters[i].r = 0.0;
            clusters[i].g = 0.0;
            clusters[i].b = 0.0;
            clusters[i].count = 0.0;
        }
        return;
    }
    k = target_k;
    if (k > active_count) {
        k = active_count;
    }
    if (k < 1) {
        k = 1;
    }
    active_indices
        = (int *)malloc((size_t)active_count * sizeof(int));
    assignment = (int *)malloc((size_t)nclusters * sizeof(int));
    centers = (sixel_final_merge_cluster_t *)malloc(
        (size_t)k * sizeof(sixel_final_merge_cluster_t));
    weights = (double *)malloc((size_t)k * sizeof(double));
    sums = (double *)malloc((size_t)k * 3U * sizeof(double));
    if (active_indices == NULL || assignment == NULL || centers == NULL
        || weights == NULL || sums == NULL) {
        free(active_indices);
        free(assignment);
        free(centers);
        free(weights);
        free(sums);
        sixel_final_merge_ward(clusters, nclusters, target_k, use_reversible);
        return;
    }
    j = 0;
    for (i = 0; i < nclusters; ++i) {
        if (clusters[i].count > 0.0) {
            active_indices[j] = i;
            ++j;
        }
    }
    for (i = 0; i < k; ++i) {
        fraction = ((double)i + 0.5) / (double)k;
        if (fraction > 1.0) {
            fraction = 1.0;
        }
        j = (int)(fraction * (double)active_count);
        if (j >= active_count) {
            j = active_count - 1;
        }
        centers[i] = clusters[active_indices[j]];
        centers[i].count = clusters[active_indices[j]].count;
    }
    for (i = 0; i < nclusters; ++i) {
        assignment[i] = 0;
    }
    sixel_final_merge_load_env();
    max_iter = (int)env_final_merge_hkmeans_iter_max;
    if (max_iter < 1) {
        max_iter = 1;
    }
    threshold = env_final_merge_hkmeans_threshold;
    for (iter = 0; iter < max_iter; ++iter) {
        memset(weights, 0, (size_t)k * sizeof(double));
        memset(sums, 0, (size_t)k * 3U * sizeof(double));
        for (i = 0; i < nclusters; ++i) {
            if (clusters[i].count <= 0.0) {
                continue;
            }
            best_index = 0;
            shift = sixel_final_merge_distance_sq(
                &clusters[i], &centers[0]);
            for (j = 1; j < k; ++j) {
                delta = sixel_final_merge_distance_sq(
                    &clusters[i], &centers[j]);
                if (delta < shift) {
                    shift = delta;
                    best_index = j;
                }
            }
            assignment[i] = best_index;
            weight = clusters[i].count;
            weights[best_index] += weight;
            sums[(size_t)best_index * 3U + 0U]
                += clusters[i].r * weight;
            sums[(size_t)best_index * 3U + 1U]
                += clusters[i].g * weight;
            sums[(size_t)best_index * 3U + 2U]
                += clusters[i].b * weight;
        }
        delta = 0.0;
        for (i = 0; i < k; ++i) {
            if (weights[i] <= 0.0) {
                reseed_index = active_indices[i % active_count];
                centers[i] = clusters[reseed_index];
                weights[i] = clusters[reseed_index].count;
                delta += 1.0;
                continue;
            }
            total = weights[i];
            diff_r = sums[(size_t)i * 3U + 0U] / total;
            diff_g = sums[(size_t)i * 3U + 1U] / total;
            diff_b = sums[(size_t)i * 3U + 2U] / total;
            shift = sixel_final_merge_distance_sq(
                &centers[i],
                &(sixel_final_merge_cluster_t){
                    diff_r,
                    diff_g,
                    diff_b,
                    total});
            delta += shift;
            centers[i].r = sixel_final_merge_snap(diff_r, use_reversible);
            centers[i].g = sixel_final_merge_snap(diff_g, use_reversible);
            centers[i].b = sixel_final_merge_snap(diff_b, use_reversible);
        }
        sixel_log_printf(LOG_DEBUG,
                          "hkmeans iter=%d delta=%f\n",
                          iter,
                          delta);
        if (delta <= threshold) {
            break;
        }
    }
    for (i = 0; i < k; ++i) {
        clusters[i].r = centers[i].r;
        clusters[i].g = centers[i].g;
        clusters[i].b = centers[i].b;
        clusters[i].count = weights[i];
        if (clusters[i].count <= 0.0) {
            clusters[i].count = 0.0;
        }
    }
    for (i = k; i < nclusters; ++i) {
        clusters[i].r = 0.0;
        clusters[i].g = 0.0;
        clusters[i].b = 0.0;
        clusters[i].count = 0.0;
    }
    free(active_indices);
    free(assignment);
    free(centers);
    free(weights);
    free(sums);
}

void
sixel_palette_reversible_tuple(sample *tuple,
                               unsigned int depth)
{
    unsigned int plane;
    unsigned int sample_value;

    for (plane = 0U; plane < depth; ++plane) {
        sample_value = (unsigned int)tuple[plane];
        tuple[plane] = (sample)sixel_palette_reversible_value(sample_value);
    }
}

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 unsigned int depth)
{
    unsigned int index;
    unsigned int plane;
    unsigned int sample_value;
    size_t offset;

    for (index = 0U; index < colors; ++index) {
        for (plane = 0U; plane < depth; ++plane) {
            offset = (size_t)index * (size_t)depth + (size_t)plane;
            sample_value = (unsigned int)palette[offset];
            palette[offset] = sixel_palette_reversible_value(sample_value);
        }
    }
}

void
sixel_palette_set_lut_policy(int lut_policy)
{
    int normalized;

    normalized = SIXEL_LUT_POLICY_AUTO;
    if (lut_policy == SIXEL_LUT_POLICY_5BIT
        || lut_policy == SIXEL_LUT_POLICY_6BIT
        || lut_policy == SIXEL_LUT_POLICY_CERTLUT
        || lut_policy == SIXEL_LUT_POLICY_NONE) {
        normalized = lut_policy;
    }

    palette_default_lut_policy = normalized;
}

void
sixel_palette_set_method_for_largest(int method)
{
    int normalized;

    normalized = SIXEL_LARGE_NORM;
    if (method == SIXEL_LARGE_NORM || method == SIXEL_LARGE_LUM) {
        normalized = method;
    } else if (method == SIXEL_LARGE_AUTO) {
        normalized = SIXEL_LARGE_NORM;
    }

    palette_method_for_largest = normalized;
}

typedef struct box* boxVector;
struct box {
    unsigned int ind;
    unsigned int colors;
    unsigned int sum;
};

static unsigned int compareplanePlane;
static tupletable2 const *force_palette_source;
    /* This is a parameter to compareplane().  We use this global variable
       so that compareplane() can be called by qsort(), to compare two
       tuples.  qsort() doesn't pass any arguments except the two tuples.
    */
static int
compareplane(const void * const arg1,
             const void * const arg2)
{
    int lhs, rhs;

    typedef const struct tupleint * const * const sortarg;
    sortarg comparandPP  = (sortarg) arg1;
    sortarg comparatorPP = (sortarg) arg2;
    lhs = (int)(*comparandPP)->tuple[compareplanePlane];
    rhs = (int)(*comparatorPP)->tuple[compareplanePlane];

    return lhs - rhs;
}


static int
sumcompare(const void * const b1, const void * const b2)
{
    return (int)((boxVector)b2)->sum - (int)((boxVector)b1)->sum;
}



static tupletable2
newColorMap(unsigned int const newcolors, unsigned int const depth, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colormap;
    unsigned int i;

    colormap.size = 0;
    status = alloctupletable(&colormap.table, depth, newcolors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (colormap.table) {
        for (i = 0; i < newcolors; ++i) {
            unsigned int plane;
            for (plane = 0; plane < depth; ++plane)
                colormap.table[i]->tuple[plane] = 0;
        }
        colormap.size = newcolors;
    }

end:
    return colormap;
}


static boxVector
newBoxVector(
    unsigned int const  /* in */ colors,
    unsigned int const  /* in */ sum,
    unsigned int const  /* in */ newcolors,
    sixel_allocator_t   /* in */ *allocator)
{
    boxVector bv;

    bv = (boxVector)sixel_allocator_malloc(allocator,
                                           sizeof(struct box) * (size_t)newcolors);
    if (bv == NULL) {
        sixel_helper_set_additional_message("out of memory allocating box vector table");
        return NULL;
    }

    /* Set up the initial box. */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;

    return bv;
}


static void
findBoxBoundaries(tupletable2  const colorfreqtable,
                  unsigned int const depth,
                  unsigned int const boxStart,
                  unsigned int const boxSize,
                  sample             minval[],
                  sample             maxval[])
{
/*----------------------------------------------------------------------------
  Go through the box finding the minimum and maximum of each
  component - the boundaries of the box.
-----------------------------------------------------------------------------*/
    unsigned int plane;
    unsigned int i;

    for (plane = 0; plane < depth; ++plane) {
        minval[plane] = colorfreqtable.table[boxStart]->tuple[plane];
        maxval[plane] = minval[plane];
    }

    for (i = 1; i < boxSize; ++i) {
        for (plane = 0; plane < depth; ++plane) {
            sample const v = colorfreqtable.table[boxStart + i]->tuple[plane];
            if (v < minval[plane]) minval[plane] = v;
            if (v > maxval[plane]) maxval[plane] = v;
        }
    }
}



static unsigned int
largestByNorm(sample minval[], sample maxval[], unsigned int const depth)
{

    unsigned int largestDimension;
    unsigned int plane;
    sample largestSpreadSoFar;

    largestSpreadSoFar = 0;
    largestDimension = 0;
    for (plane = 0; plane < depth; ++plane) {
        sample const spread = maxval[plane]-minval[plane];
        if (spread > largestSpreadSoFar) {
            largestDimension = plane;
            largestSpreadSoFar = spread;
        }
    }
    return largestDimension;
}



static unsigned int
largestByLuminosity(sample minval[], sample maxval[], unsigned int const depth)
{
/*----------------------------------------------------------------------------
   This subroutine presumes that the tuple type is either
   BLACKANDWHITE, GRAYSCALE, or RGB (which implies pamP->depth is 1 or 3).
   To save time, we don't actually check it.
-----------------------------------------------------------------------------*/
    unsigned int retval;

    double lumin_factor[3];

    sixel_final_merge_load_env();
    lumin_factor[0] = env_lumin_factor_r;
    lumin_factor[1] = env_lumin_factor_g;
    lumin_factor[2] = env_lumin_factor_b;

    if (depth == 1) {
        retval = 0;
    } else {
        /* An RGB tuple */
        unsigned int largestDimension;
        unsigned int plane;
        double largestSpreadSoFar;

        largestSpreadSoFar = 0.0;
        largestDimension = 0;

        for (plane = 0; plane < 3; ++plane) {
            double const spread =
                lumin_factor[plane] * (maxval[plane]-minval[plane]);
            if (spread > largestSpreadSoFar) {
                largestDimension = plane;
                largestSpreadSoFar = spread;
            }
        }
        retval = largestDimension;
    }
    return retval;
}



static void
centerBox(unsigned int const boxStart,
          unsigned int const boxSize,
          tupletable2  const colorfreqtable,
          unsigned int const depth,
          tuple        const newTuple)
{

    unsigned int plane;
    sample minval, maxval;
    unsigned int i;

    for (plane = 0; plane < depth; ++plane) {
        minval = maxval = colorfreqtable.table[boxStart]->tuple[plane];

        for (i = 1; i < boxSize; ++i) {
            sample v = colorfreqtable.table[boxStart + i]->tuple[plane];
            minval = minval < v ? minval: v;
            maxval = maxval > v ? maxval: v;
        }
        newTuple[plane] = (minval + maxval) / 2;
    }
}



static void
averageColors(unsigned int const boxStart,
              unsigned int const boxSize,
              tupletable2  const colorfreqtable,
              unsigned int const depth,
              tuple        const newTuple)
{
    unsigned int plane;
    sample sum;
    unsigned int i;

    for (plane = 0; plane < depth; ++plane) {
        sum = 0;

        for (i = 0; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->tuple[plane];
        }

        newTuple[plane] = sum / boxSize;
    }
}



static void
averagePixels(unsigned int const boxStart,
              unsigned int const boxSize,
              tupletable2 const colorfreqtable,
              unsigned int const depth,
              tuple const newTuple)
{

    unsigned int n;
        /* Number of tuples represented by the box */
    unsigned int plane;
    unsigned int i;

    /* Count the tuples in question */
    n = 0;  /* initial value */
    for (i = 0; i < boxSize; ++i) {
        n += (unsigned int)colorfreqtable.table[boxStart + i]->value;
    }

    for (plane = 0; plane < depth; ++plane) {
        sample sum;

        sum = 0;

        for (i = 0; i < boxSize; ++i) {
            sum += colorfreqtable.table[boxStart + i]->tuple[plane]
                * (unsigned int)colorfreqtable.table[boxStart + i]->value;
        }

        newTuple[plane] = sum / n;
    }
}



static tupletable2
colormapFromBv(unsigned int const newcolors,
               boxVector const bv,
               unsigned int const boxes,
               tupletable2 const colorfreqtable,
               unsigned int const depth,
               int const methodForRep,
               int const use_reversible,
               sixel_allocator_t *allocator)
{
    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box.
    */
    tupletable2 colormap;
    unsigned int bi;

    colormap = newColorMap(newcolors, depth, allocator);
    if (!colormap.size) {
        return colormap;
    }

    for (bi = 0; bi < boxes; ++bi) {
        switch (methodForRep) {
        case SIXEL_REP_CENTER_BOX:
            centerBox(bv[bi].ind, bv[bi].colors,
                      colorfreqtable, depth,
                      colormap.table[bi]->tuple);
            break;
        case SIXEL_REP_AVERAGE_COLORS:
            averageColors(bv[bi].ind, bv[bi].colors,
                          colorfreqtable, depth,
                          colormap.table[bi]->tuple);
            break;
        case SIXEL_REP_AVERAGE_PIXELS:
            averagePixels(bv[bi].ind, bv[bi].colors,
                          colorfreqtable, depth,
                          colormap.table[bi]->tuple);
            break;
        default:
#if HAVE_ASSERT
            assert("Internal error: invalid value of methodForRep");
#endif  /* HAVE_ASSERT */
            break;
        }
        if (use_reversible) {
            sixel_palette_reversible_tuple(colormap.table[bi]->tuple,
                                          depth);
        }
    }
    return colormap;
}


static int
force_palette_compare(const void *lhs, const void *rhs)
{
    unsigned int left;
    unsigned int right;
    unsigned int left_value;
    unsigned int right_value;

    left = *(const unsigned int *)lhs;
    right = *(const unsigned int *)rhs;
    left_value = force_palette_source->table[left]->value;
    right_value = force_palette_source->table[right]->value;
    if (left_value > right_value) {
        return -1;
    }
    if (left_value < right_value) {
        return 1;
    }
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}


static SIXELSTATUS
force_palette_completion(tupletable2 *colormapP,
                         unsigned int depth,
                         unsigned int reqColors,
                         tupletable2 const colorfreqtable,
                         sixel_allocator_t *allocator)
{
    /*
     * We enqueue "losers" from the histogram so that we can revive them:
     *
     *   histogram --> sort by hit count --> append to palette tail
     *        ^                             |
     *        +-----------------------------+
     *
     * The ASCII loop shows how discarded colors walk back into the
     * palette when the user demands an exact size.
     */
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable new_table = NULL;
    unsigned int *order = NULL;
    unsigned int current;
    unsigned int fill;
    unsigned int candidate;
    unsigned int plane;
    unsigned int source;

    current = colormapP->size;
    if (current >= reqColors) {
        return SIXEL_OK;
    }

    status = alloctupletable(&new_table, depth, reqColors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (colorfreqtable.size > 0U) {
        order = (unsigned int *)sixel_allocator_malloc(
            allocator, colorfreqtable.size * sizeof(unsigned int));
        if (order == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (candidate = 0; candidate < colorfreqtable.size; ++candidate) {
            order[candidate] = candidate;
        }
        force_palette_source = &colorfreqtable;
        qsort(order, colorfreqtable.size, sizeof(unsigned int),
              force_palette_compare);
        force_palette_source = NULL;
    }

    for (fill = 0; fill < current; ++fill) {
        new_table[fill]->value = colormapP->table[fill]->value;
        for (plane = 0; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] =
                colormapP->table[fill]->tuple[plane];
        }
    }

    candidate = 0U;
    fill = current;
    if (order != NULL) {
        while (fill < reqColors && candidate < colorfreqtable.size) {
            unsigned int index;

            index = order[candidate];
            new_table[fill]->value = colorfreqtable.table[index]->value;
            for (plane = 0; plane < depth; ++plane) {
                new_table[fill]->tuple[plane] =
                    colorfreqtable.table[index]->tuple[plane];
            }
            ++fill;
            ++candidate;
        }
    }

    if (fill < reqColors && fill == 0U) {
        new_table[fill]->value = 0U;
        for (plane = 0; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] = 0U;
        }
        ++fill;
    }

    source = 0U;
    while (fill < reqColors) {
        new_table[fill]->value = new_table[source]->value;
        for (plane = 0; plane < depth; ++plane) {
            new_table[fill]->tuple[plane] = new_table[source]->tuple[plane];
        }
        ++fill;
        ++source;
        if (source >= fill) {
            source = 0U;
        }
    }

    sixel_allocator_free(allocator, colormapP->table);
    colormapP->table = new_table;
    colormapP->size = reqColors;
    status = SIXEL_OK;

end:
    if (status != SIXEL_OK && new_table != NULL) {
        sixel_allocator_free(allocator, new_table);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    return status;
}


static SIXELSTATUS
splitBox(boxVector const bv,
         unsigned int *const boxesP,
         unsigned int const bi,
         tupletable2 const colorfreqtable,
         unsigned int const depth,
         int const methodForLargest)
{
/*----------------------------------------------------------------------------
   Split Box 'bi' in the box vector bv (so that bv contains one more box
   than it did as input).  Split it so that each new box represents about
   half of the pixels in the distribution given by 'colorfreqtable' for
   the colors in the original box, but with distinct colors in each of the
   two new boxes.

   Assume the box contains at least two colors.
-----------------------------------------------------------------------------*/
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned int const boxStart = bv[bi].ind;
    unsigned int const boxSize  = bv[bi].colors;
    unsigned int const sm       = bv[bi].sum;

    enum { max_depth= 16 };
    sample minval[max_depth];
    sample maxval[max_depth];

    /* assert(max_depth >= depth); */

    unsigned int largestDimension;
    /* number of the plane with the largest spread */
    unsigned int medianIndex;
    unsigned int lowersum;

    /* Number of pixels whose value is "less than" the median */
    findBoxBoundaries(colorfreqtable, depth, boxStart, boxSize,
                      minval, maxval);

    /* Find the largest dimension, and sort by that component.  I have
       included two methods for determining the "largest" dimension;
       first by simply comparing the range in RGB space, and second by
       transforming into luminosities before the comparison.
    */
    switch (methodForLargest) {
    case SIXEL_LARGE_NORM:
        largestDimension = largestByNorm(minval, maxval, depth);
        break;
    case SIXEL_LARGE_LUM:
        largestDimension = largestByLuminosity(minval, maxval, depth);
        break;
    default:
        sixel_helper_set_additional_message(
            "Internal error: invalid value of methodForLargest.");
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    /* TODO: I think this sort should go after creating a box,
       not before splitting.  Because you need the sort to use
       the SIXEL_REP_CENTER_BOX method of choosing a color to
       represent the final boxes
    */

    /* Set the gross global variable 'compareplanePlane' as a
       parameter to compareplane(), which is called by qsort().
    */
    compareplanePlane = largestDimension;
    qsort((char*) &colorfreqtable.table[boxStart], boxSize,
          sizeof(colorfreqtable.table[boxStart]),
          compareplane);

    {
        /* Now find the median based on the counts, so that about half
           the pixels (not colors, pixels) are in each subdivision.  */

        unsigned int i;

        lowersum = colorfreqtable.table[boxStart]->value; /* initial value */
        for (i = 1; i < boxSize - 1 && lowersum < sm / 2; ++i) {
            lowersum += colorfreqtable.table[boxStart + i]->value;
        }
        medianIndex = i;
    }
    /* Split the box, and sort to bring the biggest boxes to the top.  */

    bv[bi].colors = medianIndex;
    bv[bi].sum = lowersum;
    bv[*boxesP].ind = boxStart + medianIndex;
    bv[*boxesP].colors = boxSize - medianIndex;
    bv[*boxesP].sum = sm - lowersum;
    ++(*boxesP);
    qsort((char*) bv, *boxesP, sizeof(struct box), sumcompare);

    status = SIXEL_OK;

end:
    return status;
}



static unsigned int sixel_final_merge_target(unsigned int reqcolors,
                                             int final_merge_mode);
static void sixel_final_merge_lloyd_histogram(
    tupletable2 const colorfreqtable,
    unsigned int depth,
    unsigned int cluster_count,
    unsigned long *cluster_weight,
    unsigned long *cluster_sums,
    unsigned int iterations);
static int
sixel_palette_apply_merge(unsigned long *weights,
                          unsigned long *sums,
                          unsigned int depth,
                          int cluster_count,
                          int target,
                          int final_merge_mode,
                          int use_reversible,
                          sixel_allocator_t *allocator);
static SIXELSTATUS
sixel_palette_clusters_to_colormap(unsigned long *weights,
                                   unsigned long *sums,
                                   unsigned int depth,
                                   unsigned int cluster_count,
                                   int use_reversible,
                                   tupletable2 *colormapP,
                                   sixel_allocator_t *allocator);

/*
 * Count unique RGB triples until the limit is exceeded.  This allows the
 * caller to detect low-color images without building a full histogram.
 */
static SIXELSTATUS
sixel_palette_count_unique_within_limit(unsigned char const *data,
                                        unsigned int length,
                                        unsigned int channels,
                                        unsigned int limit,
                                        unsigned int *unique_count,
                                        int *within_limit,
                                        sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    uint32_t *table;
    unsigned int table_size;
    unsigned int mask;
    unsigned int pixel_count;
    unsigned int index;
    unsigned int base;
    unsigned int slot;
    unsigned int unique;
    uint32_t color;
    int limited;

    status = SIXEL_BAD_ARGUMENT;
    table = NULL;
    table_size = 0U;
    mask = 0U;
    pixel_count = 0U;
    index = 0U;
    base = 0U;
    slot = 0U;
    unique = 0U;
    color = 0U;
    limited = 0;

    if (unique_count != NULL) {
        *unique_count = 0U;
    }
    if (within_limit != NULL) {
        *within_limit = 0;
    }
    if (data == NULL || allocator == NULL) {
        return status;
    }
    if (channels != 3U && channels != 4U) {
        return status;
    }
    if (limit == 0U) {
        return status;
    }

    pixel_count = length / channels;
    if (pixel_count == 0U) {
        status = SIXEL_OK;
        if (within_limit != NULL) {
            *within_limit = 1;
        }
        return status;
    }

    table_size = 1U;
    while (table_size < limit * 2U) {
        table_size <<= 1U;
    }
    if (table_size < 8U) {
        table_size = 8U;
    }
    mask = table_size - 1U;

    table = (uint32_t *)sixel_allocator_malloc(
        allocator, (size_t)table_size * sizeof(uint32_t));
    if (table == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0U; index < table_size; ++index) {
        table[index] = 0xffffffffU;
    }

    limited = 1;
    for (index = 0U; index < pixel_count; ++index) {
        base = index * channels;
        if (channels == 4U && data[base + 3U] == 0U) {
            continue;
        }
        color = ((uint32_t)data[base] << 16)
              | ((uint32_t)data[base + 1U] << 8)
              | (uint32_t)data[base + 2U];
        slot = (unsigned int)(((uint32_t)0x9e3779b9U * color) & mask);
        while (table[slot] != 0xffffffffU && table[slot] != color) {
            slot = (slot + 1U) & mask;
        }
        if (table[slot] == color) {
            continue;
        }
        table[slot] = color;
        ++unique;
        if (unique > limit) {
            limited = 0;
            unique = limit + 1U;
            break;
        }
    }

    status = SIXEL_OK;
    if (unique_count != NULL) {
        *unique_count = unique;
    }
    if (within_limit != NULL) {
        *within_limit = limited;
    }

    sixel_allocator_free(allocator, table);
    return status;
}

static SIXELSTATUS
mediancut(tupletable2 const colorfreqtable,
          unsigned int const depth,
          unsigned int const newcolors,
          int const methodForLargest,
          int const methodForRep,
          int const use_reversible,
          int const final_merge_mode,
          tupletable2 *const colormapP,
          sixel_allocator_t *allocator)
{
/*----------------------------------------------------------------------------
   Compute a set of only 'newcolors' colors that best represent an
   image whose pixels are summarized by the histogram
   'colorfreqtable'.  Each tuple in that table has depth 'depth'.
   colorfreqtable.table[i] tells the number of pixels in the subject image
   have a particular color.

   As a side effect, sort 'colorfreqtable'.
-----------------------------------------------------------------------------*/
    boxVector bv;
    unsigned int bi;
    unsigned int boxes;
    int multicolorBoxesExist;
    unsigned int i;
    unsigned int sum;
    unsigned int working_colors;
    int apply_merge;
    int resolved_merge;
    unsigned long *cluster_weight;
    unsigned long *cluster_sums;
    int cluster_total;
    unsigned int plane;
    unsigned int offset;
    unsigned int size;
    unsigned long value;
    struct tupleint *entry;
    SIXELSTATUS merge_status;
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned int iteration_limit;

    sum = 0;
    working_colors = newcolors;
    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD
                   || resolved_merge == SIXEL_FINAL_MERGE_HKMEANS);
    bv = NULL;
    cluster_weight = NULL;
    cluster_sums = NULL;
    cluster_total = 0;
    plane = 0U;
    offset = 0U;
    size = 0U;
    value = 0UL;
    entry = NULL;
    merge_status = SIXEL_OK;
    iteration_limit = 0U;

    for (i = 0; i < colorfreqtable.size; ++i) {
        sum += colorfreqtable.table[i]->value;
    }

    if (apply_merge) {
        /* Choose an oversplit target so that the merge stage has slack. */
        working_colors = sixel_final_merge_target(newcolors,
                                                  final_merge_mode);
        if (working_colors > colorfreqtable.size) {
            working_colors = colorfreqtable.size;
        }
        sixel_debugf("overshoot: %u", working_colors);
    }
    if (working_colors == 0U) {
        working_colors = 1U;
    }

    /* There is at least one box that contains at least 2 colors; ergo,
       there is more splitting we can do.  */
    bv = newBoxVector(colorfreqtable.size, sum, working_colors, allocator);
    if (bv == NULL) {
        goto end;
    }
    boxes = 1;
    multicolorBoxesExist = (colorfreqtable.size > 1);

    /* Main loop: split boxes until we have enough. */
    while (boxes < working_colors && multicolorBoxesExist) {
        /* Find the first splittable box. */
        for (bi = 0; bi < boxes && bv[bi].colors < 2; ++bi)
            ;
        if (bi >= boxes) {
            multicolorBoxesExist = 0;
        } else {
            status = splitBox(bv, &boxes, bi,
                              colorfreqtable, depth,
                              methodForLargest);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
    }
    if (apply_merge && boxes > newcolors) {
        /* Capture weight and component sums for each temporary box. */
        cluster_weight = (unsigned long *)sixel_allocator_malloc(
            allocator, (size_t)boxes * sizeof(unsigned long));
        cluster_sums = (unsigned long *)sixel_allocator_malloc(
            allocator, (size_t)boxes * (size_t)depth * sizeof(unsigned long));
        if (cluster_weight == NULL || cluster_sums == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (bi = 0U; bi < boxes; ++bi) {
            offset = bv[bi].ind;
            size = bv[bi].colors;
            cluster_weight[bi] = 0UL;
            for (plane = 0U; plane < depth; ++plane) {
                cluster_sums[(size_t)bi * (size_t)depth + plane] = 0UL;
            }
            for (i = 0U; i < size; ++i) {
                entry = colorfreqtable.table[offset + i];
                value = (unsigned long)entry->value;
                cluster_weight[bi] += value;
                for (plane = 0U; plane < depth; ++plane) {
                    cluster_sums[(size_t)bi * (size_t)depth + plane] +=
                        (unsigned long)entry->tuple[plane] * value;
                }
            }
        }
        cluster_total = sixel_palette_apply_merge(cluster_weight,
                                                  cluster_sums,
                                                  depth,
                                                  (int)boxes,
                                                  (int)newcolors,
                                                  resolved_merge,
                                                  use_reversible,
                                                  allocator);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > newcolors) {
            cluster_total = (int)newcolors;
        }
        if (cluster_total > 0) {
            sixel_final_merge_load_env();
            iteration_limit
                = sixel_final_merge_lloyd_iterations(resolved_merge);
            if (iteration_limit > 0U) {
                sixel_final_merge_lloyd_histogram(colorfreqtable,
                                                  depth,
                                                  (unsigned int)cluster_total,
                                                  cluster_weight,
                                                  cluster_sums,
                                                  iteration_limit);
            }
        }
        /* Rebuild the palette using the merged cluster statistics. */
        merge_status = sixel_palette_clusters_to_colormap(cluster_weight,
                                                         cluster_sums,
                                                         depth,
                                                         (unsigned int)cluster_total,
                                                         use_reversible,
                                                         colormapP,
                                                         allocator);
        if (SIXEL_FAILED(merge_status)) {
            status = merge_status;
            goto end;
        }
    } else {
        *colormapP = colormapFromBv(newcolors, bv, boxes,
                                    colorfreqtable, depth,
                                    methodForRep, use_reversible,
                                    allocator);
    }

    status = SIXEL_OK;

end:
    if (bv != NULL) {
        sixel_allocator_free(allocator, bv);
    }
    if (cluster_sums != NULL) {
        sixel_allocator_free(allocator, cluster_sums);
    }
    if (cluster_weight != NULL) {
        sixel_allocator_free(allocator, cluster_weight);
    }
    return status;
}


/* Determine how many clusters to create before the final merge step. */
static unsigned int
sixel_final_merge_target(unsigned int reqcolors,
                         int final_merge_mode)
{
    double factor;
    unsigned int scaled;
    int resolved;

    sixel_final_merge_load_env();
    resolved = sixel_resolve_final_merge_mode(final_merge_mode);
    if (resolved != SIXEL_FINAL_MERGE_WARD
        && resolved != SIXEL_FINAL_MERGE_HKMEANS) {
        return reqcolors;
    }
    factor = env_final_merge_target_factor;
    scaled = (unsigned int)((double)reqcolors * factor);
    if (scaled <= reqcolors) {
        scaled = reqcolors;
    }
    if (scaled < 1U) {
        scaled = 1U;
    }

    return scaled;
}


/*
 * Reassign histogram entries to the merged palette so the final colors stay
 * consistent with their nearest centroids.  The miniature ladder below
 * explains how each iteration rotates through the steps:
 *
 *   histogram --> assign --> accumulate --> update centers
 *          ^                                         |
 *          +-----------------------------------------+
 */
static void
sixel_final_merge_lloyd_histogram(tupletable2 const colorfreqtable,
                                  unsigned int depth,
                                  unsigned int cluster_count,
                                  unsigned long *cluster_weight,
                                  unsigned long *cluster_sums,
                                  unsigned int iterations)
{
    double *centers;
    double distance;
    double best_distance;
    double diff;
    double channel;
    unsigned int iteration;
    unsigned int cluster_index;
    unsigned int component;
    unsigned int entry_index;
    unsigned int best_cluster;
    unsigned long weight;
    unsigned long value;
    size_t offset;
    size_t total;
    struct tupleint *entry;

    centers = NULL;
    distance = 0.0;
    best_distance = 0.0;
    diff = 0.0;
    channel = 0.0;
    iteration = 0U;
    cluster_index = 0U;
    component = 0U;
    entry_index = 0U;
    best_cluster = 0U;
    weight = 0UL;
    value = 0UL;
    offset = 0U;
    total = 0U;
    entry = NULL;
    if (iterations == 0U || cluster_count == 0U || depth == 0U
        || cluster_weight == NULL || cluster_sums == NULL
        || colorfreqtable.table == NULL || colorfreqtable.size == 0U) {
        return;
    }
    total = (size_t)cluster_count * (size_t)depth;
    centers = (double *)malloc(total * sizeof(double));
    if (centers == NULL) {
        return;
    }
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        offset = (size_t)cluster_index * (size_t)depth;
        weight = cluster_weight[cluster_index];
        for (component = 0U; component < depth; ++component) {
            if (weight > 0UL) {
                centers[offset + (size_t)component]
                    = (double)cluster_sums[offset + (size_t)component]
                    / (double)weight;
            } else {
                centers[offset + (size_t)component] = 0.0;
            }
        }
    }
    for (iteration = 0U; iteration < iterations; ++iteration) {
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            cluster_weight[cluster_index] = 0UL;
        }
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            offset = (size_t)cluster_index * (size_t)depth;
            for (component = 0U; component < depth; ++component) {
                cluster_sums[offset + (size_t)component] = 0UL;
            }
        }
        for (entry_index = 0U; entry_index < colorfreqtable.size;
                ++entry_index) {
            entry = colorfreqtable.table[entry_index];
            if (entry == NULL) {
                continue;
            }
            value = (unsigned long)entry->value;
            if (value == 0UL) {
                continue;
            }
            best_cluster = 0U;
            best_distance = 0.0;
            offset = 0U;
            for (component = 0U; component < depth; ++component) {
                diff = (double)entry->tuple[component]
                    - centers[offset + (size_t)component];
                best_distance += diff * diff;
            }
            for (cluster_index = 1U; cluster_index < cluster_count;
                    ++cluster_index) {
                distance = 0.0;
                offset = (size_t)cluster_index * (size_t)depth;
                for (component = 0U; component < depth; ++component) {
                    diff = (double)entry->tuple[component]
                        - centers[offset + (size_t)component];
                    distance += diff * diff;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_cluster = cluster_index;
                }
            }
            offset = (size_t)best_cluster * (size_t)depth;
            cluster_weight[best_cluster] += value;
            for (component = 0U; component < depth; ++component) {
                cluster_sums[offset + (size_t)component]
                    += (unsigned long)entry->tuple[component] * value;
            }
        }
        for (cluster_index = 0U; cluster_index < cluster_count;
                ++cluster_index) {
            weight = cluster_weight[cluster_index];
            if (weight == 0UL) {
                continue;
            }
            offset = (size_t)cluster_index * (size_t)depth;
            for (component = 0U; component < depth; ++component) {
                centers[offset + (size_t)component]
                    = (double)cluster_sums[offset + (size_t)component]
                    / (double)weight;
            }
        }
    }
    for (cluster_index = 0U; cluster_index < cluster_count;
            ++cluster_index) {
        weight = cluster_weight[cluster_index];
        offset = (size_t)cluster_index * (size_t)depth;
        if (weight == 0UL) {
            for (component = 0U; component < depth; ++component) {
                channel = centers[offset + (size_t)component];
                if (channel < 0.0) {
                    channel = 0.0;
                }
                if (channel > 255.0) {
                    channel = 255.0;
                }
                cluster_sums[offset + (size_t)component]
                    = (unsigned long)(channel + 0.5);
            }
            cluster_weight[cluster_index] = 1UL;
        }
    }
    free(centers);
}


/* Merge clusters using either Ward linkage or hierarchical k-means. */
static int
sixel_palette_apply_merge(unsigned long *weights,
                          unsigned long *sums,
                          unsigned int depth,
                          int cluster_count,
                          int target,
                          int final_merge_mode,
                          int use_reversible,
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
    unsigned long sum_value;

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
            component_r = (double)sums[(size_t)index * (size_t)depth]
                / weight;
        }
        if (depth > 1U) {
            component_g = (double)sums[(size_t)index * (size_t)depth + 1U]
                / weight;
        } else {
            component_g = component_r;
        }
        if (depth > 2U) {
            component_b =
                (double)sums[(size_t)index * (size_t)depth + 2U] / weight;
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
    if (resolved_mode == SIXEL_FINAL_MERGE_HKMEANS) {
        dispatch_mode = SIXEL_FINAL_MERGE_DISPATCH_HKMEANS;
    } else if (resolved_mode == SIXEL_FINAL_MERGE_WARD) {
        dispatch_mode = SIXEL_FINAL_MERGE_DISPATCH_WARD;
    }
    sixel_final_merge_clusters(
        clusters, cluster_count, target, dispatch_mode, use_reversible);
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
            if (scaled > (double)ULONG_MAX) {
                scaled = (double)ULONG_MAX;
            }
            sum_value = (unsigned long)(scaled + 0.5);
            sums[(size_t)result * (size_t)depth + (size_t)plane] = sum_value;
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
            if (scaled > (double)ULONG_MAX) {
                scaled = (double)ULONG_MAX;
            }
            sum_value = (unsigned long)(scaled + 0.5);
            sums[(size_t)0 * (size_t)depth + (size_t)plane] = sum_value;
        }
        result = 1;
    }
    for (index = result; index < cluster_count; ++index) {
        weights[index] = 0UL;
        for (plane = 0; plane < (int)depth; ++plane) {
            sums[(size_t)index * (size_t)depth + (size_t)plane] = 0UL;
        }
    }
    sixel_allocator_free(allocator, clusters);

    return result;
}


/* Translate merged cluster statistics into a tupletable palette. */
static SIXELSTATUS
sixel_palette_clusters_to_colormap(unsigned long *weights,
                                   unsigned long *sums,
                                   unsigned int depth,
                                   unsigned int cluster_count,
                                   int use_reversible,
                                   tupletable2 *colormapP,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    tupletable2 colormap;
    unsigned int index;
    unsigned int plane;
    double component;
    unsigned long weight;

    status = SIXEL_BAD_ARGUMENT;
    if (colormapP == NULL) {
        return status;
    }
    colormap = newColorMap(cluster_count, depth, allocator);
    if (colormap.size == 0U) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0U; index < cluster_count; ++index) {
        weight = weights[index];
        if (weight == 0UL) {
            weight = 1UL;
        }
        colormap.table[index]->value =
            (unsigned int)((weight > (unsigned long)UINT_MAX)
                ? UINT_MAX
                : weight);
        for (plane = 0U; plane < depth; ++plane) {
            component = (double)sums[(size_t)index * (size_t)depth + plane];
            component /= (double)weight;
            if (component < 0.0) {
                component = 0.0;
            }
            if (component > 255.0) {
                component = 255.0;
            }
            colormap.table[index]->tuple[plane] =
                (sample)(component + 0.5);
        }
        if (use_reversible) {
            sixel_palette_reversible_tuple(colormap.table[index]->tuple,
                                          depth);
        }
    }
    *colormapP = colormap;
    status = SIXEL_OK;

    return status;
}


static SIXELSTATUS
computeColorMapFromInput(unsigned char const *data,
                         unsigned int const length,
                         unsigned int const depth,
                         unsigned int const reqColors,
                         int const methodForLargest,
                         int const methodForRep,
                         int const qualityMode,
                         int const force_palette,
                         int const use_reversible,
                         int const final_merge_mode,
                         int const lut_policy,
                         tupletable2 * const colormapP,
                         unsigned int *origcolors,
                         sixel_allocator_t *allocator)
{
/*----------------------------------------------------------------------------
   Produce a colormap containing the best colors to represent the
   image stream in file 'ifP'.  Figure it out using the median cut
   technique.

   The colormap will have 'reqcolors' or fewer colors in it, unless
   'allcolors' is true, in which case it will have all the colors that
   are in the input.

   The colormap has the same maxval as the input.

   Put the colormap in newly allocated storage as a tupletable2
   and return its address as *colormapP.  Return the number of colors in
   it as *colorsP and its maxval as *colormapMaxvalP.

   Return the characteristics of the input file as
   *formatP and *freqPamP.  (This information is not really
   relevant to our colormap mission; just a fringe benefit).
-----------------------------------------------------------------------------*/
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colorfreqtable = {0, NULL};
    unsigned int i;
    unsigned int n;

    /*
     * Build a histogram using the same LUT policy that the palette
     * application stage will employ.  This keeps bucket packing and
     * sparse table strategies consistent across the pipeline.
     */
    status = sixel_lut_build_histogram(data,
                                       length,
                                       depth,
                                       qualityMode,
                                       use_reversible,
                                       lut_policy,
                                       &colorfreqtable,
                                       allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (origcolors) {
        *origcolors = colorfreqtable.size;
    }

    if (colorfreqtable.size <= reqColors) {
        sixel_debugf("Image already has few enough colors (<=%u). "
                     "Keeping same colors.",
                     reqColors);
        /* *colormapP = colorfreqtable; */
        colormapP->size = colorfreqtable.size;
        status = alloctupletable(&colormapP->table,
                                 depth,
                                 colorfreqtable.size,
                                 allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        for (i = 0; i < colorfreqtable.size; ++i) {
            colormapP->table[i]->value = colorfreqtable.table[i]->value;
            for (n = 0; n < depth; ++n) {
                colormapP->table[i]->tuple[n] =
                    colorfreqtable.table[i]->tuple[n];
            }
            if (use_reversible) {
                sixel_palette_reversible_tuple(colormapP->table[i]->tuple,
                                              depth);
            }
        }
    } else {
        sixel_debugf("choosing %u colors...", reqColors);
        status = mediancut(colorfreqtable, depth, reqColors,
                           methodForLargest, methodForRep,
                           use_reversible, final_merge_mode,
                           colormapP, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_debugf("%u colors are chosen.",
                     colorfreqtable.size);
    }
    if (force_palette) {
        status = force_palette_completion(colormapP, depth, reqColors,
                                          colorfreqtable, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, colorfreqtable.table);
    return status;
}


static SIXELSTATUS
build_palette_kmeans(unsigned char **result,
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
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int channels;
    unsigned int pixel_count;
    unsigned int sample_limit;
    unsigned int sample_cap;
    unsigned int valid_seen;
    unsigned int sample_count;
    unsigned int k;
    unsigned int index;
    unsigned int channel;
    unsigned int center_index;
    unsigned int sample_index;
    unsigned int replace;
    unsigned int max_iterations;
    unsigned int iteration;
    unsigned int best_index;
    unsigned int old_cluster;
    unsigned int farthest_index;
    unsigned int fill;
    unsigned int source;
    unsigned int swap_temp;
    unsigned int base;
    unsigned int extra_component;
    unsigned int unique_colors;
    unsigned int *membership;
    unsigned int *order;
    unsigned char *samples;
    unsigned char *palette;
    unsigned char *new_palette;
    double *centers;
    double *distance_cache;
    double total_weight;
    double random_point;
    double best_distance;
    double distance;
    double diff;
    double update;
    double farthest_distance;
    double delta;
    double lloyd_threshold;
    unsigned long *counts;
    unsigned long *accum;
    unsigned long *channel_sum;
    unsigned long rand_value;
    int apply_merge;
    int resolved_merge;
    unsigned int overshoot;
    unsigned int refine_iterations;
    int cluster_total;
    int unique_within;
    SIXELSTATUS unique_status;

    status = SIXEL_BAD_ARGUMENT;
    channels = depth;
    pixel_count = 0U;
    sample_limit = 50000U;
    sample_cap = sample_limit;
    valid_seen = 0U;
    sample_count = 0U;
    k = 0U;
    index = 0U;
    channel = 0U;
    center_index = 0U;
    sample_index = 0U;
    replace = 0U;
    max_iterations = 0U;
    iteration = 0U;
    best_index = 0U;
    old_cluster = 0U;
    farthest_index = 0U;
    fill = 0U;
    source = 0U;
    swap_temp = 0U;
    base = 0U;
    extra_component = 0U;
    unique_colors = 0U;
    membership = NULL;
    order = NULL;
    samples = NULL;
    palette = NULL;
    new_palette = NULL;
    centers = NULL;
    distance_cache = NULL;
    counts = NULL;
    accum = NULL;
    channel_sum = NULL;
    rand_value = 0UL;
    total_weight = 0.0;
    random_point = 0.0;
    best_distance = 0.0;
    distance = 0.0;
    diff = 0.0;
    update = 0.0;
    farthest_distance = 0.0;
    apply_merge = 0;
    resolved_merge = SIXEL_FINAL_MERGE_NONE;
    overshoot = 0U;
    refine_iterations = 0U;
    cluster_total = 0;
    unique_within = 0;
    unique_status = SIXEL_OK;

    if (result != NULL) {
        *result = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = 0U;
    }
    if (origcolors != NULL) {
        *origcolors = 0U;
    }

    if (channels != 3U && channels != 4U) {
        goto end;
    }
    if (channels == 0U) {
        goto end;
    }

    pixel_count = length / channels;
    if (pixel_count == 0U) {
        goto end;
    }
    if (pixel_count < sample_cap) {
        sample_cap = pixel_count;
    }
    if (sample_cap == 0U) {
        sample_cap = 1U;
    }

    samples = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)sample_cap * 3U);
    if (samples == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /*
     * Reservoir sampling keeps the distribution fair when the image is
     * larger than our budget. Transparent pixels are skipped so that the
     * solver only sees visible colors.
     */
    for (index = 0U; index < pixel_count; ++index) {
        base = index * channels;
        if (channels == 4U && data[base + 3U] == 0U) {
            continue;
        }
        ++valid_seen;
        if (sample_count < sample_cap) {
            for (channel = 0U; channel < 3U; ++channel) {
                samples[sample_count * 3U + channel] =
                    data[base + channel];
            }
            ++sample_count;
        } else {
            rand_value = (unsigned long)rand();
            replace = (unsigned int)(rand_value % valid_seen);
            if (replace < sample_cap) {
                for (channel = 0U; channel < 3U; ++channel) {
                    samples[replace * 3U + channel] =
                        data[base + channel];
                }
            }
        }
    }

    if (origcolors != NULL) {
        *origcolors = valid_seen;
    }
    if (sample_count == 0U) {
        goto end;
    }

    if (reqcolors == 0U) {
        reqcolors = 1U;
    }
    resolved_merge = sixel_resolve_final_merge_mode(final_merge_mode);
    apply_merge = (resolved_merge == SIXEL_FINAL_MERGE_WARD
                   || resolved_merge == SIXEL_FINAL_MERGE_HKMEANS);
    if (apply_merge) {
        /*
         * Skip Ward merging when the source already uses fewer colors
         * than requested.  The helper only inspects opaque pixels.
         */
        unique_status = sixel_palette_count_unique_within_limit(
            data, length, channels, reqcolors, &unique_colors,
            &unique_within, allocator);
        if (unique_status == SIXEL_OK && unique_within != 0) {
            apply_merge = 0;
        }
    }
    overshoot = reqcolors;
    /* Oversplit so the subsequent Ward merge has room to consolidate. */
    if (apply_merge) {
        sixel_final_merge_load_env();
        refine_iterations
            = sixel_final_merge_lloyd_iterations(resolved_merge);
        overshoot = sixel_final_merge_target(reqcolors, resolved_merge);
        sixel_debugf("overshoot: %u", overshoot);
    }
    if (overshoot > sample_count) {
        overshoot = sample_count;
    }
    k = overshoot;
    if (k == 0U) {
        goto end;
    }

    centers = (double *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(double));
    distance_cache = (double *)sixel_allocator_malloc(
        allocator, (size_t)sample_count * sizeof(double));
    counts = (unsigned long *)sixel_allocator_malloc(
        allocator, (size_t)k * sizeof(unsigned long));
    accum = (unsigned long *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U * sizeof(unsigned long));
    membership = (unsigned int *)sixel_allocator_malloc(
        allocator, (size_t)sample_count * sizeof(unsigned int));
    if (centers == NULL || distance_cache == NULL || counts == NULL ||
            accum == NULL || membership == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    /*
     * Seed the first center uniformly from the sampled set. Subsequent
     * centers use k-means++ to favour distant samples.
     */
    rand_value = (unsigned long)rand();
    replace = (unsigned int)(rand_value % sample_count);
    for (channel = 0U; channel < 3U; ++channel) {
        centers[channel] =
            (double)samples[replace * 3U + channel];
    }
    for (sample_index = 0U; sample_index < sample_count; ++sample_index) {
        distance = 0.0;
        for (channel = 0U; channel < 3U; ++channel) {
            diff = (double)samples[sample_index * 3U + channel]
                - centers[channel];
            distance += diff * diff;
        }
        distance_cache[sample_index] = distance;
    }

    for (center_index = 1U; center_index < k; ++center_index) {
        total_weight = 0.0;
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            total_weight += distance_cache[sample_index];
        }
        random_point = 0.0;
        if (total_weight > 0.0) {
            random_point =
                ((double)rand() / ((double)RAND_MAX + 1.0)) *
                total_weight;
        }
        sample_index = 0U;
        while (sample_index + 1U < sample_count &&
               random_point > distance_cache[sample_index]) {
            random_point -= distance_cache[sample_index];
            ++sample_index;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            centers[center_index * 3U + channel] =
                (double)samples[sample_index * 3U + channel];
        }
        for (index = 0U; index < sample_count; ++index) {
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[index * 3U + channel]
                    - centers[center_index * 3U + channel];
                distance += diff * diff;
            }
            if (distance < distance_cache[index]) {
                distance_cache[index] = distance;
            }
        }
    }

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        max_iterations = 6U;
        break;
    case SIXEL_QUALITY_HIGH:
        max_iterations = 24U;
        break;
    case SIXEL_QUALITY_FULL:
        max_iterations = 48U;
        break;
    case SIXEL_QUALITY_HIGHCOLOR:
        max_iterations = 24U;
        break;
    case SIXEL_QUALITY_AUTO:
    default:
        max_iterations = 12U;
        break;
    }
    if (max_iterations == 0U) {
        max_iterations = 1U;
    }
    sixel_final_merge_load_env();
    if (max_iterations > env_kmeans_iter_max) {
        max_iterations = env_kmeans_iter_max;
    }
    if (max_iterations == 0U) {
        max_iterations = 1U;
    }
    lloyd_threshold = env_kmeans_threshold;
    /*
     * Lloyd refinement assigns samples to their nearest center and moves
     * each center to the mean of its cluster. Empty clusters are reseeded
     * using the farthest sample to improve stability.
     */
    for (iteration = 0U; iteration < max_iterations; ++iteration) {
        for (index = 0U; index < k; ++index) {
            counts[index] = 0UL;
        }
        for (index = 0U; index < k * 3U; ++index) {
            accum[index] = 0UL;
        }
        for (sample_index = 0U; sample_index < sample_count;
                ++sample_index) {
            best_index = 0U;
            distance = 0.0;
            for (channel = 0U; channel < 3U; ++channel) {
                diff = (double)samples[sample_index * 3U + channel]
                    - centers[channel];
                distance += diff * diff;
            }
            best_distance = distance;
            for (center_index = 1U; center_index < k;
                    ++center_index) {
                distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)samples[sample_index * 3U + channel]
                        - centers[center_index * 3U + channel];
                    distance += diff * diff;
                }
                if (distance < best_distance) {
                    best_distance = distance;
                    best_index = center_index;
                }
            }
            membership[sample_index] = best_index;
            distance_cache[sample_index] = best_distance;
            counts[best_index] += 1UL;
            channel_sum = accum + (size_t)best_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] +=
                    (unsigned long)samples[sample_index * 3U + channel];
            }
        }
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] != 0UL) {
                continue;
            }
            farthest_distance = -1.0;
            farthest_index = 0U;
            for (sample_index = 0U; sample_index < sample_count;
                    ++sample_index) {
                if (distance_cache[sample_index] > farthest_distance) {
                    farthest_distance = distance_cache[sample_index];
                    farthest_index = sample_index;
                }
            }
            old_cluster = membership[farthest_index];
            if (counts[old_cluster] > 0UL) {
                counts[old_cluster] -= 1UL;
                channel_sum = accum + (size_t)old_cluster * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    extra_component =
                        (unsigned int)samples[farthest_index * 3U + channel];
                    if (channel_sum[channel] >=
                            (unsigned long)extra_component) {
                        channel_sum[channel] -=
                            (unsigned long)extra_component;
                    } else {
                        channel_sum[channel] = 0UL;
                    }
                }
            }
            membership[farthest_index] = center_index;
            counts[center_index] = 1UL;
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                channel_sum[channel] =
                    (unsigned long)samples[farthest_index * 3U + channel];
            }
            distance_cache[farthest_index] = 0.0;
        }
        delta = 0.0;
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] == 0UL) {
                continue;
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                update = (double)channel_sum[channel]
                    / (double)counts[center_index];
                diff = centers[center_index * 3U + channel] - update;
                delta += diff * diff;
                centers[center_index * 3U + channel] = update;
            }
        }
        if (delta <= lloyd_threshold) {
            break;
        }
    }

    if (apply_merge && k > reqcolors) {
        /* Merge the provisional clusters and polish with a few Lloyd steps. */
        cluster_total = sixel_palette_apply_merge(counts,
                                                 accum,
                                                 3U,
                                                 (int)k,
                                                 (int)reqcolors,
                                                 resolved_merge,
                                                 use_reversible,
                                                 allocator);
        if (cluster_total < 1) {
            cluster_total = 1;
        }
        if ((unsigned int)cluster_total > reqcolors) {
            cluster_total = (int)reqcolors;
        }
        k = (unsigned int)cluster_total;
        if (k == 0U) {
            k = 1U;
        }
        for (center_index = 0U; center_index < k; ++center_index) {
            if (counts[center_index] == 0UL) {
                counts[center_index] = 1UL;
            }
            channel_sum = accum + (size_t)center_index * 3U;
            for (channel = 0U; channel < 3U; ++channel) {
                centers[center_index * 3U + channel] =
                    (double)channel_sum[channel]
                    / (double)counts[center_index];
            }
        }
        for (iteration = 0U; iteration < refine_iterations; ++iteration) {
            for (index = 0U; index < k; ++index) {
                counts[index] = 0UL;
            }
            for (index = 0U; index < k * 3U; ++index) {
                accum[index] = 0UL;
            }
            for (sample_index = 0U; sample_index < sample_count;
                    ++sample_index) {
                best_index = 0U;
                best_distance = 0.0;
                for (channel = 0U; channel < 3U; ++channel) {
                    diff = (double)samples[sample_index * 3U + channel]
                        - centers[channel];
                    best_distance += diff * diff;
                }
                for (center_index = 1U; center_index < k;
                        ++center_index) {
                    distance = 0.0;
                    for (channel = 0U; channel < 3U; ++channel) {
                        diff = (double)samples[sample_index * 3U + channel]
                            - centers[center_index * 3U + channel];
                        distance += diff * diff;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = center_index;
                    }
                }
                membership[sample_index] = best_index;
                distance_cache[sample_index] = best_distance;
                counts[best_index] += 1UL;
                channel_sum = accum + (size_t)best_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] +=
                        (unsigned long)samples[sample_index * 3U + channel];
                }
            }
            for (center_index = 0U; center_index < k; ++center_index) {
                if (counts[center_index] != 0UL) {
                    continue;
                }
                farthest_distance = -1.0;
                farthest_index = 0U;
                for (sample_index = 0U; sample_index < sample_count;
                        ++sample_index) {
                    if (distance_cache[sample_index] > farthest_distance) {
                        farthest_distance = distance_cache[sample_index];
                        farthest_index = sample_index;
                    }
                }
                old_cluster = membership[farthest_index];
                if (counts[old_cluster] > 0UL) {
                    counts[old_cluster] -= 1UL;
                    channel_sum = accum + (size_t)old_cluster * 3U;
                    for (channel = 0U; channel < 3U; ++channel) {
                        extra_component =
                            (unsigned int)samples[farthest_index * 3U + channel];
                        if (channel_sum[channel] >=
                                (unsigned long)extra_component) {
                            channel_sum[channel] -=
                                (unsigned long)extra_component;
                        } else {
                            channel_sum[channel] = 0UL;
                        }
                    }
                }
                membership[farthest_index] = center_index;
                counts[center_index] = 1UL;
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    channel_sum[channel] =
                        (unsigned long)samples[farthest_index * 3U + channel];
                }
                distance_cache[farthest_index] = 0.0;
            }
            delta = 0.0;
            for (center_index = 0U; center_index < k; ++center_index) {
                if (counts[center_index] == 0UL) {
                    continue;
                }
                channel_sum = accum + (size_t)center_index * 3U;
                for (channel = 0U; channel < 3U; ++channel) {
                    update = (double)channel_sum[channel]
                        / (double)counts[center_index];
                    diff = centers[center_index * 3U + channel] - update;
                    delta += diff * diff;
                    centers[center_index * 3U + channel] = update;
                }
            }
            if (delta <= lloyd_threshold) {
                break;
            }
        }
    }

    /*
     * Convert the floating point centers back into the byte palette that
     * callers expect.
     */
    palette = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)k * 3U);
    if (palette == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (center_index = 0U; center_index < k; ++center_index) {
        for (channel = 0U; channel < 3U; ++channel) {
            update = centers[center_index * 3U + channel];
            if (update < 0.0) {
                update = 0.0;
            }
            if (update > 255.0) {
                update = 255.0;
            }
            palette[center_index * 3U + channel] =
                (unsigned char)(update + 0.5);
        }
    }

    if (force_palette && k < reqcolors) {
        /*
         * Populate the tail of the palette by repeating the most frequent
         * clusters so the caller still receives the requested palette size.
         */
        new_palette = (unsigned char *)sixel_allocator_malloc(
            allocator, (size_t)reqcolors * 3U);
        if (new_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k * 3U; ++index) {
            new_palette[index] = palette[index];
        }
        order = (unsigned int *)sixel_allocator_malloc(
            allocator, (size_t)k * sizeof(unsigned int));
        if (order == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0U; index < k; ++index) {
            order[index] = index;
        }
        for (index = 0U; index < k; ++index) {
            for (center_index = index + 1U; center_index < k;
                    ++center_index) {
                if (counts[order[center_index]] >
                        counts[order[index]]) {
                    swap_temp = order[index];
                    order[index] = order[center_index];
                    order[center_index] = swap_temp;
                }
            }
        }
        fill = k;
        source = 0U;
        while (fill < reqcolors && k > 0U) {
            center_index = order[source];
            for (channel = 0U; channel < 3U; ++channel) {
                new_palette[fill * 3U + channel] =
                    palette[center_index * 3U + channel];
            }
            ++fill;
            ++source;
            if (source >= k) {
                source = 0U;
            }
        }
        sixel_allocator_free(allocator, palette);
        palette = new_palette;
        new_palette = NULL;
        k = reqcolors;
    }

    status = SIXEL_OK;
    if (result != NULL) {
        *result = palette;
    } else {
        palette = NULL;
    }
    if (ncolors != NULL) {
        *ncolors = k;
    }

end:
    if (status != SIXEL_OK && palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (new_palette != NULL) {
        sixel_allocator_free(allocator, new_palette);
    }
    if (order != NULL) {
        sixel_allocator_free(allocator, order);
    }
    if (membership != NULL) {
        sixel_allocator_free(allocator, membership);
    }
    if (accum != NULL) {
        sixel_allocator_free(allocator, accum);
    }
    if (counts != NULL) {
        sixel_allocator_free(allocator, counts);
    }
    if (distance_cache != NULL) {
        sixel_allocator_free(allocator, distance_cache);
    }
    if (centers != NULL) {
        sixel_allocator_free(allocator, centers);
    }
    if (samples != NULL) {
        sixel_allocator_free(allocator, samples);
    }
    return status;
}




static void
sixel_palette_dispose(sixel_palette_t *palette);

/*
 * Resize the palette entry buffer.
 *
 * The helper keeps the allocation logic in a single place so both k-means and
 * median-cut paths can rely on the same growth strategy.  When the caller
 * requests a size of zero the buffer is released entirely.
 */
static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator);

SIXELAPI SIXELSTATUS
sixel_palette_new(sixel_palette_t **palette, sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *object;

    if (palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_new: palette pointer is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            *palette = NULL;
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    object = (sixel_palette_t *)sixel_allocator_malloc(
        allocator, sizeof(*object));
    if (object == NULL) {
        sixel_allocator_unref(allocator);
        sixel_helper_set_additional_message(
            "sixel_palette_new: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        *palette = NULL;
        goto end;
    }

    object->ref = 1U;
    object->allocator = allocator;
    object->entries = NULL;
    object->entries_size = 0U;
    object->entry_count = 0U;
    object->requested_colors = 0U;
    object->original_colors = 0U;
    object->depth = 0;
    object->method_for_largest = SIXEL_LARGE_AUTO;
    object->method_for_rep = SIXEL_REP_AUTO;
    object->quality_mode = SIXEL_QUALITY_AUTO;
    object->force_palette = 0;
    object->use_reversible = 0;
    object->quantize_model = SIXEL_QUANTIZE_MODEL_AUTO;
    object->final_merge_mode = SIXEL_FINAL_MERGE_AUTO;
    object->complexion = 1;
    object->lut_policy = SIXEL_LUT_POLICY_AUTO;
    object->sixel_reversible = 0;
    object->final_merge = 0;
    object->lut = NULL;

    *palette = object;
    status = SIXEL_OK;

end:
    return status;
}

SIXELAPI sixel_palette_t *
sixel_palette_ref(sixel_palette_t *palette)
{
    if (palette != NULL) {
        ++palette->ref;
    }

    return palette;
}

static void
sixel_palette_dispose(sixel_palette_t *palette)
{
    sixel_allocator_t *allocator;

    if (palette == NULL) {
        return;
    }

    allocator = palette->allocator;
    if (palette->entries != NULL) {
        sixel_allocator_free(allocator, palette->entries);
        palette->entries = NULL;
    }

    if (palette->lut != NULL) {
        sixel_lut_unref(palette->lut);
        palette->lut = NULL;
    }

    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI void
sixel_palette_unref(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return;
    }

    if (palette->ref > 1U) {
        --palette->ref;
        return;
    }

    sixel_palette_dispose(palette);
    sixel_allocator_free(palette->allocator, palette);
}

static SIXELSTATUS
sixel_palette_resize_entries(sixel_palette_t *palette,
                             unsigned int colors,
                             unsigned int depth,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t required;
    unsigned char *resized;

    if (palette == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    required = (size_t)colors * (size_t)depth;
    if (required == 0U) {
        if (palette->entries != NULL) {
            sixel_allocator_free(allocator, palette->entries);
            palette->entries = NULL;
        }
        palette->entries_size = 0U;
        return SIXEL_OK;
    }

    if (palette->entries != NULL && palette->entries_size >= required) {
        return SIXEL_OK;
    }

    if (palette->entries == NULL) {
        resized = (unsigned char *)sixel_allocator_malloc(allocator, required);
    } else {
        resized = (unsigned char *)sixel_allocator_realloc(allocator,
                                                           palette->entries,
                                                           required);
    }
    if (resized == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_resize_entries: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    palette->entries = resized;
    palette->entries_size = required;

    status = SIXEL_OK;

    return status;
}

SIXELAPI SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *work_allocator;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize_entries(palette,
                                          colors,
                                          (unsigned int)depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette->entry_count = colors;
    palette->depth = depth;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_palette_resize(palette, colors, depth, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    payload_size = (size_t)colors * (size_t)depth;
    if (entries != NULL && palette->entries != NULL && payload_size > 0U) {
        memcpy(palette->entries, entries, payload_size);
    }

    return SIXEL_OK;
}

SIXELAPI unsigned char *
sixel_palette_get_entries(sixel_palette_t *palette)
{
    if (palette == NULL) {
        return NULL;
    }

    return palette->entries;
}

SIXELAPI unsigned int
sixel_palette_get_entry_count(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0U;
    }

    return palette->entry_count;
}

SIXELAPI int
sixel_palette_get_depth(sixel_palette_t const *palette)
{
    if (palette == NULL) {
        return 0;
    }

    return palette->depth;
}

/*
 * sixel_palette_generate builds the palette entries inside the provided
 * sixel_palette_t instance.  The procedure performs two major steps:
 *
 *   1. Try the k-means implementation when explicitly requested.  Successful
 *      runs populate a temporary buffer that is copied into the palette.
 *   2. Fall back to the existing histogram and median-cut pipeline.  The
 *      resulting tuple table is converted into tightly packed palette bytes.
 *
 * Both branches share helper routines for cache management and post-processing
 * (for example reversible palette transformation).  The palette object tracks
 * the generated metadata so the caller can publish it without recomputing.
 */
SIXELSTATUS
sixel_palette_generate(sixel_palette_t *palette,
                       unsigned char const *data,
                       unsigned int length,
                       int pixelformat,
                       sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    tupletable2 colormap = { 0U, NULL };
    unsigned char *kmeans_entries = NULL;
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    sixel_allocator_t *work_allocator;
    size_t payload_size;

    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    work_allocator = allocator;
    if (work_allocator == NULL) {
        work_allocator = palette->allocator;
    }
    if (work_allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result_depth = sixel_helper_compute_depth(pixelformat);
    if (result_depth <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: invalid pixel format depth.");
        return SIXEL_BAD_ARGUMENT;
    }
    depth = (unsigned int)result_depth;

    status = SIXEL_FALSE;
    payload_size = 0U;

    if (palette->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = build_palette_kmeans(&kmeans_entries,
                                      data,
                                      length,
                                      depth,
                                      palette->requested_colors,
                                      &ncolors,
                                      &origcolors,
                                      palette->quality_mode,
                                      palette->force_palette,
                                      palette->use_reversible,
                                      palette->final_merge_mode,
                                      work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            if (palette->use_reversible) {
                sixel_palette_reversible_palette(kmeans_entries,
                                                 ncolors,
                                                 depth);
            }
            payload_size = (size_t)ncolors * (size_t)depth;
            status = sixel_palette_resize_entries(palette,
                                                  ncolors,
                                                  depth,
                                                  work_allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (payload_size > 0U) {
                memcpy(palette->entries, kmeans_entries, payload_size);
            }
            status = SIXEL_OK;
            goto success;
        }
    }

    status = computeColorMapFromInput(data,
                                      length,
                                      depth,
                                      palette->requested_colors,
                                      palette->method_for_largest,
                                      palette->method_for_rep,
                                      palette->quality_mode,
                                      palette->force_palette,
                                      palette->use_reversible,
                                      palette->final_merge_mode,
                                      palette->lut_policy,
                                      &colormap,
                                      &origcolors,
                                      work_allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = colormap.size;
    status = sixel_palette_resize_entries(palette,
                                          ncolors,
                                          depth,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    payload_size = (size_t)ncolors * (size_t)depth;
    if (payload_size > 0U && palette->entries != NULL) {
        unsigned int i;
        unsigned int plane;

        for (i = 0U; i < ncolors; ++i) {
            for (plane = 0U; plane < depth; ++plane) {
                palette->entries[i * depth + plane]
                    = (unsigned char)colormap.table[i]->tuple[plane];
            }
        }
    }
    if (palette->use_reversible && palette->entries != NULL) {
        sixel_palette_reversible_palette(palette->entries,
                                         ncolors,
                                         depth);
    }
    status = SIXEL_OK;

success:
    palette->entry_count = ncolors;
    palette->original_colors = origcolors;
    palette->depth = (int)depth;

end:
    if (colormap.table != NULL) {
        sixel_allocator_free(work_allocator, colormap.table);
    }
    if (kmeans_entries != NULL) {
        sixel_allocator_free(work_allocator, kmeans_entries);
    }
    return status;
}

SIXELSTATUS
sixel_palette_make_palette(unsigned char **result,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           unsigned int reqcolors,
                           unsigned int *ncolors,
                           unsigned int *origcolors,
                           int methodForLargest,
                           int methodForRep,
                           int qualityMode,
                           int force_palette,
                           int use_reversible,
                           int quantize_model,
                           int final_merge_mode,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_palette_t *palette = NULL;
    sixel_allocator_t *work_allocator;
    size_t payload_size;
    unsigned int depth;

    if (result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;

    status = sixel_palette_new(&palette, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (methodForLargest == SIXEL_LARGE_AUTO) {
        methodForLargest = palette_method_for_largest;
    }

    palette->requested_colors = reqcolors;
    palette->method_for_largest = methodForLargest;
    palette->method_for_rep = methodForRep;
    palette->quality_mode = qualityMode;
    palette->force_palette = force_palette;
    palette->use_reversible = use_reversible;
    palette->quantize_model = quantize_model;
    palette->final_merge_mode = final_merge_mode;
    palette->lut_policy = palette_default_lut_policy;

    status = sixel_palette_generate(palette,
                                    data,
                                    length,
                                    pixelformat,
                                    allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (ncolors != NULL) {
        *ncolors = palette->entry_count;
    }
    if (origcolors != NULL) {
        *origcolors = palette->original_colors;
    }

    if (palette->depth <= 0 || palette->entry_count == 0U) {
        status = SIXEL_OK;
        goto end;
    }

    depth = (unsigned int)palette->depth;
    payload_size = (size_t)palette->entry_count * (size_t)depth;
    work_allocator = (allocator != NULL) ? allocator : palette->allocator;
    if (work_allocator == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    *result = (unsigned char *)sixel_allocator_malloc(work_allocator,
                                                      payload_size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_make_palette: allocation failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(*result, palette->entries, payload_size);

    status = SIXEL_OK;

end:
    if (palette != NULL) {
        sixel_palette_unref(palette);
    }
    return status;
}

void
sixel_palette_free_palette(unsigned char *data,
                           sixel_allocator_t *allocator)
{
    sixel_allocator_free(allocator, data);
}

#if HAVE_TESTS
static int
palette_test_luminosity(void)
{
    int nret = EXIT_FAILURE;
    sample minval[1] = { 1 };
    sample maxval[1] = { 2 };
    unsigned int retval;

    retval = largestByLuminosity(minval, maxval, 1);
    if (retval != 0) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}

SIXELAPI int
sixel_palette_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (*palette_testcase)(void);

    static palette_testcase const testcases[] = {
        palette_test_luminosity,
    };

    for (i = 0U; i < sizeof(testcases) / sizeof(palette_testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/*
 * Palette generation, reversible tone helpers, and histogram coordination now
 * live together in this module.  Future refactors can continue to fold any
 * remaining quantization utilities here so palette.c stays the central entry
 * point for palette lifecycle management.
 */
