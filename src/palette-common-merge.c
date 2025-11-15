/*
 * SPDX-License-Identifier: MIT
 *
 * Shared final merge utilities for palette quantizers.  The helpers centralise
 * environment handling and the Ward/HK-means refinement logic so
 * palette-heckbert.c, palette-kmeans.c, and palette.c can share a consistent
 * implementation without depending on each other's internals.
 */

#include "config.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "allocator.h"
#include "compat_stub.h"
#include "lut.h"
#include "palette-common-merge.h"
#include "palette-kmeans.h"

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

static double
sixel_final_merge_distance_sq(sixel_final_merge_cluster_t const *lhs,
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

/*
 * Resolve auto merge requests.  The ladder clarifies the mapping:
 *
 *   AUTO    -> NONE
 *   NONE    -> NONE
 *   WARD    -> WARD
 *   HKMEANS -> HKMEANS
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

static void
sixel_final_merge_clusters(sixel_final_merge_cluster_t *clusters,
                           int nclusters,
                           int target_k,
                           int merge_mode,
                           int use_reversible)
{
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
    }
    for (k = 0; k < nclusters; ++k) {
        if (clusters[k].count <= 0.0) {
            clusters[k].r = 0.0;
            clusters[k].g = 0.0;
            clusters[k].b = 0.0;
        }
    }
}

static void
sixel_final_merge_hkmeans(sixel_final_merge_cluster_t *clusters,
                          int nclusters,
                          int target_k,
                          int use_reversible)
{
    sixel_final_merge_cluster_t *centroids;
    double best_distance;
    double distance;
    double scale;
    double lumin;
    unsigned int iter;
    unsigned int max_iter;
    int best;
    int i;
    int j;
    int k;
    int active;
    int target;
    int centroid_count;
    int resolved;
    int limit;

    centroids = NULL;
    best_distance = 0.0;
    distance = 0.0;
    scale = 0.0;
    lumin = 0.0;
    iter = 0U;
    max_iter = 0U;
    best = -1;
    i = 0;
    j = 0;
    k = 0;
    active = 0;
    target = target_k;
    centroid_count = 0;
    resolved = 0;
    limit = 0;

    if (clusters == NULL || nclusters <= 0) {
        return;
    }
    resolved = target_k;
    if (resolved < 1) {
        resolved = 1;
    }
    centroids = (sixel_final_merge_cluster_t *)sixel_allocator_malloc(
        NULL,
        (size_t)resolved * sizeof(sixel_final_merge_cluster_t));
    if (centroids == NULL) {
        return;
    }

    /* initialise centroids by picking the brightest clusters */
    for (i = 0; i < nclusters; ++i) {
        if (clusters[i].count > 0.0) {
            ++active;
        }
    }
    if (active <= resolved) {
        sixel_allocator_free(NULL, centroids);
        return;
    }
    for (i = 0; i < resolved; ++i) {
        best = -1;
        lumin = -1.0;
        for (j = 0; j < nclusters; ++j) {
            if (clusters[j].count <= 0.0) {
                continue;
            }
            distance = clusters[j].r * env_lumin_factor_r
                + clusters[j].g * env_lumin_factor_g
                + clusters[j].b * env_lumin_factor_b;
            if (distance > lumin) {
                lumin = distance;
                best = j;
            }
        }
        if (best < 0) {
            break;
        }
        centroids[i] = clusters[best];
        clusters[best].count = 0.0;
        ++centroid_count;
    }
    for (i = 0; i < nclusters; ++i) {
        if (clusters[i].count > 0.0) {
            clusters[i].r = sixel_final_merge_snap(
                clusters[i].r, use_reversible);
            clusters[i].g = sixel_final_merge_snap(
                clusters[i].g, use_reversible);
            clusters[i].b = sixel_final_merge_snap(
                clusters[i].b, use_reversible);
        }
    }

    max_iter = env_final_merge_hkmeans_iter_max;
    for (iter = 0U; iter < max_iter; ++iter) {
        double moved;

        moved = 0.0;
        for (i = 0; i < nclusters; ++i) {
            sixel_final_merge_cluster_t const *centroid;

            if (clusters[i].count <= 0.0) {
                continue;
            }
            best = -1;
            best_distance = DBL_MAX;
            for (j = 0; j < centroid_count; ++j) {
                centroid = &centroids[j];
                distance = sixel_final_merge_distance_sq(
                    &clusters[i], centroid);
                if (distance < best_distance) {
                    best_distance = distance;
                    best = j;
                }
            }
            if (best < 0) {
                continue;
            }
            scale = centroids[best].count + clusters[i].count;
            if (scale <= 0.0) {
                scale = 1.0;
            }
            centroids[best].r
                = (centroids[best].r * centroids[best].count
                   + clusters[i].r * clusters[i].count)
                / scale;
            centroids[best].g
                = (centroids[best].g * centroids[best].count
                   + clusters[i].g * clusters[i].count)
                / scale;
            centroids[best].b
                = (centroids[best].b * centroids[best].count
                   + clusters[i].b * clusters[i].count)
                / scale;
            centroids[best].count += clusters[i].count;
            moved += best_distance;
        }
        moved /= (double)nclusters;
        if (moved <= env_final_merge_hkmeans_threshold) {
            break;
        }
        for (k = 0; k < centroid_count; ++k) {
            centroids[k].r
                = sixel_final_merge_snap(centroids[k].r, use_reversible);
            centroids[k].g
                = sixel_final_merge_snap(centroids[k].g, use_reversible);
            centroids[k].b
                = sixel_final_merge_snap(centroids[k].b, use_reversible);
        }
    }

    target = target_k;
    if (target > centroid_count) {
        target = centroid_count;
    }
    limit = target;
    if (limit > nclusters) {
        limit = nclusters;
    }
    for (i = 0; i < limit; ++i) {
        clusters[i] = centroids[i];
    }
    for (i = limit; i < nclusters; ++i) {
        clusters[i].r = 0.0;
        clusters[i].g = 0.0;
        clusters[i].b = 0.0;
        clusters[i].count = 0.0;
    }
    sixel_allocator_free(NULL, centroids);
}

/* Determine how many clusters to create before the final merge step. */
unsigned int
sixel_final_merge_target(unsigned int reqcolors, int final_merge_mode)
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
