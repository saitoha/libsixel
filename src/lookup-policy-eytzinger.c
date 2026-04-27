/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include "lookup-policy-eytzinger.h"
#include "logger.h"
#include "pixelformat.h"
#include "sixel_atomic.h"


/*
 * IDL (internal contract)
 *
 * class LookupEytzinger : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 * }
 */

enum { SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS = 3 };

typedef struct sixel_lookup_policy_eytzinger_8bit_1d_eytzinger {
    int count;
    int weights[3];
    int window;
    float *keys;
    int *palette_index;
    int *rank;
    int *sorted_palette_index;
    float *sorted_keys;
    int ready;
} sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t;

typedef struct sixel_lookup_policy_eytzinger_float32_1d_eytzinger {
    int count;
    float weights[SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS];
    int window;
    float *keys;
    int *palette_index;
    int *rank;
    int *sorted_palette_index;
    float *sorted_keys;
    int ready;
} sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t;

typedef struct sixel_lookup_policy_eytzinger_8bit {
    int policy;
    int depth;
    int ncolors;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t eytz;
} sixel_lookup_policy_eytzinger_8bit_t;

typedef struct sixel_lookup_policy_eytzinger_float32 {
    int policy;
    int depth;
    int ncolors;
    float weights[SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS];
    float *palette;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t eytz;
} sixel_lookup_policy_eytzinger_float32_t;

static void
sixel_lookup_policy_eytzinger_float32_release(
    sixel_lookup_policy_eytzinger_float32_t *lut);

static void
sixel_lookup_policy_eytzinger_8bit_release(sixel_lookup_policy_eytzinger_8bit_t *lut);

static void
sixel_lookup_policy_eytzinger_8bit_init(sixel_lookup_policy_eytzinger_8bit_t *lut,
                       sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(*lut));
    lut->allocator = allocator;
}

static void
sixel_lookup_policy_eytzinger_8bit_clear(sixel_lookup_policy_eytzinger_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_eytzinger_8bit_release(lut);
    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
}

static void
sixel_lookup_policy_eytzinger_8bit_finalize(sixel_lookup_policy_eytzinger_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_eytzinger_8bit_clear(lut);
    lut->allocator = NULL;
}

static void
sixel_lookup_policy_eytzinger_float32_init(sixel_lookup_policy_eytzinger_float32_t *lut,
                          sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(*lut));
    lut->allocator = allocator;
    lut->weights[0] = 1.0f;
    lut->weights[1] = 1.0f;
    lut->weights[2] = 1.0f;
}

static void
sixel_lookup_policy_eytzinger_float32_clear(sixel_lookup_policy_eytzinger_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
    sixel_lookup_policy_eytzinger_float32_release(lut);
    lut->depth = 0;
    lut->ncolors = 0;
}

static void
sixel_lookup_policy_eytzinger_float32_finalize(sixel_lookup_policy_eytzinger_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_eytzinger_float32_clear(lut);
    lut->allocator = NULL;
}

typedef struct sixel_lookup_policy_eytzinger_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int backend_initialized;
    int prepared;
    sixel_lookup_policy_eytzinger_8bit_t state_8bit;
    sixel_lookup_policy_eytzinger_float32_t state_float;
    int lookup_source_is_float;
} sixel_lookup_policy_eytzinger_object_t;

static sixel_lookup_policy_eytzinger_object_t *
sixel_lookup_policy_eytzinger_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_eytzinger_object_t *)(void *)policy;
}

static sixel_lookup_policy_eytzinger_object_t const *
sixel_lookup_policy_eytzinger_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_eytzinger_object_t const *)(void const *)policy;
}

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

typedef struct sixel_lookup_policy_eytzinger_8bit_pair {
    float key;
    int index;
} sixel_lookup_policy_eytzinger_8bit_pair_t;

typedef struct sixel_lookup_policy_eytzinger_float32_pair {
    float key;
    int index;
} sixel_lookup_policy_eytzinger_float32_pair_t;

static void
sixel_lookup_policy_eytzinger_log_event(int ncolors, char const *event)
{
    sixel_logger_t logger;
    SIXELSTATUS status;

    status = SIXEL_FALSE;
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

static void
sixel_lookup_policy_eytzinger_float32_release(
    sixel_lookup_policy_eytzinger_float32_t *lut)
{
    sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t *eytz;

    eytz = NULL;
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
sixel_lookup_policy_eytzinger_float_component(float const *palette,
                                              int depth,
                                              int index,
                                              int axis)
{
    int clamped_axis;

    clamped_axis = axis;
    if (clamped_axis < 0) {
        clamped_axis = 0;
    } else if (clamped_axis >= SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS) {
        clamped_axis = SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS - 1;
    }

    return palette[index * depth + clamped_axis];
}

static float
sixel_lookup_policy_eytzinger_float_distance(
    sixel_lookup_policy_eytzinger_float32_t const *lut,
    float const *sample,
    int palette_index)
{
    float diff;
    float distance;
    int component;

    diff = 0.0f;
    distance = 0.0f;
    component = 0;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS;
            ++component) {
        diff = sample[component]
            - sixel_lookup_policy_eytzinger_float_component(
                lut->palette,
                lut->depth,
                palette_index,
                component);
        diff *= diff;
        diff *= lut->weights[component];
        distance += diff;
    }

    return distance;
}

static SIXELSTATUS
sixel_lookup_policy_eytzinger_prepare_float_palette(
    sixel_lookup_policy_eytzinger_float32_t *lut,
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

    total = 0U;
    float_payload = 0U;
    index = 0;
    component = 0;
    cursor = NULL;
    float_cursor = NULL;
    expected_float_depth = 0;

    if (lut == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }

    total = (size_t)lut->ncolors * (size_t)lut->depth;
    lut->palette = (float *)sixel_allocator_malloc(lut->allocator,
                                                   total * sizeof(float));
    if (lut->palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_eytzinger: "
            "float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_eytzinger: "
                "float palette depth mismatch.");
            sixel_allocator_free(lut->allocator, lut->palette);
            lut->palette = NULL;
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
            *cursor = sixel_pixelformat_byte_to_float(
                pixelformat,
                component,
                palette[index * lut->depth + component]);
            ++cursor;
        }
    }

    return SIXEL_OK;
}

static float
sixel_lookup_policy_eytzinger_float32_project_palette(
    sixel_lookup_policy_eytzinger_float32_t const *lut,
    int palette_index)
{
    float comp0;
    float comp1;
    float comp2;
    float key;

    comp0 = sixel_lookup_policy_eytzinger_float_component(
        lut->palette,
        lut->depth,
        palette_index,
        0);
    comp1 = sixel_lookup_policy_eytzinger_float_component(
        lut->palette,
        lut->depth,
        palette_index,
        1);
    comp2 = sixel_lookup_policy_eytzinger_float_component(
        lut->palette,
        lut->depth,
        palette_index,
        2);
    key = lut->eytz.weights[0] * comp0
        + lut->eytz.weights[1] * comp1
        + lut->eytz.weights[2] * comp2;

    return key;
}

static float
sixel_lookup_policy_eytzinger_float32_project_sample(
    sixel_lookup_policy_eytzinger_float32_t const *lut,
    float const *sample)
{
    float comp0;
    float comp1;
    float comp2;
    float key;
    int depth;

    comp0 = 0.0f;
    comp1 = 0.0f;
    comp2 = 0.0f;
    key = 0.0f;
    depth = 0;

    if (lut == NULL || sample == NULL) {
        return 0.0f;
    }

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
sixel_lookup_policy_eytzinger_float32_compare(void const *left,
                                               void const *right)
{
    float diff;
    sixel_lookup_policy_eytzinger_float32_pair_t const *a;
    sixel_lookup_policy_eytzinger_float32_pair_t const *b;

    diff = 0.0f;
    a = NULL;
    b = NULL;
    a = (sixel_lookup_policy_eytzinger_float32_pair_t const *)left;
    b = (sixel_lookup_policy_eytzinger_float32_pair_t const *)right;
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
sixel_lookup_policy_eytzinger_float32_fill(
    sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t *eytz,
    sixel_lookup_policy_eytzinger_float32_pair_t const *src,
    int count,
    int node,
    int *rank)
{
    if (node > count) {
        return;
    }

    sixel_lookup_policy_eytzinger_float32_fill(eytz,
                                               src,
                                               count,
                                               node * 2,
                                               rank);
    eytz->keys[node] = src[*rank].key;
    eytz->palette_index[node] = src[*rank].index;
    eytz->rank[node] = *rank;
    (*rank)++;
    sixel_lookup_policy_eytzinger_float32_fill(eytz,
                                               src,
                                               count,
                                               node * 2 + 1,
                                               rank);
}

static int
sixel_lookup_policy_eytzinger_float32_lower_bound(
    sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t const *eytz,
    float key)
{
    int node;
    int candidate;
    int count;
    int next;

    node = 1;
    candidate = 0;
    count = eytz->count;
    next = 0;
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
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, node, count);
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, node + 1, count);
    }

    return candidate;
}

static SIXELSTATUS
sixel_lookup_policy_eytzinger_configure_float32_1d(
    sixel_lookup_policy_eytzinger_float32_t *lut)
{
    SIXELSTATUS status;
    sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t *eytz;
    sixel_lookup_policy_eytzinger_float32_pair_t *pairs;
    size_t bytes;
    int count;
    int index;
    int rank;
    float weight_sum;
    float weight_norm;

    status = SIXEL_BAD_ALLOCATION;
    eytz = NULL;
    pairs = NULL;
    bytes = 0U;
    count = 0;
    index = 0;
    rank = 0;
    weight_sum = 0.0f;
    weight_norm = 1.0f;

    if (lut == NULL || lut->palette == NULL || lut->ncolors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    eytz = &lut->eytz;
    sixel_lookup_policy_eytzinger_float32_release(lut);
    eytz->ready = 0;
    eytz->count = 0;
    eytz->window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    weight_sum = lut->weights[0] + lut->weights[1] + lut->weights[2];
    if (weight_sum > 0.0f) {
        weight_norm = sqrtf(weight_sum);
    }
    eytz->weights[0] = sqrtf(lut->weights[0]) / weight_norm;
    eytz->weights[1] = sqrtf(lut->weights[1]) / weight_norm;
    eytz->weights[2] = sqrtf(lut->weights[2]) / weight_norm;

    count = lut->ncolors;
    bytes = (size_t)count * sizeof(*pairs);
    pairs = (sixel_lookup_policy_eytzinger_float32_pair_t *)
        sixel_allocator_malloc(lut->allocator, bytes);
    if (pairs == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_eytzinger: "
            "float Eytzinger allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0; index < count; ++index) {
        pairs[index].index = index;
        pairs[index].key =
            sixel_lookup_policy_eytzinger_float32_project_palette(lut, index);
    }

    qsort(pairs,
          (size_t)count,
          sizeof(*pairs),
          sixel_lookup_policy_eytzinger_float32_compare);

    sixel_lookup_policy_eytzinger_log_event(count, "builder-start");
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
            "sixel_lookup_policy_eytzinger: "
            "float Eytzinger arrays missing.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }

    for (index = 0; index < count; ++index) {
        eytz->sorted_palette_index[index] = pairs[index].index;
        eytz->sorted_keys[index] = pairs[index].key;
    }

    rank = 0;
    sixel_lookup_policy_eytzinger_float32_fill(eytz, pairs, count, 1, &rank);

    eytz->count = count;
    eytz->ready = 1;
    sixel_lookup_policy_eytzinger_log_event(count, "builder-end");
    status = SIXEL_OK;

error:
    if (pairs != NULL) {
        sixel_allocator_free(lut->allocator, pairs);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_eytzinger_float32_release(lut);
    }

    return status;
}

static SIXELSTATUS
sixel_lookup_policy_eytzinger_configure_float32(
    sixel_lookup_policy_eytzinger_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS];
    float range;
    float scale;
    int component;

    status = SIXEL_FALSE;
    base_weights[0] = 0.0f;
    base_weights[1] = 0.0f;
    base_weights[2] = 0.0f;
    range = 1.0f;
    scale = 1.0f;
    component = 0;

    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_eytzinger_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_EYTZINGER;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_EYTZINGER_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_policy_eytzinger_prepare_float_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_lookup_policy_eytzinger_configure_float32_1d(lut);
}

static int
sixel_lookup_policy_eytzinger_map_float32(
    sixel_lookup_policy_eytzinger_float32_t const *lut,
    unsigned char const *pixel)
{
    sixel_lookup_policy_eytzinger_float32_1d_eytzinger_t const *eytz;
    float const *sample;
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

    eytz = NULL;
    sample = NULL;
    key = 0.0f;
    count = 0;
    node = 0;
    rank = 0;
    start = 0;
    end = 0;
    offset_left = 0;
    offset_right = 0;
    stop_left = 0;
    stop_right = 0;
    palette_index = 0;
    best_index = 0;
    best_distance = 0.0f;
    distance = 0.0f;
    key_diff = 0.0f;
    key_diff_sq = 0.0f;

    if (lut == NULL || pixel == NULL || lut->palette == NULL
            || lut->ncolors <= 0) {
        return 0;
    }

    eytz = &lut->eytz;
    if (eytz->ready == 0 || eytz->count <= 0) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    key = sixel_lookup_policy_eytzinger_float32_project_sample(lut, sample);
    node = sixel_lookup_policy_eytzinger_float32_lower_bound(eytz, key);
    count = eytz->count;
    if (node == 0) {
        rank = count - 1;
    } else {
        rank = eytz->rank[node];
    }
    start = 0;
    end = count - 1;
    best_index = eytz->sorted_palette_index[rank];
    best_distance = sixel_lookup_policy_eytzinger_float_distance(
        lut,
        sample,
        best_index);
    offset_left = rank - 1;
    offset_right = rank + 1;
    stop_left = 0;
    stop_right = 0;
    while (stop_left == 0 || stop_right == 0) {
        if (stop_left == 0) {
            if (offset_left < start) {
                stop_left = 1;
            } else {
                key_diff = key - eytz->sorted_keys[offset_left];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance) {
                    stop_left = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_left];
                    distance = sixel_lookup_policy_eytzinger_float_distance(
                        lut,
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
                key_diff = key - eytz->sorted_keys[offset_right];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance) {
                    stop_right = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_right];
                    distance = sixel_lookup_policy_eytzinger_float_distance(
                        lut,
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
sixel_lookup_policy_eytzinger_8bit_release(sixel_lookup_policy_eytzinger_8bit_t *lut)
{
    sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t *eytz;

    eytz = NULL;
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
sixel_lookup_policy_eytzinger_8bit_project(sixel_lookup_policy_eytzinger_8bit_t const *lut,
                                           unsigned char const *pixel)
{
    int depth;
    int comp0;
    int comp1;
    int comp2;
    float key;

    depth = 0;
    comp0 = 0;
    comp1 = 0;
    comp2 = 0;
    key = 0.0f;
    if (lut == NULL || pixel == NULL) {
        return 0.0f;
    }

    depth = lut->depth;
    comp0 = (depth > 0) ? (int)pixel[0] : 0;
    comp1 = (depth > 1) ? (int)pixel[1] : 0;
    comp2 = (depth > 2) ? (int)pixel[2] : 0;
    key = (float)lut->eytz.weights[0] * (float)comp0
        + (float)lut->eytz.weights[1] * (float)comp1
        + (float)lut->eytz.weights[2] * (float)comp2;

    return key;
}

static int
sixel_lookup_policy_eytzinger_8bit_compare(void const *left,
                                            void const *right)
{
    float diff;
    sixel_lookup_policy_eytzinger_8bit_pair_t const *a;
    sixel_lookup_policy_eytzinger_8bit_pair_t const *b;

    diff = 0.0f;
    a = NULL;
    b = NULL;
    a = (sixel_lookup_policy_eytzinger_8bit_pair_t const *)left;
    b = (sixel_lookup_policy_eytzinger_8bit_pair_t const *)right;
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
sixel_lookup_policy_eytzinger_8bit_fill(
    sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t *eytz,
    sixel_lookup_policy_eytzinger_8bit_pair_t const *src,
    int count,
    int node,
    int *rank)
{
    if (node > count) {
        return;
    }

    sixel_lookup_policy_eytzinger_8bit_fill(eytz,
                                            src,
                                            count,
                                            node * 2,
                                            rank);
    eytz->keys[node] = src[*rank].key;
    eytz->palette_index[node] = src[*rank].index;
    eytz->rank[node] = *rank;
    (*rank)++;
    sixel_lookup_policy_eytzinger_8bit_fill(eytz,
                                            src,
                                            count,
                                            node * 2 + 1,
                                            rank);
}

static int
sixel_lookup_policy_eytzinger_8bit_lower_bound(
    sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t const *eytz,
    float key)
{
    int node;
    int candidate;
    int count;
    int next;

    node = 1;
    candidate = 0;
    count = eytz->count;
    next = 0;
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
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, node, count);
        SIXEL_LOOKUP_EYTZINGER_PREFETCH(eytz->rank, node + 1, count);
    }

    return candidate;
}

static int
sixel_lookup_policy_eytzinger_8bit_distance(
    sixel_lookup_policy_eytzinger_8bit_t const *lut,
    unsigned char const *pixel,
    int palette_index)
{
    unsigned char const *entry;
    int depth;
    int pixel0;
    int pixel1;
    int pixel2;
    int entry0;
    int entry1;
    int entry2;
    int diff;
    int distance;

    entry = NULL;
    depth = 0;
    pixel0 = 0;
    pixel1 = 0;
    pixel2 = 0;
    entry0 = 0;
    entry1 = 0;
    entry2 = 0;
    diff = 0;
    distance = 0;
    if (lut == NULL || pixel == NULL || lut->palette == NULL
            || lut->depth <= 0 || lut->ncolors <= 0
            || palette_index < 0 || palette_index >= lut->ncolors) {
        return 0;
    }

    depth = lut->depth;
    pixel0 = (depth > 0) ? (int)pixel[0] : 0;
    pixel1 = (depth > 1) ? (int)pixel[1] : 0;
    pixel2 = (depth > 2) ? (int)pixel[2] : 0;
    entry = lut->palette + (size_t)palette_index * (size_t)depth;
    entry0 = (depth > 0) ? (int)entry[0] : 0;
    entry1 = (depth > 1) ? (int)entry[1] : 0;
    entry2 = (depth > 2) ? (int)entry[2] : 0;

    diff = pixel0 - entry0;
    distance = diff * diff;
    diff = pixel1 - entry1;
    distance += diff * diff;
    diff = pixel2 - entry2;
    distance += diff * diff;

    return distance;
}

static SIXELSTATUS
sixel_lookup_policy_eytzinger_configure_8bit_1d(
    sixel_lookup_policy_eytzinger_8bit_t *lut,
    unsigned char const *palette,
    int ncolors,
    int wcomp1,
    int wcomp2,
    int wcomp3)
{
    SIXELSTATUS status;
    sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t *eytz;
    sixel_lookup_policy_eytzinger_8bit_pair_t *pairs;
    size_t bytes;
    int depth;
    int count;
    int index;
    int rank;

    status = SIXEL_BAD_ALLOCATION;
    eytz = NULL;
    pairs = NULL;
    bytes = 0U;
    depth = 0;
    count = 0;
    index = 0;
    rank = 0;

    if (lut == NULL || palette == NULL || ncolors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    eytz = &lut->eytz;
    sixel_lookup_policy_eytzinger_8bit_release(lut);
    eytz->ready = 0;
    eytz->count = 0;
    eytz->window = SIXEL_LOOKUP_EYTZINGER_WINDOW;
    eytz->weights[0] = wcomp1;
    eytz->weights[1] = wcomp2;
    eytz->weights[2] = wcomp3;

    depth = lut->depth;
    count = ncolors;
    bytes = (size_t)count * sizeof(*pairs);
    pairs = (sixel_lookup_policy_eytzinger_8bit_pair_t *)
        sixel_allocator_malloc(lut->allocator, bytes);
    if (pairs == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_eytzinger: "
            "8bit Eytzinger allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0; index < count; ++index) {
        pairs[index].index = index;
        pairs[index].key = sixel_lookup_policy_eytzinger_8bit_project(
            lut,
            palette + (size_t)index * (size_t)depth);
    }

    qsort(pairs,
          (size_t)count,
          sizeof(*pairs),
          sixel_lookup_policy_eytzinger_8bit_compare);

    sixel_lookup_policy_eytzinger_log_event(count, "builder-start");
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
            "sixel_lookup_policy_eytzinger: "
            "8bit Eytzinger arrays missing.");
        status = SIXEL_BAD_ALLOCATION;
        goto error;
    }

    for (index = 0; index < count; ++index) {
        eytz->sorted_palette_index[index] = pairs[index].index;
        eytz->sorted_keys[index] = pairs[index].key;
    }

    rank = 0;
    sixel_lookup_policy_eytzinger_8bit_fill(eytz, pairs, count, 1, &rank);
    eytz->count = count;
    eytz->ready = 1;
    sixel_lookup_policy_eytzinger_log_event(count, "builder-end");
    status = SIXEL_OK;

error:
    if (pairs != NULL) {
        sixel_allocator_free(lut->allocator, pairs);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_eytzinger_8bit_release(lut);
    }

    return status;
}

static SIXELSTATUS
sixel_lookup_policy_eytzinger_configure_8bit(
    sixel_lookup_policy_eytzinger_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_eytzinger_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_EYTZINGER;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->palette = request->palette;

    return sixel_lookup_policy_eytzinger_configure_8bit_1d(
        lut,
        request->palette,
        request->reqcolor,
        1,
        1,
        1);
}

static int
sixel_lookup_policy_eytzinger_map_8bit(
    sixel_lookup_policy_eytzinger_8bit_t const *lut,
    unsigned char const *pixel)
{
    sixel_lookup_policy_eytzinger_8bit_1d_eytzinger_t const *eytz;
    float key;
    int count;
    int node;
    int rank;
    int window;
    int start;
    int end;
    int offset_left;
    int offset_right;
    int stop_left;
    int stop_right;
    int palette_index;
    int best_index;
    int best_distance;
    int distance;
    float best_distance_f;
    float key_diff;
    float key_diff_sq;

    eytz = NULL;
    key = 0.0f;
    count = 0;
    node = 0;
    rank = 0;
    window = 0;
    start = 0;
    end = 0;
    offset_left = 0;
    offset_right = 0;
    stop_left = 0;
    stop_right = 0;
    palette_index = 0;
    best_index = 0;
    best_distance = 0;
    distance = 0;
    best_distance_f = 0.0f;
    key_diff = 0.0f;
    key_diff_sq = 0.0f;

    if (lut == NULL || pixel == NULL || lut->palette == NULL
            || lut->ncolors <= 0) {
        return 0;
    }

    eytz = &lut->eytz;
    if (eytz->ready == 0 || eytz->count <= 0) {
        return 0;
    }

    key = sixel_lookup_policy_eytzinger_8bit_project(lut, pixel);
    node = sixel_lookup_policy_eytzinger_8bit_lower_bound(eytz, key);
    count = eytz->count;
    if (node == 0) {
        rank = count - 1;
    } else {
        rank = eytz->rank[node];
    }
    window = eytz->window;
    start = rank - window;
    if (start < 0) {
        start = 0;
    }
    end = rank + window;
    if (end >= count) {
        end = count - 1;
    }

    best_index = eytz->sorted_palette_index[rank];
    best_distance = sixel_lookup_policy_eytzinger_8bit_distance(
        lut,
        pixel,
        best_index);
    best_distance_f = (float)best_distance;
    offset_left = rank - 1;
    offset_right = rank + 1;
    stop_left = 0;
    stop_right = 0;
    while (stop_left == 0 || stop_right == 0) {
        if (stop_left == 0) {
            if (offset_left < start) {
                stop_left = 1;
            } else {
                key_diff = key - eytz->sorted_keys[offset_left];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance_f) {
                    stop_left = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_left];
                    distance = sixel_lookup_policy_eytzinger_8bit_distance(
                        lut,
                        pixel,
                        palette_index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_distance_f = (float)best_distance;
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
                key_diff = key - eytz->sorted_keys[offset_right];
                key_diff_sq = key_diff * key_diff;
                if (key_diff_sq > best_distance_f) {
                    stop_right = 1;
                } else {
                    palette_index = eytz->sorted_palette_index[offset_right];
                    distance = sixel_lookup_policy_eytzinger_8bit_distance(
                        lut,
                        pixel,
                        palette_index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_distance_f = (float)best_distance;
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
sixel_lookup_policy_eytzinger_reset_state(
    sixel_lookup_policy_eytzinger_object_t *object)
{
    if (object == NULL) {
        return;
    }

    if (object->backend_initialized != 0) {
        sixel_lookup_policy_eytzinger_8bit_finalize(&object->state_8bit);
        sixel_lookup_policy_eytzinger_float32_finalize(&object->state_float);
    }

    memset(&object->state_8bit, 0, sizeof(object->state_8bit));
    memset(&object->state_float, 0, sizeof(object->state_float));
    object->backend_initialized = 0;
    object->prepared = 0;
    object->lookup_source_is_float = 0;
}

static void
sixel_lookup_policy_eytzinger_detach_state(
    sixel_lookup_policy_eytzinger_object_t *object)
{
    if (object == NULL) {
        return;
    }

    memset(&object->state_8bit, 0, sizeof(object->state_8bit));
    memset(&object->state_float, 0, sizeof(object->state_float));
    object->backend_initialized = 0;
    object->prepared = 0;
    object->lookup_source_is_float = 0;
}

static void
sixel_lookup_policy_eytzinger_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_eytzinger_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_eytzinger_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_eytzinger_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_eytzinger_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_eytzinger_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_eytzinger_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_eytzinger_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_eytzinger_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_eytzinger_object_t *reuse_object;

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_eytzinger_from_base(policy);
    sixel_lookup_policy_eytzinger_reset_state(object);
    object->backend_initialized = 1;
    sixel_lookup_policy_eytzinger_8bit_init(&object->state_8bit, request->allocator);
    sixel_lookup_policy_eytzinger_float32_init(&object->state_float, request->allocator);

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    reuse_policy = request->reuse_policy;
    if (request->parallel_dither_active != 0
            /* Reuse slot NULL means ownership migration is unsafe. */
            && request->reuse_policy_slot == NULL) {
        reuse_policy = NULL;
    }

    object->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);

    if (reuse_policy != NULL
            && reuse_policy->vtbl == policy->vtbl) {
        reuse_object = sixel_lookup_policy_eytzinger_from_base(reuse_policy);
        if (reuse_object->prepared != 0
                && reuse_object->lookup_source_is_float
                == object->lookup_source_is_float) {
            sixel_lookup_policy_eytzinger_reset_state(object);
            object->state_8bit = reuse_object->state_8bit;
            object->state_float = reuse_object->state_float;
            object->backend_initialized = reuse_object->backend_initialized;
            object->prepared = reuse_object->prepared;
            object->lookup_source_is_float =
                reuse_object->lookup_source_is_float;
            sixel_lookup_policy_eytzinger_detach_state(reuse_object);
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    if (object->lookup_source_is_float != 0) {
        status = sixel_lookup_policy_eytzinger_configure_float32(
            &object->state_float,
            request);
    } else {
        status = sixel_lookup_policy_eytzinger_configure_8bit(
            &object->state_8bit,
            request);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_eytzinger_reset_state(object);
        return status;
    }
    object->prepared = 1;

    if (request->reuse_policy_slot != NULL
            && *request->reuse_policy_slot == NULL) {
        *request->reuse_policy_slot = policy;
        policy->vtbl->ref(policy);
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_eytzinger_map_pixel_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_eytzinger_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_eytzinger_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    return sixel_lookup_policy_eytzinger_map_8bit(
        &object->state_8bit,
        pixel);
}

static int
sixel_lookup_policy_eytzinger_map_pixel_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_eytzinger_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_eytzinger_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    return sixel_lookup_policy_eytzinger_map_float32(
        &object->state_float,
        pixel);
}

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_eytzinger_8bit_vtbl = {
    sixel_lookup_policy_eytzinger_ref,
    sixel_lookup_policy_eytzinger_unref,
    sixel_lookup_policy_eytzinger_prepare,
    sixel_lookup_policy_eytzinger_map_pixel_8bit,
};

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_eytzinger_float32_vtbl = {
    sixel_lookup_policy_eytzinger_ref,
    sixel_lookup_policy_eytzinger_unref,
    sixel_lookup_policy_eytzinger_prepare,
    sixel_lookup_policy_eytzinger_map_pixel_float32,
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
static SIXELSTATUS
sixel_lookup_policy_eytzinger_create_with_vtbl(
    sixel_lookup_policy_interface_t **policy,
    sixel_lookup_policy_vtbl_t const *vtbl)
{
    sixel_lookup_policy_eytzinger_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL || vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_eytzinger_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_eytzinger: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = vtbl;
    object->ref = 1U;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

SIXELSTATUS
sixel_lookup_policy_create_eytzinger_8bit(
    sixel_lookup_policy_interface_t **policy)
{
    return sixel_lookup_policy_eytzinger_create_with_vtbl(
        policy,
        &g_sixel_lookup_policy_eytzinger_8bit_vtbl);
}

SIXELSTATUS
sixel_lookup_policy_create_eytzinger_float32(
    sixel_lookup_policy_interface_t **policy)
{
    return sixel_lookup_policy_eytzinger_create_with_vtbl(
        policy,
        &g_sixel_lookup_policy_eytzinger_float32_vtbl);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
