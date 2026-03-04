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
 * Float32-aware lookup backend.  The search avoids byte quantization so
 * floating-point inputs keep their precision when resolving palette entries.
 * The CERT LUT path uses a lightweight kd-tree while other policies fall back
 * to a linear scan over the palette.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "compat_stub.h"
#include "logger.h"
#include "lookup-common.h"
#include "lookup-float32.h"
#include "pixelformat.h"
#include "status.h"

struct sixel_lookup_float32_node {
    int index;
    int left;
    int right;
    int axis;
};

static float
sixel_lookup_float32_distance(sixel_lookup_float32_t const *lut,
                              float const *sample,
                              int palette_index);

static int
sixel_lookup_float32_linear_search(sixel_lookup_float32_t const *lut,
                                   float const *sample);

static void
sixel_lookup_float32_rbc_clear(sixel_lookup_float32_t *lut);

#define SIXEL_LOOKUP_EYTZINGER_WINDOW 6

#if HAVE_BUILTIN_PREFETCH
# define SIXEL_LOOKUP_EYTZINGER_PREFETCH(base, index, count) \
    do { \
        if ((index) <= (count)) { \
            __builtin_prefetch((base) + (index), 0, 1); \
        } \
    } while (0)
#else
# define SIXEL_LOOKUP_EYTZINGER_PREFETCH(base, index, count) ((void)0)
#endif

typedef struct sixel_lookup_float32_1d_eytzinger_pair {
    float key;
    int index;
} sixel_lookup_float32_1d_eytzinger_pair_t;

#define SIXEL_LOOKUP_RBC_PIVOTS 16

static void
sixel_lookup_float32_1d_eytzinger_log_event(
    int ncolors,
    char const *event)
{
    sixel_logger_t logger;
    SIXELSTATUS status;

    sixel_logger_init(&logger);
    status = sixel_logger_prepare_env(&logger);
    if (SIXEL_FAILED(status) || !logger.active) {
        sixel_logger_close(&logger);
        return;
    }

    sixel_logger_logf(&logger,
                      "eytzinger",
                      "eytzinger",
                      event,
                      ncolors);
    sixel_logger_close(&logger);
}

static int
sixel_lookup_float32_policy_normalize(int policy)
{
    int normalized;

    normalized = policy;
    if (normalized == SIXEL_LUT_POLICY_AUTO) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    } else if (normalized != SIXEL_LUT_POLICY_5BIT
               && normalized != SIXEL_LUT_POLICY_6BIT
               && normalized != SIXEL_LUT_POLICY_CERTLUT
               && normalized != SIXEL_LUT_POLICY_EYTZINGER
               && normalized != SIXEL_LUT_POLICY_NONE
               && normalized != SIXEL_LUT_POLICY_FHEDT
               && normalized != SIXEL_LUT_POLICY_VPTREE
               && normalized != SIXEL_LUT_POLICY_RBC
               && normalized != SIXEL_LUT_POLICY_MAHALANOBIS) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    return normalized;
}

static int
sixel_lookup_fhedt_parse_flag_float32(char const *text, int default_value)
{
    long parsed;
    char *endptr;

    if (text == NULL || text[0] == '\0') {
        return default_value;
    }

    errno = 0;
    endptr = NULL;
    parsed = strtol(text, &endptr, 10);
    if (errno == ERANGE || endptr == text || *endptr != '\0') {
        return default_value;
    }

    if (parsed == 0L) {
        return 0;
    }
    if (parsed == 1L) {
        return 1;
    }

    return default_value;
}

static int
sixel_lookup_fhedt_env_resolution_float32(void)
{
    char const *env;
    long parsed;
    char *endptr;

    env = sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_RESOLUTION");
    if (env == NULL || env[0] == '\0') {
        return 64;
    }

    errno = 0;
    endptr = NULL;
    parsed = strtol(env, &endptr, 10);
    if (errno == ERANGE || endptr == env || *endptr != '\0') {
        return 64;
    }

    if (parsed == 64L || parsed == 128L || parsed == 256L) {
        return (int)parsed;
    }

    return 64;
}

static int
sixel_lookup_fhedt_env_refine_float32(void)
{
    return sixel_lookup_fhedt_parse_flag_float32(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_REFINE"),
        1);
}

static int
sixel_lookup_fhedt_env_shared_float32(void)
{
    return sixel_lookup_fhedt_parse_flag_float32(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_SHARED"),
        1);
}

static int
sixel_lookup_fhedt_env_use_dist2_float32(void)
{
    /*
     * Dist2 is disabled by default because measurements have not shown
     * consistent wins.  Enable explicitly when experimenting with boundary
     * refinement short-circuiting.
     */
    return sixel_lookup_fhedt_parse_flag_float32(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_USE_DIST2"),
        0);
}

static int
sixel_lookup_fhedt_env_use_cache_float32(void)
{
    /*
     * The cache is disabled by default because its benefit has not been
     * demonstrated.  Callers can opt in for experiments without impacting
     * parallel TLS availability checks.
     */
    return sixel_lookup_fhedt_parse_flag_float32(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_USE_CACHE"),
        0);
}

static float
sixel_lookup_float32_component(float const *palette,
                               int depth,
                               int index,
                               int axis)
{
    int clamped_axis;

    clamped_axis = axis;
    if (clamped_axis < 0) {
        clamped_axis = 0;
    } else if (clamped_axis >= SIXEL_LOOKUP_FLOAT_COMPONENTS) {
        clamped_axis = SIXEL_LOOKUP_FLOAT_COMPONENTS - 1;
    }

    return palette[index * depth + clamped_axis];
}

static void
sixel_lookup_float32_1d_eytzinger_release(sixel_lookup_float32_t *lut)
{
    sixel_lookup_float32_1d_eytzinger_t *eytz;

    if (lut == NULL) {
        return;
    }

    eytz = &lut->eytz;
    if (eytz->keys != NULL) {
        sixel_allocator_free(lut->allocator, eytz->keys);
    }
    if (eytz->palette_index != NULL) {
        sixel_allocator_free(lut->allocator, eytz->palette_index);
    }
    if (eytz->rank != NULL) {
        sixel_allocator_free(lut->allocator, eytz->rank);
    }
    if (eytz->sorted_palette_index != NULL) {
        sixel_allocator_free(lut->allocator, eytz->sorted_palette_index);
    }
    if (eytz->sorted_keys != NULL) {
        sixel_allocator_free(lut->allocator, eytz->sorted_keys);
    }

    eytz->keys = NULL;
    eytz->palette_index = NULL;
    eytz->rank = NULL;
    eytz->sorted_palette_index = NULL;
    eytz->sorted_keys = NULL;
    eytz->count = 0;
    eytz->ready = 0;
}

static float
sixel_lookup_float32_1d_eytzinger_project_palette(
    sixel_lookup_float32_t const *lut,
    int palette_index)
{
    float comp0;
    float comp1;
    float comp2;
    float key;

    comp0 = sixel_lookup_float32_component(lut->palette,
                                           lut->depth,
                                           palette_index,
                                           0);
    comp1 = sixel_lookup_float32_component(lut->palette,
                                           lut->depth,
                                           palette_index,
                                           1);
    comp2 = sixel_lookup_float32_component(lut->palette,
                                           lut->depth,
                                           palette_index,
                                           2);

    key = lut->eytz.weights[0] * comp0
        + lut->eytz.weights[1] * comp1
        + lut->eytz.weights[2] * comp2;

    return key;
}

static float
sixel_lookup_float32_1d_eytzinger_project_sample(
    sixel_lookup_float32_t const *lut,
    float const *sample)
{
    float comp0;
    float comp1;
    float comp2;
    float key;
    int depth;

    depth = lut->depth;
    comp0 = (depth > 0) ? sample[0] : 0.0f;
    comp1 = (depth > 1) ? sample[1] : 0.0f;
    comp2 = (depth > 2) ? sample[2] : 0.0f;

    key = lut->eytz.weights[0] * comp0
        + lut->eytz.weights[1] * comp1
        + lut->eytz.weights[2] * comp2;

    return key;
}

static int
sixel_lookup_float32_1d_eytzinger_compare(void const *left, void const *right)
{
    float diff;
    sixel_lookup_float32_1d_eytzinger_pair_t const *a;
    sixel_lookup_float32_1d_eytzinger_pair_t const *b;

    a = (sixel_lookup_float32_1d_eytzinger_pair_t const *)left;
    b = (sixel_lookup_float32_1d_eytzinger_pair_t const *)right;
    diff = a->key - b->key;
    if (diff < 0.0f) {
        return -1;
    }
    if (diff > 0.0f) {
        return 1;
    }
    return 0;
}

static void
sixel_lookup_float32_1d_eytzinger_fill(
    sixel_lookup_float32_1d_eytzinger_t *eytz,
    sixel_lookup_float32_1d_eytzinger_pair_t const *src,
    int count,
    int node,
    int *rank)
{
    if (node > count) {
        return;
    }

    sixel_lookup_float32_1d_eytzinger_fill(eytz, src, count, node * 2, rank);
    eytz->keys[node] = src[*rank].key;
    eytz->palette_index[node] = src[*rank].index;
    eytz->rank[node] = *rank;
    (*rank)++;
    sixel_lookup_float32_1d_eytzinger_fill(eytz,
                                           src,
                                           count,
                                           node * 2 + 1,
                                           rank);
}

static SIXELSTATUS
sixel_lookup_float32_configure_1d_eytzinger(sixel_lookup_float32_t *lut)
{
    SIXELSTATUS status;
    sixel_lookup_float32_1d_eytzinger_t *eytz;
    sixel_lookup_float32_1d_eytzinger_pair_t *pairs;
    size_t bytes;
    int count;
    int index;
    int rank;
    float weight_sum;
    float weight_norm;

    status = SIXEL_BAD_ALLOCATION;
    eytz = &lut->eytz;
    pairs = NULL;
    weight_sum = 0.0f;
    weight_norm = 1.0f;

    sixel_lookup_float32_1d_eytzinger_release(lut);
    eytz->ready = 0;
    eytz->count = 0;
    eytz->window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    /*
     * Use a unit vector in the weighted space so key deltas bound the
     * weighted distance. This keeps the neighbor scan consistent with the
     * distance function.
     */
    weight_sum = lut->weights[0] + lut->weights[1] + lut->weights[2];
    if (weight_sum > 0.0f) {
        weight_norm = sqrtf(weight_sum);
    }
    eytz->weights[0] = sqrtf(lut->weights[0]) / weight_norm;
    eytz->weights[1] = sqrtf(lut->weights[1]) / weight_norm;
    eytz->weights[2] = sqrtf(lut->weights[2]) / weight_norm;

    count = lut->ncolors;
    if (count <= 0 || lut->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    bytes = (size_t)count * sizeof(*pairs);
    pairs = (sixel_lookup_float32_1d_eytzinger_pair_t *)sixel_allocator_malloc(
        lut->allocator,
        bytes);
    if (pairs == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_float32_configure: Eytzinger allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    /*
     * Eytzinger preparation steps:
     *   1) Project palette entries onto a 1D key.
     *   2) Sort the (key, palette index) pairs by key.
     *   3) Reorder into an implicit binary tree while keeping ranks.
     */
    for (index = 0; index < count; ++index) {
        pairs[index].index = index;
        pairs[index].key = sixel_lookup_float32_1d_eytzinger_project_palette(
            lut,
            index);
    }

    qsort(pairs, (size_t)count, sizeof(*pairs),
          sixel_lookup_float32_1d_eytzinger_compare);

    sixel_lookup_float32_1d_eytzinger_log_event(count, "builder-start");

    eytz->keys = (float *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)(count + 1) * sizeof(float));
    eytz->palette_index = (int *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)(count + 1) * sizeof(int));
    eytz->rank = (int *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)(count + 1) * sizeof(int));
    eytz->sorted_palette_index = (int *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)count * sizeof(int));
    eytz->sorted_keys = (float *)sixel_allocator_malloc(
        lut->allocator,
        (size_t)count * sizeof(float));

    if (eytz->keys == NULL || eytz->palette_index == NULL
            || eytz->rank == NULL || eytz->sorted_palette_index == NULL
            || eytz->sorted_keys == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_float32_configure: Eytzinger arrays missing.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }

    for (index = 0; index < count; ++index) {
        eytz->sorted_palette_index[index] = pairs[index].index;
        eytz->sorted_keys[index] = pairs[index].key;
    }

    /*
     * The Eytzinger layout stores a sorted array in an implicit binary tree.
     * We keep the original rank so the lookup can scan neighbours around the
     * lower-bound node.
     */
    rank = 0;
    sixel_lookup_float32_1d_eytzinger_fill(eytz, pairs, count, 1, &rank);

    eytz->count = count;
    eytz->ready = 1;
    sixel_lookup_float32_1d_eytzinger_log_event(count, "builder-end");
    status = SIXEL_OK;

error:
    if (pairs != NULL) {
        sixel_allocator_free(lut->allocator, pairs);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_float32_1d_eytzinger_release(lut);
    }
    return status;
}

static int
sixel_lookup_float32_1d_eytzinger_lower_bound(
    sixel_lookup_float32_1d_eytzinger_t const *eytz,
    float key)
{
    int node;
    int candidate;
    int count;
    int next;
    int prefetch;

    count = eytz->count;
    node = 1;
    candidate = 0;
    while (node <= count) {
        if (key <= eytz->keys[node]) {
            candidate = node;
            next = node * 2;
        } else {
            next = node * 2 + 1;
        }
        node = next;
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->keys, node, count);
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->keys, node + 1, count);
        /*
         * Prefetch ranks to reduce the latency of the post-loop lookup.
         */
        prefetch = node;
        if (prefetch <= count) {
            SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, prefetch, count);
        }
        prefetch = node + 1;
        if (prefetch <= count) {
            SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, prefetch, count);
        }
    }
    return candidate;
}

static int
sixel_lookup_float32_lookup_1d_eytzinger(sixel_lookup_float32_t *lut,
                                      float const *sample)
{
    sixel_lookup_float32_1d_eytzinger_t const *eytz;
    float key;
    int count;
    int node;
    int rank;
    int start;
    int end;
    int offset_left;
    int offset_right;
    int stop_left;
    int stop_right;
    int palette_index;
    int best_index;
    float best_distance;
    float distance;
    float key_diff;
    float key_diff_sq;
    int prefetch;
    int prefetch_limit;

    eytz = &lut->eytz;
    if (eytz->ready == 0 || eytz->count <= 0) {
        return 0;
    }

    key = sixel_lookup_float32_1d_eytzinger_project_sample(lut, sample);
    node = sixel_lookup_float32_1d_eytzinger_lower_bound(eytz, key);
    count = eytz->count;
    if (node == 0) {
        rank = count - 1;
    } else {
        rank = eytz->rank[node];
    }
    start = 0;
    end = count - 1;

    best_index = eytz->sorted_palette_index[rank];
    best_distance = sixel_lookup_float32_distance(lut,
                                                  sample,
                                                  best_index);
    offset_left = rank - 1;
    offset_right = rank + 1;
    stop_left = 0;
    stop_right = 0;
    prefetch_limit = count - 1;
    /*
     * The key is a projection onto a unit vector in weighted space, so the
     * squared key delta is a lower bound for the weighted distance.  This lets
     * us expand outward until both sides cannot beat the current best.
     */
    while (stop_left == 0 || stop_right == 0) {
        if (stop_left == 0) {
            if (offset_left < start) {
                stop_left = 1;
            } else {
                prefetch = offset_left - 1;
                if (prefetch >= 0) {
                    SIXEL_LOOKUP_EYTZINGER_PREFETCH(
                        eytz->sorted_keys,
                        prefetch,
                        prefetch_limit);
                    SIXEL_LOOKUP_EYTZINGER_PREFETCH(
                        eytz->sorted_palette_index,
                        prefetch,
                        prefetch_limit);
                }
                key_diff = key - eytz->sorted_keys[offset_left];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance) {
                    stop_left = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_left];
                    distance = sixel_lookup_float32_distance(lut,
                                                             sample,
                                                             palette_index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = palette_index;
                    }
                    offset_left--;
                }
            }
        }
        if (stop_right == 0) {
            if (offset_right > end) {
                stop_right = 1;
            } else {
                prefetch = offset_right + 1;
                if (prefetch <= prefetch_limit) {
                    SIXEL_LOOKUP_EYTZINGER_PREFETCH(
                        eytz->sorted_keys,
                        prefetch,
                        prefetch_limit);
                    SIXEL_LOOKUP_EYTZINGER_PREFETCH(
                        eytz->sorted_palette_index,
                        prefetch,
                        prefetch_limit);
                }
                key_diff = key - eytz->sorted_keys[offset_right];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance) {
                    stop_right = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_right];
                    distance = sixel_lookup_float32_distance(lut,
                                                             sample,
                                                             palette_index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = palette_index;
                    }
                    offset_right++;
                }
            }
        }
    }

    return best_index;
}

static void
sixel_lookup_float32_sort_indices(float const *palette,
                                  int depth,
                                  int *indices,
                                  int count,
                                  int axis)
{
    int i;
    int j;
    int key;
    float key_value;
    float current;

    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_lookup_float32_component(palette,
                                                   depth,
                                                   key,
                                                   axis);
        j = i - 1;
        while (j >= 0) {
            current = sixel_lookup_float32_component(palette,
                                                     depth,
                                                     indices[j],
                                                     axis);
            if (current <= key_value) {
                break;
            }
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int
sixel_lookup_float32_build_kdtree(sixel_lookup_float32_t *lut,
                                  int *indices,
                                  int count,
                                  int depth)
{
    int axis;
    int median;
    int node_index;

    if (count <= 0) {
        return -1;
    }

    axis = depth % SIXEL_LOOKUP_FLOAT_COMPONENTS;
    sixel_lookup_float32_sort_indices(lut->palette,
                                      lut->depth,
                                      indices,
                                      count,
                                      axis);
    median = count / 2;
    node_index = lut->kdnodes_count;
    if (node_index >= lut->ncolors) {
        return -1;
    }

    lut->kdnodes_count++;
    lut->kdnodes[node_index].index = indices[median];
    lut->kdnodes[node_index].axis = axis;
    lut->kdnodes[node_index].left =
        sixel_lookup_float32_build_kdtree(lut,
                                          indices,
                                          median,
                                          depth + 1);
    lut->kdnodes[node_index].right =
        sixel_lookup_float32_build_kdtree(lut,
                                          indices + median + 1,
                                          count - median - 1,
                                          depth + 1);

    return node_index;
}

static float
sixel_lookup_float32_distance(sixel_lookup_float32_t const *lut,
                              float const *sample,
                              int palette_index)
{
    float diff;
    float distance;
    int component;

    distance = 0.0f;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        diff = sample[component]
             - sixel_lookup_float32_component(lut->palette,
                                              lut->depth,
                                              palette_index,
                                              component);
        diff *= diff;
        diff *= lut->weights[component];
        distance += diff;
    }

    return distance;
}

static float
sixel_lookup_float32_weighted_component(
    sixel_lookup_float32_t const *lut,
    float value,
    int component)
{
    float weight;

    weight = lut->weights[component];
    if (weight <= 0.0f) {
        return 0.0f;
    }

    return value * sqrtf(weight);
}

static float
sixel_lookup_float32_quadratic3(float const *m,
                                float const *v)
{
    float x0;
    float x1;
    float x2;

    x0 = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
    x1 = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
    x2 = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
    return v[0] * x0 + v[1] * x1 + v[2] * x2;
}

static int
sixel_lookup_float32_inverse3(float const *src,
                              float *dst)
{
    float det;

    det = src[0] * (src[4] * src[8] - src[5] * src[7])
        - src[1] * (src[3] * src[8] - src[5] * src[6])
        + src[2] * (src[3] * src[7] - src[4] * src[6]);
    if (fabsf(det) < 1.0e-12f) {
        return 0;
    }

    dst[0] = (src[4] * src[8] - src[5] * src[7]) / det;
    dst[1] = (src[2] * src[7] - src[1] * src[8]) / det;
    dst[2] = (src[1] * src[5] - src[2] * src[4]) / det;
    dst[3] = (src[5] * src[6] - src[3] * src[8]) / det;
    dst[4] = (src[0] * src[8] - src[2] * src[6]) / det;
    dst[5] = (src[2] * src[3] - src[0] * src[5]) / det;
    dst[6] = (src[3] * src[7] - src[4] * src[6]) / det;
    dst[7] = (src[1] * src[6] - src[0] * src[7]) / det;
    dst[8] = (src[0] * src[4] - src[1] * src[3]) / det;
    return 1;
}

static SIXELSTATUS
sixel_lookup_float32_configure_rbc(sixel_lookup_float32_t *lut,
                                   int mahalanobis)
{
    int pivots;
    int i;
    int j;
    int k;
    int c;
    int best_pivot;
    float best_distance;
    float distance;
    float d0;
    float d1;
    float d2;
    float cov[9];
    float mean0;
    float mean1;
    float mean2;

    sixel_lookup_float32_rbc_clear(lut);
    pivots = lut->ncolors;
    if (pivots > SIXEL_LOOKUP_RBC_PIVOTS) {
        pivots = SIXEL_LOOKUP_RBC_PIVOTS;
    }
    if (pivots <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut->rbc.pivots = (int *)calloc((size_t)pivots, sizeof(int));
    lut->rbc.radius = (float *)calloc((size_t)pivots, sizeof(float));
    lut->rbc.member_offset = (int *)calloc((size_t)pivots + 1u, sizeof(int));
    lut->rbc.member_index = (int *)calloc((size_t)lut->ncolors, sizeof(int));
    lut->rbc.mean = (float *)calloc((size_t)pivots * 3u, sizeof(float));
    lut->rbc.inv_cov = (float *)calloc((size_t)pivots * 9u, sizeof(float));
    if (lut->rbc.pivots == NULL || lut->rbc.radius == NULL
        || lut->rbc.member_offset == NULL || lut->rbc.member_index == NULL
        || lut->rbc.mean == NULL || lut->rbc.inv_cov == NULL) {
        sixel_lookup_float32_rbc_clear(lut);
        return SIXEL_BAD_ALLOCATION;
    }

    for (j = 0; j < pivots; ++j) {
        lut->rbc.pivots[j] = (j * lut->ncolors) / pivots;
    }
    for (i = 0; i < lut->ncolors; ++i) {
        best_pivot = 0;
        best_distance = FLT_MAX;
        for (j = 0; j < pivots; ++j) {
            distance = sixel_lookup_float32_distance(lut,
                                                     lut->palette
                                                     + (size_t)i * 3u,
                                                     lut->rbc.pivots[j]);
            if (distance < best_distance) {
                best_distance = distance;
                best_pivot = j;
            }
        }
        if (best_pivot < 0 || best_pivot >= pivots) {
            sixel_lookup_float32_rbc_clear(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        lut->rbc.member_offset[best_pivot + 1]++;
    }
    for (j = 0; j < pivots; ++j) {
        lut->rbc.member_offset[j + 1] += lut->rbc.member_offset[j];
    }
    for (j = 0; j < pivots; ++j) {
        lut->rbc.radius[j] = 0.0f;
    }
    for (i = 0; i < lut->ncolors; ++i) {
        best_pivot = 0;
        best_distance = FLT_MAX;
        for (j = 0; j < pivots; ++j) {
            distance = sixel_lookup_float32_distance(lut,
                                                     lut->palette
                                                     + (size_t)i * 3u,
                                                     lut->rbc.pivots[j]);
            if (distance < best_distance) {
                best_distance = distance;
                best_pivot = j;
            }
        }
        if (best_pivot < 0 || best_pivot >= pivots) {
            sixel_lookup_float32_rbc_clear(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        c = lut->rbc.member_offset[best_pivot]++;
        if (c < 0 || c >= lut->ncolors) {
            sixel_lookup_float32_rbc_clear(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        lut->rbc.member_index[c] = i;
        distance = sqrtf(best_distance);
        if (distance > lut->rbc.radius[best_pivot]) {
            lut->rbc.radius[best_pivot] = distance;
        }
    }
    for (j = pivots; j > 0; --j) {
        lut->rbc.member_offset[j] = lut->rbc.member_offset[j - 1];
    }
    lut->rbc.member_offset[0] = 0;

    for (j = 0; j < pivots; ++j) {
        i = lut->rbc.member_offset[j + 1] - lut->rbc.member_offset[j];
        mean0 = 0.0f;
        mean1 = 0.0f;
        mean2 = 0.0f;
        lut->rbc.inv_cov[j * 9 + 0] = 1.0f;
        lut->rbc.inv_cov[j * 9 + 4] = 1.0f;
        lut->rbc.inv_cov[j * 9 + 8] = 1.0f;
        if (i <= 0) {
            continue;
        }
        for (k = lut->rbc.member_offset[j];
            k < lut->rbc.member_offset[j + 1];
             ++k) {
            c = lut->rbc.member_index[k];
            if (c < 0 || c >= lut->ncolors) {
                continue;
            }
            mean0 += lut->palette[c * 3 + 0];
            mean1 += lut->palette[c * 3 + 1];
            mean2 += lut->palette[c * 3 + 2];
        }
        mean0 /= (float)i;
        mean1 /= (float)i;
        mean2 /= (float)i;
        if (mahalanobis) {
            /*
             * Mahalanobis pruning must live in the same weighted metric
             * space as the nearest-neighbour objective.
             */
            lut->rbc.mean[j * 3 + 0] =
                sixel_lookup_float32_weighted_component(lut, mean0, 0);
            lut->rbc.mean[j * 3 + 1] =
                sixel_lookup_float32_weighted_component(lut, mean1, 1);
            lut->rbc.mean[j * 3 + 2] =
                sixel_lookup_float32_weighted_component(lut, mean2, 2);
        } else {
            lut->rbc.mean[j * 3 + 0] = mean0;
            lut->rbc.mean[j * 3 + 1] = mean1;
            lut->rbc.mean[j * 3 + 2] = mean2;
        }
        if (!mahalanobis || i < 2) {
            continue;
        }
        memset(cov, 0, sizeof(cov));
        for (k = lut->rbc.member_offset[j];
             k < lut->rbc.member_offset[j + 1];
             ++k) {
            c = lut->rbc.member_index[k];
            d0 = sixel_lookup_float32_weighted_component(
                lut,
                lut->palette[c * 3 + 0],
                0) - lut->rbc.mean[j * 3 + 0];
            d1 = sixel_lookup_float32_weighted_component(
                lut,
                lut->palette[c * 3 + 1],
                1) - lut->rbc.mean[j * 3 + 1];
            d2 = sixel_lookup_float32_weighted_component(
                lut,
                lut->palette[c * 3 + 2],
                2) - lut->rbc.mean[j * 3 + 2];
            cov[0] += d0 * d0;
            cov[1] += d0 * d1;
            cov[2] += d0 * d2;
            cov[4] += d1 * d1;
            cov[5] += d1 * d2;
            cov[8] += d2 * d2;
        }
        cov[0] /= (float)(i - 1);
        cov[1] /= (float)(i - 1);
        cov[2] /= (float)(i - 1);
        cov[3] = cov[1];
        cov[4] /= (float)(i - 1);
        cov[5] /= (float)(i - 1);
        cov[6] = cov[2];
        cov[7] = cov[5];
        cov[8] /= (float)(i - 1);
        cov[0] += 1.0e-6f;
        cov[4] += 1.0e-6f;
        cov[8] += 1.0e-6f;
        if (!sixel_lookup_float32_inverse3(cov,
                                           lut->rbc.inv_cov + j * 9)) {
            lut->rbc.inv_cov[j * 9 + 0] = 1.0f;
            lut->rbc.inv_cov[j * 9 + 1] = 0.0f;
            lut->rbc.inv_cov[j * 9 + 2] = 0.0f;
            lut->rbc.inv_cov[j * 9 + 3] = 0.0f;
            lut->rbc.inv_cov[j * 9 + 4] = 1.0f;
            lut->rbc.inv_cov[j * 9 + 5] = 0.0f;
            lut->rbc.inv_cov[j * 9 + 6] = 0.0f;
            lut->rbc.inv_cov[j * 9 + 7] = 0.0f;
            lut->rbc.inv_cov[j * 9 + 8] = 1.0f;
        }
    }

    lut->rbc.pivot_count = pivots;
    lut->rbc.ready = 1;
    return SIXEL_OK;
}

static int
sixel_lookup_float32_rbc_search(sixel_lookup_float32_t const *lut,
                                float const *sample,
                                int mahalanobis)
{
    int j;
    int k;
    int start;
    int end;
    int idx;
    int best_index;
    float best2;
    float dist2;
    float lb2;
    float lb;
    float diff[3];

    if (lut->rbc.ready == 0) {
        return sixel_lookup_float32_linear_search(lut, sample);
    }
    best2 = FLT_MAX;
    best_index = 0;
    for (j = 0; j < lut->rbc.pivot_count; ++j) {
        if (mahalanobis) {
            diff[0] = sixel_lookup_float32_weighted_component(
                lut,
                sample[0],
                0) - lut->rbc.mean[j * 3 + 0];
            diff[1] = sixel_lookup_float32_weighted_component(
                lut,
                sample[1],
                1) - lut->rbc.mean[j * 3 + 1];
            diff[2] = sixel_lookup_float32_weighted_component(
                lut,
                sample[2],
                2) - lut->rbc.mean[j * 3 + 2];
            lb2 = sixel_lookup_float32_quadratic3(lut->rbc.inv_cov + j * 9,
                                                  diff);
        } else {
            dist2 = sixel_lookup_float32_distance(lut,
                                                  sample,
                                                  lut->rbc.pivots[j]);
            lb = sqrtf(dist2) - lut->rbc.radius[j];
            if (lb < 0.0f) {
                lb = 0.0f;
            }
            lb2 = lb * lb;
        }
        /*
         * Spherical RBC uses a strict triangle-inequality lower bound, so
         * cluster-level pruning is safe. Mahalanobis scores are used only for
         * ordering because they are not strict lower bounds for this metric.
         */
        if (!mahalanobis && lb2 >= best2) {
            continue;
        }
        start = lut->rbc.member_offset[j];
        end = lut->rbc.member_offset[j + 1];
        for (k = start; k < end; ++k) {
            idx = lut->rbc.member_index[k];
            dist2 = sixel_lookup_float32_distance(lut, sample, idx);
            if (dist2 < best2) {
                best2 = dist2;
                best_index = idx;
            }
        }
    }
    return best_index;
}

static void
sixel_lookup_float32_search_kdtree(sixel_lookup_float32_t const *lut,
                                   int node_index,
                                   float const *sample,
                                   int *best_index,
                                   float *best_distance)
{
    sixel_lookup_float32_node_t const *node;
    float pivot;
    float diff;
    float distance;
    float plane_distance;
    int next;
    int other;

    if (node_index < 0) {
        return;
    }

    node = &lut->kdnodes[node_index];
    pivot = sixel_lookup_float32_component(lut->palette,
                                           lut->depth,
                                           node->index,
                                           node->axis);
    diff = sample[node->axis] - pivot;
    next = node->left;
    other = node->right;
    if (diff > 0.0f) {
        next = node->right;
        other = node->left;
    }

    sixel_lookup_float32_search_kdtree(lut,
                                       next,
                                       sample,
                                       best_index,
                                       best_distance);

    distance = sixel_lookup_float32_distance(lut, sample, node->index);
    if (distance < *best_distance) {
        *best_distance = distance;
        *best_index = node->index;
    }

    plane_distance = diff * diff * lut->weights[node->axis];
    if (plane_distance < *best_distance) {
        sixel_lookup_float32_search_kdtree(lut,
                                           other,
                                           sample,
                                           best_index,
                                           best_distance);
    }
}

static int
sixel_lookup_float32_linear_search(sixel_lookup_float32_t const *lut,
                                   float const *sample)
{
    float best;
    float distance;
    int best_index;
    int index;

    best = FLT_MAX;
    best_index = 0;
    for (index = 0; index < lut->ncolors; ++index) {
        distance = sixel_lookup_float32_distance(lut, sample, index);
        if (distance < best) {
            best = distance;
            best_index = index;
        }
    }

    return best_index;
}

static void
sixel_lookup_float32_rbc_clear(sixel_lookup_float32_t *lut)
{
    free(lut->rbc.pivots);
    free(lut->rbc.radius);
    free(lut->rbc.member_offset);
    free(lut->rbc.member_index);
    free(lut->rbc.mean);
    free(lut->rbc.inv_cov);
    lut->rbc.pivots = NULL;
    lut->rbc.radius = NULL;
    lut->rbc.member_offset = NULL;
    lut->rbc.member_index = NULL;
    lut->rbc.mean = NULL;
    lut->rbc.inv_cov = NULL;
    lut->rbc.pivot_count = 0;
    lut->rbc.ready = 0;
}

void
sixel_lookup_float32_init(sixel_lookup_float32_t *lut,
                          sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(sixel_lookup_float32_t));
    lut->policy = SIXEL_LUT_POLICY_6BIT;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
    lut->weights[0] = 1.0f;
    lut->weights[1] = 1.0f;
    lut->weights[2] = 1.0f;
    lut->palette = NULL;
    lut->kdnodes = NULL;
    lut->kdtree_root = -1;
    lut->kdnodes_count = 0;
    lut->allocator = allocator;
    lut->fhedt = NULL;
    lut->fhedt_ready = 0;
    lut->vptree = NULL;
    lut->vptree_ready = 0;
    lut->eytz.count = 0;
    lut->eytz.ready = 0;
    lut->eytz.keys = NULL;
    lut->eytz.palette_index = NULL;
    lut->eytz.rank = NULL;
    lut->eytz.sorted_palette_index = NULL;
    lut->eytz.sorted_keys = NULL;
    lut->eytz.window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    lut->eytz.weights[0] = 1.0f;
    lut->eytz.weights[1] = 1.0f;
    lut->eytz.weights[2] = 1.0f;
    lut->rbc.pivot_count = 0;
    lut->rbc.pivots = NULL;
    lut->rbc.radius = NULL;
    lut->rbc.member_offset = NULL;
    lut->rbc.member_index = NULL;
    lut->rbc.mean = NULL;
    lut->rbc.inv_cov = NULL;
    lut->rbc.ready = 0;
    (void)sixel_lookup_fhedt_float32_create(allocator, &lut->fhedt);
    (void)sixel_lookup_vptree_float32_create(allocator, &lut->vptree);
}

static void
sixel_lookup_float32_release_palette(sixel_lookup_float32_t *lut)
{
    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
}

static void
sixel_lookup_float32_release_kdtree(sixel_lookup_float32_t *lut)
{
    free(lut->kdnodes);
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

void
sixel_lookup_float32_clear(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_float32_release_palette(lut);
    sixel_lookup_float32_release_kdtree(lut);
    sixel_lookup_float32_rbc_clear(lut);
    sixel_lookup_float32_1d_eytzinger_release(lut);
    if (lut->fhedt != NULL) {
        sixel_lookup_fhedt_float32_unref(lut->fhedt);
        lut->fhedt = NULL;
    }
    lut->fhedt_ready = 0;
    if (lut->vptree != NULL) {
        sixel_lookup_vptree_float32_unref(lut->vptree);
        lut->vptree = NULL;
    }
    lut->vptree_ready = 0;
    lut->ncolors = 0;
    lut->depth = 0;
}

void
sixel_lookup_float32_finalize(sixel_lookup_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_float32_clear(lut);
    lut->allocator = NULL;
}

static SIXELSTATUS
sixel_lookup_float32_prepare_palette(sixel_lookup_float32_t *lut,
                                     unsigned char const *palette,
                                     float const *palette_float,
                                     int float_depth,
                                     int pixelformat)
{
    size_t total;
    size_t float_payload;
    int index;
    int component;
    float *cursor;
    float const *float_cursor;
    int expected_float_depth;

    total = (size_t)lut->ncolors * (size_t)lut->depth;
    float_payload = 0U;
    sixel_lookup_float32_release_palette(lut);
    lut->palette = (float *)sixel_allocator_malloc(lut->allocator,
                                                   total * sizeof(float));
    if (lut->palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_float32_prepare_palette: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    /*
     * If a float palette is supplied it already matches the working color
     * space, so copy it verbatim.  Otherwise, convert the RGB888 bytes into
     * float32 components using the pixelformat conversion helper.
     */
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_float32_prepare_palette: "
                "float palette depth mismatch.");
            sixel_lookup_float32_release_palette(lut);
            return SIXEL_BAD_ARGUMENT;
        }
        float_payload = (size_t)lut->ncolors * (size_t)expected_float_depth;
        if (float_payload > 0U) {
            memcpy(cursor, float_cursor, float_payload);
            return SIXEL_OK;
        }
    }

    for (index = 0; index < lut->ncolors; ++index) {
        for (component = 0; component < lut->depth; ++component) {
            *cursor = sixel_pixelformat_byte_to_float(pixelformat,
                                                     component,
                                                     palette[index
                                                         * lut->depth
                                                         + component]);
            ++cursor;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_float32_prepare_kdtree(sixel_lookup_float32_t *lut)
{
    int *indices;
    SIXELSTATUS status;
    int i;

    lut->kdnodes = (sixel_lookup_float32_node_t *)
        calloc((size_t)lut->ncolors, sizeof(sixel_lookup_float32_node_t));
    if (lut->kdnodes == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        free(lut->kdnodes);
        lut->kdnodes = NULL;
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_lookup_float32_build_kdtree(lut,
                                                          indices,
                                                          lut->ncolors,
                                                          0);
    status = SIXEL_OK;
    if (lut->kdtree_root < 0) {
        status = SIXEL_BAD_ALLOCATION;
    }

    free(indices);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_float32_release_kdtree(lut);
    }

    return status;
}

static SIXELSTATUS
sixel_lookup_float32_configure_fhedt(sixel_lookup_float32_t *lut,
                                    int pixelformat)
{
    SIXELSTATUS status;
    int resolution;
    int refine;
    int shared_flag;
    int use_dist2;
    int use_cache;
    uint32_t signature;

    resolution = sixel_lookup_fhedt_env_resolution_float32();
    refine = sixel_lookup_fhedt_env_refine_float32();
    shared_flag = sixel_lookup_fhedt_env_shared_float32();
    use_dist2 = sixel_lookup_fhedt_env_use_dist2_float32();
    use_cache = sixel_lookup_fhedt_env_use_cache_float32();

    if (lut->fhedt == NULL) {
        status = sixel_lookup_fhedt_float32_create(lut->allocator, &lut->fhedt);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_float32_configure: FHEDT handle allocation "
                "failed.");
            return status;
        }
    }

    signature = sixel_lookup_fhedt_float32_signature(lut->palette,
                                                    lut->ncolors,
                                                    resolution,
                                                    refine,
                                                    lut->weights[0],
                                                    lut->weights[1],
                                                    lut->weights[2],
                                                    lut->depth,
                                                    pixelformat);

    status = sixel_lookup_fhedt_float32_configure(lut->fhedt,
                                                 lut->palette,
                                                 lut->ncolors,
                                                 resolution,
                                                 refine,
                                                 use_dist2,
                                                 use_cache,
                                                 shared_flag,
                                                 lut->weights[0],
                                                 lut->weights[1],
                                                 lut->weights[2],
                                                 pixelformat);
    if (SIXEL_FAILED(status)) {
        lut->fhedt_ready = 0;
        return status;
    }

    sixel_lookup_fhedt_float32_shared_set_signature(lut->fhedt->shared,
                                                   signature);
    lut->fhedt_ready = 1;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_float32_configure(sixel_lookup_float32_t *lut,
                               unsigned char const *palette,
                               float const *palette_float,
                               int depth,
                               int float_depth,
                               int ncolors,
                               int complexion,
                               int wcomp1,
                               int wcomp2,
                               int wcomp3,
                               int policy,
                               int pixelformat)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    float range;
    float scale;
    int component;

    base_weights[0] = 0.0f;
    base_weights[1] = 0.0f;
    base_weights[2] = 0.0f;
    range = 1.0f;
    scale = 1.0f;
    component = 0;

    if (lut == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth <= 0 || ncolors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_float32_clear(lut);
    lut->policy = sixel_lookup_float32_policy_normalize(policy);
    lut->depth = depth;
    lut->ncolors = ncolors;
    lut->complexion = complexion;
    /*
     * Apply per-component weighting.  The first component inherits the
     * complexion factor to preserve existing luminance-driven defaults while
     * keeping the parameter names agnostic to the color space.
     */
    base_weights[0] = (float)wcomp1 * (float)complexion;
    base_weights[1] = (float)wcomp2;
    base_weights[2] = (float)wcomp3;
    /*
     * Normalize weights by the expected float32 channel ranges so L/ab
     * scale asymmetry does not skew the distance function.
     */
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_float32_prepare_palette(lut,
                                                  palette,
                                                  palette_float,
                                                  float_depth,
                                                  pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    lut->vptree_ready = 0;
    if (lut->policy == SIXEL_LUT_POLICY_RBC
        || lut->policy == SIXEL_LUT_POLICY_MAHALANOBIS) {
        status = sixel_lookup_float32_configure_rbc(
            lut,
            lut->policy == SIXEL_LUT_POLICY_MAHALANOBIS);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_float32_configure: RBC failed; "
                "falling back to CERTLUT.");
            lut->policy = SIXEL_LUT_POLICY_CERTLUT;
        } else {
            return SIXEL_OK;
        }
    }
    if (lut->policy == SIXEL_LUT_POLICY_FHEDT) {
        status = sixel_lookup_float32_configure_fhedt(lut, pixelformat);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_float32_configure: FHEDT failed; "
                "falling back to CERTLUT.");
            lut->policy = SIXEL_LUT_POLICY_CERTLUT;
        } else {
            return SIXEL_OK;
        }
    } else if (lut->policy == SIXEL_LUT_POLICY_VPTREE) {
        if (lut->vptree == NULL) {
            status = sixel_lookup_vptree_float32_create(lut->allocator,
                                                        &lut->vptree);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_lookup_float32_configure: VP-tree allocation "
                    "failed.");
                lut->policy = SIXEL_LUT_POLICY_CERTLUT;
            }
        }
        if (lut->policy == SIXEL_LUT_POLICY_VPTREE) {
            status = sixel_lookup_vptree_float32_configure(lut->vptree,
                                                           lut->palette,
                                                           lut->ncolors,
                                                           lut->depth,
                                                           lut->weights);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "sixel_lookup_float32_configure: VP-tree failed; "
                    "falling back to CERTLUT.");
                lut->policy = SIXEL_LUT_POLICY_CERTLUT;
            } else {
                lut->vptree_ready = 1;
                return SIXEL_OK;
            }
        }
    } else if (lut->policy == SIXEL_LUT_POLICY_EYTZINGER) {
        status = sixel_lookup_float32_configure_1d_eytzinger(lut);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_float32_configure: Eytzinger failed; "
                "falling back to CERTLUT.");
            lut->policy = SIXEL_LUT_POLICY_CERTLUT;
        } else {
            return SIXEL_OK;
        }
    } else {
        lut->fhedt_ready = 0;
        lut->vptree_ready = 0;
    }

    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        status = sixel_lookup_float32_prepare_kdtree(lut);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

int
sixel_lookup_float32_map_pixel(sixel_lookup_float32_t *lut,
                               unsigned char const *pixel)
{
    float const *sample;
    int best_index;
    float best_distance;

    if (lut == NULL || pixel == NULL) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    if (lut->policy == SIXEL_LUT_POLICY_FHEDT) {
        if (lut->fhedt_ready && lut->fhedt != NULL) {
            return sixel_lookup_fhedt_float32_map(lut->fhedt, sample);
        }
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_VPTREE) {
        if (lut->vptree_ready && lut->vptree != NULL) {
            return sixel_lookup_vptree_float32_map(lut->vptree, sample);
        }
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_EYTZINGER) {
        return sixel_lookup_float32_lookup_1d_eytzinger(lut, sample);
    }
    if (lut->policy == SIXEL_LUT_POLICY_RBC) {
        return sixel_lookup_float32_rbc_search(lut, sample, 0);
    }
    if (lut->policy == SIXEL_LUT_POLICY_MAHALANOBIS) {
        return sixel_lookup_float32_rbc_search(lut, sample, 1);
    }
    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        best_index = 0;
        best_distance = FLT_MAX;
        sixel_lookup_float32_search_kdtree(lut,
                                           lut->kdtree_root,
                                           sample,
                                           &best_index,
                                           &best_distance);
        return best_index;
    }

    return sixel_lookup_float32_linear_search(lut, sample);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
