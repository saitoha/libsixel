/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

/*
 * Distance kernels and bound-update helpers shared by the K-means build
 * routine.  This translation unit isolates pruning-heavy code paths so
 * compilers with limited function-scale handling can still build reliably.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_FLOAT_H
# include <float.h>
#endif

#include "allocator.h"
#include "compat_stub.h"
#include "logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans.h"
#include "palette-kmeans-assign.h"
#include "palette.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"

static double
sixel_kmeans_center_distance_sq(double const *centers,
                                unsigned int left,
                                unsigned int right)
{
    unsigned int channel;
    double diff;
    double distance_sq;

    channel = 0u;
    diff = 0.0;
    distance_sq = 0.0;
    if (centers == NULL) {
        return 0.0;
    }
    for (channel = 0u; channel < 3u; ++channel) {
        diff = centers[(size_t)left * 3u + channel]
            - centers[(size_t)right * 3u + channel];
        distance_sq += diff * diff;
    }
    return distance_sq;
}

static double
sixel_kmeans_sample_center_distance_sq(double const *samples,
                                       unsigned int sample_index,
                                       double const *centers,
                                       unsigned int center_index)
{
    unsigned int channel;
    size_t sample_base;
    size_t center_base;
    double diff;
    double distance_sq;

    channel = 0u;
    sample_base = 0u;
    center_base = 0u;
    diff = 0.0;
    distance_sq = 0.0;
    if (samples == NULL || centers == NULL) {
        return 0.0;
    }
    sample_base = (size_t)sample_index * 3u;
    center_base = (size_t)center_index * 3u;
    for (channel = 0u; channel < 3u; ++channel) {
        diff = samples[sample_base + channel]
            - centers[center_base + channel];
        distance_sq += diff * diff;
    }
    return distance_sq;
}

static void
sixel_kmeans_fill_lower_matrix_from_samples(double const *centers,
                                            unsigned int k,
                                            double const *samples,
                                            double const *weights,
                                            unsigned int sample_count,
                                            double *lower_matrix)
{
    unsigned int sample_index;
    unsigned int center_index;
    size_t matrix_base;
    double sample_weight;
    double distance_sq;

    sample_index = 0u;
    center_index = 0u;
    matrix_base = 0u;
    sample_weight = 0.0;
    distance_sq = 0.0;
    if (centers == NULL || samples == NULL || lower_matrix == NULL) {
        return;
    }
    for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
        matrix_base = (size_t)sample_index * (size_t)k;
        sample_weight = 1.0;
        if (weights != NULL) {
            sample_weight = weights[sample_index];
        }
        if (sample_weight <= 0.0) {
            for (center_index = 0u; center_index < k; ++center_index) {
                lower_matrix[matrix_base + center_index] = 0.0;
            }
            continue;
        }
        for (center_index = 0u; center_index < k; ++center_index) {
            distance_sq = sixel_kmeans_sample_center_distance_sq(samples,
                                                                 sample_index,
                                                                 centers,
                                                                 center_index);
            lower_matrix[matrix_base + center_index] = sqrt(distance_sq);
        }
    }
}

void
sixel_kmeans_compute_half_center_distances(double const *centers,
                                           unsigned int k,
                                           double *half_center_dist)
{
    unsigned int left;
    unsigned int right;
    double distance_sq;
    double min_distance;

    left = 0u;
    right = 0u;
    distance_sq = 0.0;
    min_distance = 0.0;
    if (centers == NULL || half_center_dist == NULL || k == 0u) {
        return;
    }
    if (k == 1u) {
        half_center_dist[0u] = 0.0;
        return;
    }
    for (left = 0u; left < k; ++left) {
        min_distance = DBL_MAX;
        for (right = 0u; right < k; ++right) {
            if (right == left) {
                continue;
            }
            distance_sq = sixel_kmeans_center_distance_sq(centers,
                                                          left,
                                                          right);
            if (distance_sq < min_distance) {
                min_distance = distance_sq;
            }
        }
        if (min_distance == DBL_MAX) {
            half_center_dist[left] = 0.0;
        } else {
            half_center_dist[left] = 0.5 * sqrt(min_distance);
        }
    }
}

/*
 * Build both the per-center minimum half distance and the pairwise half
 * distance matrix used by Elkan pruning.  The matrix stores
 * 0.5 * ||c_i - c_j|| for every center pair.
 */
void
sixel_kmeans_compute_half_center_distance_matrix(double const *centers,
                                                 unsigned int k,
                                                 double *half_center_dist,
                                                 double *half_center_matrix)
{
    unsigned int left;
    unsigned int right;
    size_t base;
    double distance_sq;
    double half_distance;
    double min_distance;

    left = 0u;
    right = 0u;
    base = 0u;
    distance_sq = 0.0;
    half_distance = 0.0;
    min_distance = 0.0;
    if (centers == NULL || half_center_dist == NULL
            || half_center_matrix == NULL || k == 0u) {
        return;
    }
    if (k == 1u) {
        half_center_dist[0u] = 0.0;
        half_center_matrix[0u] = 0.0;
        return;
    }
    for (left = 0u; left < k; ++left) {
        min_distance = DBL_MAX;
        base = (size_t)left * (size_t)k;
        for (right = 0u; right < k; ++right) {
            if (left == right) {
                half_center_matrix[base + right] = 0.0;
                continue;
            }
            distance_sq = sixel_kmeans_center_distance_sq(centers,
                                                          left,
                                                          right);
            half_distance = 0.5 * sqrt(distance_sq);
            half_center_matrix[base + right] = half_distance;
            if (half_distance < min_distance) {
                min_distance = half_distance;
            }
        }
        if (min_distance == DBL_MAX) {
            half_center_dist[left] = 0.0;
        } else {
            half_center_dist[left] = min_distance;
        }
    }
}

unsigned int
sixel_kmeans_yinyang_group_count(unsigned int k)
{
    unsigned int groups;

    groups = 1u;
    if (k == 0u) {
        return 1u;
    }
    groups = k / 8u;
    if (k >= 2u && groups < 2u) {
        groups = 2u;
    }
    if (groups > 32u) {
        groups = 32u;
    }
    if (groups > k) {
        groups = k;
    }
    if (groups == 0u) {
        groups = 1u;
    }
    return groups;
}

void
sixel_kmeans_build_yinyang_groups(unsigned int k,
                                  unsigned int group_count,
                                  unsigned int *group_offsets,
                                  unsigned int *center_groups)
{
    unsigned int group_index;
    unsigned int center_index;
    size_t scaled;
    unsigned int offset;

    group_index = 0u;
    center_index = 0u;
    scaled = 0u;
    offset = 0u;
    if (group_offsets == NULL || center_groups == NULL
            || k == 0u || group_count == 0u) {
        return;
    }
    for (group_index = 0u; group_index <= group_count; ++group_index) {
        scaled = (size_t)group_index * (size_t)k;
        offset = (unsigned int)(scaled / (size_t)group_count);
        if (offset > k) {
            offset = k;
        }
        group_offsets[group_index] = offset;
    }
    group_offsets[group_count] = k;
    for (group_index = 0u; group_index < group_count; ++group_index) {
        for (center_index = group_offsets[group_index];
                center_index < group_offsets[group_index + 1u];
                ++center_index) {
            center_groups[center_index] = group_index;
        }
    }
}

static void
sixel_kmeans_update_yinyang_group_bounds_from_matrix(
    unsigned int sample_count,
    unsigned int group_count,
    unsigned int const *group_offsets,
    double const *lower_matrix,
    unsigned int k,
    double *group_lower_bounds)
{
    unsigned int sample_index;
    unsigned int group_index;
    unsigned int center_index;
    size_t matrix_base;
    size_t group_base;
    double minimum;
    double bound;

    sample_index = 0u;
    group_index = 0u;
    center_index = 0u;
    matrix_base = 0u;
    group_base = 0u;
    minimum = 0.0;
    bound = 0.0;
    if (group_offsets == NULL || lower_matrix == NULL
            || group_lower_bounds == NULL || group_count == 0u
            || k == 0u) {
        return;
    }
    for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
        matrix_base = (size_t)sample_index * (size_t)k;
        group_base = (size_t)sample_index * (size_t)group_count;
        for (group_index = 0u; group_index < group_count; ++group_index) {
            minimum = DBL_MAX;
            for (center_index = group_offsets[group_index];
                    center_index < group_offsets[group_index + 1u];
                    ++center_index) {
                bound = lower_matrix[matrix_base + center_index];
                if (bound < minimum) {
                    minimum = bound;
                }
            }
            if (minimum == DBL_MAX) {
                minimum = 0.0;
            }
            group_lower_bounds[group_base + group_index] = minimum;
        }
    }
}

double
sixel_kmeans_assign_samples_full_second(double const *centers,
                                        unsigned int k,
                                        double const *samples,
                                        double const *weights,
                                        unsigned int sample_count,
                                        unsigned int *membership,
                                        double *distance_cache,
                                        double *cluster_weights,
                                        double *accum,
                                        double *upper_bounds,
                                        double *lower_bounds);

double
sixel_kmeans_assign_samples_full_elkan(double const *centers,
                                       unsigned int k,
                                       double const *samples,
                                       double const *weights,
                                       unsigned int sample_count,
                                       unsigned int *membership,
                                       double *distance_cache,
                                       double *cluster_weights,
                                       double *accum,
                                       double *upper_bounds,
                                       double *lower_bounds,
                                       double *lower_matrix)
{
    double objective;

    objective = 0.0;
    if (centers == NULL || samples == NULL || membership == NULL
            || distance_cache == NULL || cluster_weights == NULL
            || accum == NULL || upper_bounds == NULL
            || lower_bounds == NULL || lower_matrix == NULL) {
        return 0.0;
    }

    /*
     * Use the stable full-second assignment path, then populate the per-center
     * lower matrix from exact sample/center distances for the first Elkan
     * iteration.  This keeps behavior identical while avoiding PCC ICEs on the
     * previous monolithic implementation.
     */
    objective = sixel_kmeans_assign_samples_full_second(centers,
                                                        k,
                                                        samples,
                                                        weights,
                                                        sample_count,
                                                        membership,
                                                        distance_cache,
                                                        cluster_weights,
                                                        accum,
                                                        upper_bounds,
                                                        lower_bounds);
    sixel_kmeans_fill_lower_matrix_from_samples(centers,
                                                k,
                                                samples,
                                                weights,
                                                sample_count,
                                                lower_matrix);
    return objective;
}

double
sixel_kmeans_assign_samples_full_yinyang(double const *centers,
                                         unsigned int k,
                                         double const *samples,
                                         double const *weights,
                                         unsigned int sample_count,
                                         unsigned int *membership,
                                         double *distance_cache,
                                         double *cluster_weights,
                                         double *accum,
                                         double *upper_bounds,
                                         double *lower_bounds,
                                         double *lower_matrix,
                                         unsigned int group_count,
                                         unsigned int const *group_offsets,
                                         double *group_lower_bounds)
{
    double objective;

    objective = 0.0;
    objective = sixel_kmeans_assign_samples_full_elkan(centers,
                                                       k,
                                                       samples,
                                                       weights,
                                                       sample_count,
                                                       membership,
                                                       distance_cache,
                                                       cluster_weights,
                                                       accum,
                                                       upper_bounds,
                                                       lower_bounds,
                                                       lower_matrix);
    sixel_kmeans_update_yinyang_group_bounds_from_matrix(sample_count,
                                                         group_count,
                                                         group_offsets,
                                                         lower_matrix,
                                                         k,
                                                         group_lower_bounds);
    return objective;
}

double
sixel_kmeans_assign_samples_elkan(double const *centers,
                                  unsigned int k,
                                  double const *samples,
                                  double const *weights,
                                  unsigned int sample_count,
                                  unsigned int *membership,
                                  double *distance_cache,
                                  double *cluster_weights,
                                  double *accum,
                                  double *upper_bounds,
                                  double *lower_bounds,
                                  double *lower_matrix,
                                  double const *half_center_dist,
                                  double const *half_center_matrix)
{
    unsigned int sample_index;
    unsigned int center_index;
    unsigned int channel;
    unsigned int best_index;
    size_t sum_index;
    size_t matrix_base;
    size_t pair_base;
    double objective;
    double sample_weight;
    double diff;
    double distance_sq;
    double distance;
    double best_distance_sq;
    double upper;
    double lower;

    sample_index = 0u;
    center_index = 0u;
    channel = 0u;
    best_index = 0u;
    sum_index = 0u;
    matrix_base = 0u;
    pair_base = 0u;
    objective = 0.0;
    sample_weight = 0.0;
    diff = 0.0;
    distance_sq = 0.0;
    distance = 0.0;
    best_distance_sq = 0.0;
    upper = 0.0;
    lower = 0.0;
    if (centers == NULL || samples == NULL || membership == NULL
            || distance_cache == NULL || cluster_weights == NULL
            || accum == NULL || upper_bounds == NULL
            || lower_bounds == NULL || lower_matrix == NULL
            || half_center_dist == NULL || half_center_matrix == NULL) {
        return 0.0;
    }
    for (center_index = 0u; center_index < k; ++center_index) {
        cluster_weights[center_index] = 0.0;
    }
    for (sum_index = 0u; sum_index < (size_t)k * 3u; ++sum_index) {
        accum[sum_index] = 0.0;
    }
    for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
        sample_weight = 1.0;
        if (weights != NULL) {
            sample_weight = weights[sample_index];
        }
        matrix_base = (size_t)sample_index * (size_t)k;
        if (sample_weight <= 0.0) {
            membership[sample_index] = 0u;
            distance_cache[sample_index] = 0.0;
            upper_bounds[sample_index] = 0.0;
            lower_bounds[sample_index] = 0.0;
            for (center_index = 0u; center_index < k; ++center_index) {
                lower_matrix[matrix_base + center_index] = 0.0;
            }
            continue;
        }
        best_index = membership[sample_index];
        if (best_index >= k) {
            best_index = 0u;
        }
        best_distance_sq = 0.0;
        for (channel = 0u; channel < 3u; ++channel) {
            diff = samples[sample_index * 3u + channel]
                - centers[(size_t)best_index * 3u + channel];
            best_distance_sq += diff * diff;
        }
        upper = sqrt(best_distance_sq);
        upper_bounds[sample_index] = upper;
        lower_matrix[matrix_base + best_index] = upper;

        if (upper > half_center_dist[best_index]) {
            pair_base = (size_t)best_index * (size_t)k;
            for (center_index = 0u; center_index < k; ++center_index) {
                if (center_index == best_index) {
                    continue;
                }
                lower = lower_matrix[matrix_base + center_index];
                if (upper <= lower) {
                    continue;
                }
                if (upper <= half_center_matrix[pair_base + center_index]) {
                    continue;
                }
                distance_sq = 0.0;
                for (channel = 0u; channel < 3u; ++channel) {
                    diff = samples[sample_index * 3u + channel]
                        - centers[(size_t)center_index * 3u + channel];
                    distance_sq += diff * diff;
                }
                distance = sqrt(distance_sq);
                lower_matrix[matrix_base + center_index] = distance;
                if (distance_sq < best_distance_sq) {
                    best_distance_sq = distance_sq;
                    best_index = center_index;
                    upper = distance;
                    upper_bounds[sample_index] = upper;
                    lower_matrix[matrix_base + best_index] = upper;
                    pair_base = (size_t)best_index * (size_t)k;
                }
            }
        }

        membership[sample_index] = best_index;
        lower = DBL_MAX;
        for (center_index = 0u; center_index < k; ++center_index) {
            if (center_index == best_index) {
                continue;
            }
            distance = lower_matrix[matrix_base + center_index];
            if (distance < lower) {
                lower = distance;
            }
        }
        if (k < 2u || lower == DBL_MAX) {
            lower = upper;
        }
        lower_bounds[sample_index] = lower;
        distance_cache[sample_index] = best_distance_sq * sample_weight;
        objective += distance_cache[sample_index];
        cluster_weights[best_index] += sample_weight;
        for (channel = 0u; channel < 3u; ++channel) {
            accum[(size_t)best_index * 3u + channel] +=
                samples[sample_index * 3u + channel] * sample_weight;
        }
    }
    return objective;
}

double
sixel_kmeans_assign_samples_yinyang(double const *centers,
                                    unsigned int k,
                                    double const *samples,
                                    double const *weights,
                                    unsigned int sample_count,
                                    unsigned int *membership,
                                    double *distance_cache,
                                    double *cluster_weights,
                                    double *accum,
                                    double *upper_bounds,
                                    double *lower_bounds,
                                    double *lower_matrix,
                                    double const *half_center_dist,
                                    double const *half_center_matrix,
                                    unsigned int group_count,
                                    unsigned int const *group_offsets,
                                    unsigned int const *center_groups,
                                    double *group_lower_bounds)
{
    unsigned int sample_index;
    unsigned int center_index;
    unsigned int channel;
    unsigned int best_index;
    unsigned int best_group;
    unsigned int group_index;
    size_t sum_index;
    size_t matrix_base;
    size_t pair_base;
    size_t group_base;
    double objective;
    double sample_weight;
    double diff;
    double distance_sq;
    double distance;
    double best_distance_sq;
    double upper;
    double lower;
    double group_lower;

    sample_index = 0u;
    center_index = 0u;
    channel = 0u;
    best_index = 0u;
    best_group = 0u;
    group_index = 0u;
    sum_index = 0u;
    matrix_base = 0u;
    pair_base = 0u;
    group_base = 0u;
    objective = 0.0;
    sample_weight = 0.0;
    diff = 0.0;
    distance_sq = 0.0;
    distance = 0.0;
    best_distance_sq = 0.0;
    upper = 0.0;
    lower = 0.0;
    group_lower = 0.0;
    if (centers == NULL || samples == NULL || membership == NULL
            || distance_cache == NULL || cluster_weights == NULL
            || accum == NULL || upper_bounds == NULL
            || lower_bounds == NULL || lower_matrix == NULL
            || half_center_dist == NULL || half_center_matrix == NULL
            || group_offsets == NULL || center_groups == NULL
            || group_lower_bounds == NULL || group_count == 0u) {
        return 0.0;
    }
    for (center_index = 0u; center_index < k; ++center_index) {
        cluster_weights[center_index] = 0.0;
    }
    for (sum_index = 0u; sum_index < (size_t)k * 3u; ++sum_index) {
        accum[sum_index] = 0.0;
    }
    for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
        sample_weight = 1.0;
        if (weights != NULL) {
            sample_weight = weights[sample_index];
        }
        matrix_base = (size_t)sample_index * (size_t)k;
        group_base = (size_t)sample_index * (size_t)group_count;
        if (sample_weight <= 0.0) {
            membership[sample_index] = 0u;
            distance_cache[sample_index] = 0.0;
            upper_bounds[sample_index] = 0.0;
            lower_bounds[sample_index] = 0.0;
            for (center_index = 0u; center_index < k; ++center_index) {
                lower_matrix[matrix_base + center_index] = 0.0;
            }
            for (group_index = 0u; group_index < group_count; ++group_index) {
                group_lower_bounds[group_base + group_index] = 0.0;
            }
            continue;
        }
        best_index = membership[sample_index];
        if (best_index >= k) {
            best_index = 0u;
        }
        best_group = center_groups[best_index];
        best_distance_sq = 0.0;
        for (channel = 0u; channel < 3u; ++channel) {
            diff = samples[sample_index * 3u + channel]
                - centers[(size_t)best_index * 3u + channel];
            best_distance_sq += diff * diff;
        }
        upper = sqrt(best_distance_sq);
        upper_bounds[sample_index] = upper;
        lower_matrix[matrix_base + best_index] = upper;

        if (upper > half_center_dist[best_index]) {
            pair_base = (size_t)best_index * (size_t)k;
            for (group_index = 0u; group_index < group_count; ++group_index) {
                if (group_index != best_group) {
                    group_lower =
                        group_lower_bounds[group_base + group_index];
                    if (upper <= group_lower) {
                        continue;
                    }
                }
                for (center_index = group_offsets[group_index];
                        center_index < group_offsets[group_index + 1u];
                        ++center_index) {
                    if (center_index == best_index) {
                        continue;
                    }
                    lower = lower_matrix[matrix_base + center_index];
                    if (upper <= lower) {
                        continue;
                    }
                    if (upper <= half_center_matrix[
                            pair_base + center_index]) {
                        continue;
                    }
                    distance_sq = 0.0;
                    for (channel = 0u; channel < 3u; ++channel) {
                        diff = samples[sample_index * 3u + channel]
                            - centers[(size_t)center_index * 3u + channel];
                        distance_sq += diff * diff;
                    }
                    distance = sqrt(distance_sq);
                    lower_matrix[matrix_base + center_index] = distance;
                    if (distance_sq < best_distance_sq) {
                        best_distance_sq = distance_sq;
                        best_index = center_index;
                        best_group = center_groups[best_index];
                        upper = distance;
                        upper_bounds[sample_index] = upper;
                        lower_matrix[matrix_base + best_index] = upper;
                        pair_base = (size_t)best_index * (size_t)k;
                    }
                }
            }
        }

        membership[sample_index] = best_index;
        lower = DBL_MAX;
        for (center_index = 0u; center_index < k; ++center_index) {
            if (center_index == best_index) {
                continue;
            }
            distance = lower_matrix[matrix_base + center_index];
            if (distance < lower) {
                lower = distance;
            }
        }
        if (k < 2u || lower == DBL_MAX) {
            lower = upper;
        }
        lower_bounds[sample_index] = lower;
        distance_cache[sample_index] = best_distance_sq * sample_weight;
        objective += distance_cache[sample_index];
        cluster_weights[best_index] += sample_weight;
        for (channel = 0u; channel < 3u; ++channel) {
            accum[(size_t)best_index * 3u + channel] +=
                samples[sample_index * 3u + channel] * sample_weight;
        }
    }
    sixel_kmeans_update_yinyang_group_bounds_from_matrix(sample_count,
                                                         group_count,
                                                         group_offsets,
                                                         lower_matrix,
                                                         k,
                                                         group_lower_bounds);
    return objective;
}

void
sixel_kmeans_update_elkan_bounds(unsigned int sample_count,
                                 unsigned int k,
                                 unsigned int const *membership,
                                 double const *weights,
                                 double *upper_bounds,
                                 double *lower_bounds,
                                 double *lower_matrix,
                                 double const *center_shift)
{
    unsigned int sample_index;
    unsigned int center_index;
    unsigned int best_index;
    size_t matrix_base;
    double sample_weight;
    double upper;
    double lower;
    double bound;

    sample_index = 0u;
    center_index = 0u;
    best_index = 0u;
    matrix_base = 0u;
    sample_weight = 0.0;
    upper = 0.0;
    lower = 0.0;
    bound = 0.0;
    if (membership == NULL || upper_bounds == NULL || lower_bounds == NULL
            || lower_matrix == NULL || center_shift == NULL || k == 0u) {
        return;
    }
    for (sample_index = 0u; sample_index < sample_count; ++sample_index) {
        sample_weight = 1.0;
        if (weights != NULL) {
            sample_weight = weights[sample_index];
        }
        matrix_base = (size_t)sample_index * (size_t)k;
        if (sample_weight <= 0.0) {
            upper_bounds[sample_index] = 0.0;
            lower_bounds[sample_index] = 0.0;
            for (center_index = 0u; center_index < k; ++center_index) {
                lower_matrix[matrix_base + center_index] = 0.0;
            }
            continue;
        }
        best_index = membership[sample_index];
        if (best_index >= k) {
            best_index = 0u;
        }
        upper = upper_bounds[sample_index] + center_shift[best_index];
        upper_bounds[sample_index] = upper;
        lower_matrix[matrix_base + best_index] = upper;

        lower = DBL_MAX;
        for (center_index = 0u; center_index < k; ++center_index) {
            if (center_index == best_index) {
                continue;
            }
            bound = lower_matrix[matrix_base + center_index];
            if (bound > center_shift[center_index]) {
                bound -= center_shift[center_index];
            } else {
                bound = 0.0;
            }
            lower_matrix[matrix_base + center_index] = bound;
            if (bound < lower) {
                lower = bound;
            }
        }
        if (k < 2u || lower == DBL_MAX) {
            lower = upper;
        }
        lower_bounds[sample_index] = lower;
    }
}
/* EOF */
