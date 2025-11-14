/*
 * SPDX-License-Identifier: MIT
 *
 * Palette orchestration layer shared by every quantizer.  The code below keeps
 * the environment readers, reversible palette helpers, and final merge logic in
 * one place so the algorithm-specific sources only need to request services.
 * The diagram summarises the steady-state flow:
 *
 *   load env -> dispatch -> build clusters -> final merge -> publish entries
 *        ^                 (median-cut / k-means)                 |
 *        '-------------------------------------------------------'
 *
 * The shared helpers are grouped by phase to make the interactions discoverable
 * when adjusting palette-heckbert.c or palette-kmeans.c.
 */

#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include <errno.h>

#include "lut.h"
#include "palette-internal.h"
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
 * Palette orchestration now delegates median-cut specifics to
 * palette-heckbert.c while keeping the shared final merge, k-means, and
 * allocator plumbing here.  The commentary focuses on how the common
 * components coordinate both algorithms.
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
void sixel_final_merge_load_env(void);

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
void
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

/*
 * Provide accessor functions for k-means tuning knobs.  The helpers guarantee
 * that environment overrides are processed exactly once before the values are
 * observed by palette-kmeans.c.
 */
unsigned int
sixel_palette_kmeans_iter_max(void)
{
    sixel_final_merge_load_env();
    return env_kmeans_iter_max;
}

double
sixel_palette_kmeans_threshold(void)
{
    sixel_final_merge_load_env();
    return env_kmeans_threshold;
}

unsigned int
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

/* Determine how many clusters to create before the final merge step. */
unsigned int
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
void
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
int
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


/* Count unique RGB triples until the limit is exceeded. */
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
 *      runs update the palette object in-place and return immediately.
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
    unsigned int ncolors = 0U;
    unsigned int origcolors = 0U;
    unsigned int depth = 0U;
    int result_depth;
    sixel_allocator_t *work_allocator;

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

    if (palette->quantize_model == SIXEL_QUANTIZE_MODEL_KMEANS) {
        status = sixel_palette_build_kmeans(palette,
                                            data,
                                            length,
                                            pixelformat,
                                            work_allocator);
        if (SIXEL_SUCCEEDED(status)) {
            ncolors = palette->entry_count;
            origcolors = palette->original_colors;
            depth = (unsigned int)palette->depth;
            goto success;
        }
    }

    status = sixel_palette_build_heckbert(palette,
                                          data,
                                          length,
                                          pixelformat,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_palette_generate: color map construction failed.");
        goto end;
    }

    ncolors = palette->entry_count;
    origcolors = palette->original_colors;
    depth = (unsigned int)palette->depth;
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
