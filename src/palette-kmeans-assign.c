/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
#include "timeline-logger.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "palette-kmeans.h"
#include "palette-kmeans-assign.h"
#include "palette.h"
#include "pixelformat.h"
#include "status.h"
#include "timer.h"

#define SIXEL_KMEANS_AABB_DEPTH 8u
#define SIXEL_KMEANS_AABB_LEAF 8u
/* Covers all finite FP32 inputs and leaves room for FP64 squared sums. */
#define SIXEL_KMEANS_AABB_COORD_LIMIT 0x1p500

typedef struct sixel_kmeans_aabb_point {
    uint32_t key;
    unsigned int index;
} sixel_kmeans_aabb_point_t;

typedef struct sixel_kmeans_aabb_node {
    double lower[3];
    double upper[3];
    unsigned int begin;
    unsigned int end;
    unsigned int child[8];
    unsigned int children;
} sixel_kmeans_aabb_node_t;

struct sixel_kmeans_aabb {
    sixel_kmeans_aabb_point_t *order;
    sixel_kmeans_aabb_node_t *nodes;
    unsigned int *candidates;
    unsigned int stride;
    unsigned int used;
    double scale;
};

typedef struct sixel_kmeans_aabb_search {
    sixel_kmeans_aabb_t *tree;
    double const *centers;
    double const *points;
    double const *weights;
    double *distances;
    unsigned int *membership;
    unsigned int k;
    double slack;
} sixel_kmeans_aabb_search_t;

static uint32_t
sixel_kmeans_aabb_spread(uint32_t x)
{
    x = (x | (x << 8)) & 0xf00fu;
    x = (x | (x << 4)) & 0xc30c3u;
    return (x | (x << 2)) & 0x249249u;
}

static int
sixel_kmeans_aabb_compare(void const *left, void const *right)
{
    sixel_kmeans_aabb_point_t const *a;
    sixel_kmeans_aabb_point_t const *b;

    a = left;
    b = right;
    return (a->key > b->key) - (a->key < b->key);
}

static unsigned int
sixel_kmeans_aabb_build(sixel_kmeans_aabb_t *tree, double const *points,
                       unsigned int begin, unsigned int end,
                       unsigned int depth)
{
    sixel_kmeans_aabb_node_t *node;
    double const *point;
    unsigned int id, i, channel, first, last, shift;
    uint32_t prefix;

    id = tree->used++;
    node = &tree->nodes[id];
    node->begin = begin;
    node->end = end;
    node->children = 0u;
    point = points + (size_t)tree->order[begin].index * 3u;
    for (channel = 0u; channel < 3u; ++channel) {
        node->lower[channel] = point[channel];
        node->upper[channel] = point[channel];
    }
    for (i = begin + 1u; i < end; ++i) {
        point = points + (size_t)tree->order[i].index * 3u;
        for (channel = 0u; channel < 3u; ++channel) {
            if (point[channel] < node->lower[channel])
                node->lower[channel] = point[channel];
            if (point[channel] > node->upper[channel])
                node->upper[channel] = point[channel];
        }
    }
    if (depth == 0u || end - begin < SIXEL_KMEANS_AABB_LEAF ||
            tree->order[begin].key == tree->order[end - 1u].key) {
        return id;
    }
    shift = (depth - 1u) * 3u;
    for (first = begin; first < end; first = last) {
        prefix = tree->order[first].key >> shift;
        for (last = first + 1u; last < end; ++last) {
            if (tree->order[last].key >> shift != prefix) break;
        }
        node->child[node->children++] = sixel_kmeans_aabb_build(
            tree, points, first, last, depth - 1u);
    }
    return id;
}

void
sixel_kmeans_aabb_free(sixel_kmeans_aabb_t *tree, sixel_allocator_t *allocator)
{
    if (tree == NULL) return;
    sixel_allocator_free(allocator, tree->candidates);
    sixel_allocator_free(allocator, tree->nodes);
    sixel_allocator_free(allocator, tree->order);
    sixel_allocator_free(allocator, tree);
}

sixel_kmeans_aabb_t *
sixel_kmeans_aabb_new(double const *points, unsigned int count, unsigned int k,
                      int byte_coordinates, sixel_allocator_t *allocator)
{
    sixel_kmeans_aabb_t *tree;
    double lower[3], upper[3], scale[3], value, span, magnitude;
    size_t index, capacity;
    unsigned int channel, i;
    uint32_t coordinate[3];

    /* At most count leaves and count/8 branches per level. Eight levels
     * bound the allocation by 2*count nodes even for sparse RGB prefixes. */
    if (count < SIXEL_KMEANS_AABB_LEAF || k < 2u ||
            (size_t)count > SIXEL_ALLOCATE_BYTES_MAX /
                            (2u * sizeof(sixel_kmeans_aabb_node_t)) ||
            (size_t)k > SIXEL_ALLOCATE_BYTES_MAX /
                        (SIXEL_KMEANS_AABB_DEPTH + 1u) / sizeof(unsigned int)) {
        return NULL;
    }
    magnitude = 1.0;
    for (channel = 0u; channel < 3u; ++channel) {
        lower[channel] = points[channel];
        upper[channel] = points[channel];
    }
    for (index = 0u; index < (size_t)count * 3u; ++index) {
        value = points[index];
        if (!(fabs(value) <= SIXEL_KMEANS_AABB_COORD_LIMIT)) return NULL;
        channel = (unsigned int)(index % 3u);
        if (value < lower[channel]) lower[channel] = value;
        if (value > upper[channel]) upper[channel] = value;
        if (fabs(value) > magnitude) magnitude = fabs(value);
    }
    for (channel = 0u; channel < 3u; ++channel) {
        if (byte_coordinates && lower[channel] >= 0.0 &&
                upper[channel] <= 255.0) {
            lower[channel] = 0.0;
            upper[channel] = 256.0;
        }
        span = upper[channel] - lower[channel];
        scale[channel] = span >= DBL_MIN * 256.0 ? 256.0 / span : 0.0;
    }
    tree = sixel_allocator_calloc(allocator, 1u, sizeof(*tree));
    if (tree == NULL) return NULL;
    capacity = (size_t)count * 2u;
    tree->order = sixel_allocator_malloc(allocator,
                                         (size_t)count * sizeof(*tree->order));
    tree->nodes = sixel_allocator_malloc(allocator,
                                         capacity * sizeof(*tree->nodes));
    tree->candidates = sixel_allocator_malloc(allocator,
        (size_t)k * (SIXEL_KMEANS_AABB_DEPTH + 1u) * sizeof(unsigned int));
    if (tree->order == NULL || tree->nodes == NULL ||
            tree->candidates == NULL) {
        sixel_kmeans_aabb_free(tree, allocator);
        return NULL;
    }
    tree->scale = magnitude;
    tree->stride = k;
    for (i = 0u; i < count; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            value = (points[(size_t)i * 3u + channel] - lower[channel])
                * scale[channel];
            coordinate[channel] = (uint32_t)(value < 255.0 ? value : 255.0);
        }
        tree->order[i].index = i;
        tree->order[i].key = (sixel_kmeans_aabb_spread(coordinate[0]) << 2)
            | (sixel_kmeans_aabb_spread(coordinate[1]) << 1)
            | sixel_kmeans_aabb_spread(coordinate[2]);
    }
    qsort(tree->order, count, sizeof(*tree->order), sixel_kmeans_aabb_compare);
    (void)sixel_kmeans_aabb_build(tree, points, 0u, count,
                                 SIXEL_KMEANS_AABB_DEPTH);
    return tree;
}

static double
sixel_kmeans_aabb_distance(double const *point, double const *center)
{
    double x, y, z, distance;

    x = point[0] - center[0];
    y = point[1] - center[1];
    z = point[2] - center[2];
    /* Keep the exhaustive kernel's multiply-add order for exact ties. */
    distance = x * x;
    distance += y * y;
    distance += z * z;
    return distance;
}

static void
sixel_kmeans_aabb_assign_leaf(
    sixel_kmeans_aabb_search_t const *search,
    sixel_kmeans_aabb_node_t const *node,
    unsigned int const *candidates, unsigned int count)
{
    unsigned int i, index, best, j, candidate;
    double const *point;
    double best_distance, distance;

    for (i = node->begin; i < node->end; ++i) {
        index = search->tree->order[i].index;
        if (search->weights != NULL && search->weights[index] <= 0.0) {
            search->membership[index] = 0u;
            search->distances[index] = 0.0;
            continue;
        }
        point = search->points + (size_t)index * 3u;
        if (count == 1u) {
            best = candidates[0];
            best_distance = sixel_kmeans_aabb_distance(
                point, search->centers + (size_t)best * 3u);
        } else {
            best = search->membership[index];
            if (best >= search->k) best = 0u;
            best_distance = sixel_kmeans_aabb_distance(
                point, search->centers + (size_t)best * 3u);
            for (j = 0u; j < count; ++j) {
                candidate = candidates[j];
                distance = sixel_kmeans_aabb_distance(
                    point, search->centers + (size_t)candidate * 3u);
                if (distance < best_distance) {
                    best_distance = distance;
                    best = candidate;
                }
            }
        }
        search->membership[index] = best;
        search->distances[index] = best_distance;
    }
}

static void
sixel_kmeans_aabb_search(sixel_kmeans_aabb_search_t const *search,
                        unsigned int id, unsigned int const *candidates,
                        unsigned int count, unsigned int depth)
{
    sixel_kmeans_aabb_node_t const *node;
    double const *witness;
    double const *rival;
    unsigned int *survivors;
    unsigned int j, channel, best, next;
    double midpoint[3], corner[3], best_distance, distance;

    node = &search->tree->nodes[id];
    if (count == 1u || node->children == 0u) {
        sixel_kmeans_aabb_assign_leaf(search, node, candidates, count);
        return;
    }
    for (channel = 0u; channel < 3u; ++channel)
        midpoint[channel] = (node->lower[channel] + node->upper[channel]) * 0.5;
    best = candidates[0];
    best_distance = DBL_MAX;
    for (j = 0u; j < count; ++j) {
        distance = sixel_kmeans_aabb_distance(midpoint,
            search->centers + (size_t)candidates[j] * 3u);
        if (distance < best_distance) {
            best_distance = distance;
            best = candidates[j];
        }
    }
    witness = search->centers + (size_t)best * 3u;
    survivors = search->tree->candidates + (size_t)depth * search->tree->stride;
    next = 0u;
    for (j = 0u; j < count; ++j) {
        rival = search->centers + (size_t)candidates[j] * 3u;
        for (channel = 0u; channel < 3u; ++channel) {
            corner[channel] = rival[channel] >= witness[channel]
                ? node->upper[channel] : node->lower[channel];
        }
        /* The minimum of the affine squared-distance difference occurs
         * at this corner. Keep ties and differences within FP64 error. */
        distance = sixel_kmeans_aabb_distance(corner, rival) -
            sixel_kmeans_aabb_distance(corner, witness);
        if (distance <= search->slack) survivors[next++] = candidates[j];
    }
    if (next == 1u) {
        sixel_kmeans_aabb_assign_leaf(search, node, survivors, next);
        return;
    }
    for (j = 0u; j < node->children; ++j) {
        sixel_kmeans_aabb_search(search, node->child[j], survivors,
                                 next, depth + 1u);
    }
}

double
sixel_kmeans_assign_samples_aabb(
    sixel_kmeans_aabb_t *tree, double const *centers, unsigned int k,
    double const *samples, double const *weights, unsigned int sample_count,
    unsigned int *membership, double *distance_cache, double *cluster_weights,
    double *accum)
{
    sixel_kmeans_aabb_search_t search;
    size_t i, base;
    unsigned int owner, channel;
    double objective, scale, value, weight;

    if (tree == NULL || k == 0u || k > tree->stride) goto exhaustive;
    scale = tree->scale;
    for (i = 0u; i < (size_t)k * 3u; ++i) {
        value = fabs(centers[i]);
        if (!(value <= SIXEL_KMEANS_AABB_COORD_LIMIT)) goto exhaustive;
        if (value > scale) scale = value;
    }
    search.tree = tree;
    search.centers = centers;
    search.points = samples;
    search.weights = weights;
    search.distances = distance_cache;
    search.membership = membership;
    search.k = k;
    /* Covers rounding in both distance evaluations at the corner and
     * the point, including contraction of multiply-adds. The coordinate
     * cap bounds every subtraction, product and sum away from overflow. */
    search.slack = 128.0 * DBL_EPSILON * scale * scale;
    for (owner = 0u; owner < k; ++owner) tree->candidates[owner] = owner;
    sixel_kmeans_aabb_search(&search, 0u, tree->candidates, k, 1u);

    /* Preserve the original point order for weighted sums, the objective
     * and residuals used by the existing empty-cluster repair. */
    memset(cluster_weights, 0, (size_t)k * sizeof(*cluster_weights));
    memset(accum, 0, (size_t)k * 3u * sizeof(*accum));
    objective = 0.0;
    for (i = 0u; i < sample_count; ++i) {
        weight = weights != NULL ? weights[i] : 1.0;
        if (weight <= 0.0) continue;
        owner = membership[i];
        base = (size_t)owner * 3u;
        distance_cache[i] *= weight;
        objective += distance_cache[i];
        cluster_weights[owner] += weight;
        for (channel = 0u; channel < 3u; ++channel)
            accum[base + channel] += samples[i * 3u + channel] * weight;
    }
    return objective;

exhaustive:
    return sixel_kmeans_assign_samples(centers, k, samples, weights,
        sample_count, membership, distance_cache, cluster_weights, accum);
}

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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
