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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_FLOAT_H
# include <float.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif

#include "lookup-policy-mahalanobis.h"
#include "pixelformat.h"
#include "sixel_atomic.h"


/*
 * IDL (internal contract)
 *
 * class LookupMahalanobis : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 * }
 */

enum { SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS = 3 };

typedef struct sixel_lookup_policy_mahalanobis_8bit {
    int policy;
    int depth;
    int ncolors;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
} sixel_lookup_policy_mahalanobis_8bit_t;

typedef struct sixel_lookup_policy_mahalanobis_float32 {
    int policy;
    int depth;
    int ncolors;
    float weights[SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS];
    float *palette;
    sixel_allocator_t *allocator;
    struct {
        int pivot_count;
        int *pivots;
        float *radius;
        int *member_offset;
        int *member_index;
        float *mean;
        float *inv_cov;
        int ready;
    } rbc;
} sixel_lookup_policy_mahalanobis_float32_t;

static void
sixel_lookup_policy_mahalanobis_float32_clear_state(
    sixel_lookup_policy_mahalanobis_float32_t *lut);

static void
sixel_lookup_policy_mahalanobis_8bit_init(sixel_lookup_policy_mahalanobis_8bit_t *lut,
                       sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(*lut));
    lut->allocator = allocator;
}

static void
sixel_lookup_policy_mahalanobis_8bit_clear(sixel_lookup_policy_mahalanobis_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
}

static void
sixel_lookup_policy_mahalanobis_8bit_finalize(sixel_lookup_policy_mahalanobis_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_mahalanobis_8bit_clear(lut);
    lut->allocator = NULL;
}

static void
sixel_lookup_policy_mahalanobis_float32_init(sixel_lookup_policy_mahalanobis_float32_t *lut,
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
sixel_lookup_policy_mahalanobis_float32_clear(sixel_lookup_policy_mahalanobis_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
    sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
    lut->depth = 0;
    lut->ncolors = 0;
}

static void
sixel_lookup_policy_mahalanobis_float32_finalize(sixel_lookup_policy_mahalanobis_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_mahalanobis_float32_clear(lut);
    lut->allocator = NULL;
}

typedef struct sixel_lookup_policy_mahalanobis_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int backend_initialized;
    int prepared;
    sixel_lookup_policy_mahalanobis_8bit_t state_8bit;
    sixel_lookup_policy_mahalanobis_float32_t state_float;
} sixel_lookup_policy_mahalanobis_object_t;

static sixel_lookup_policy_mahalanobis_object_t *
sixel_lookup_policy_mahalanobis_from_base(
    sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_mahalanobis_object_t *)(void *)policy;
}

static sixel_lookup_policy_mahalanobis_object_t const *
sixel_lookup_policy_mahalanobis_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_mahalanobis_object_t const *)(void const *)
        policy;
}

static float
sixel_lookup_policy_mahalanobis_float32_component(float const *palette,
                                                  int depth,
                                                  int index,
                                                  int axis)
{
    int clamped_axis;

    clamped_axis = axis;
    if (clamped_axis < 0) {
        clamped_axis = 0;
    } else if (clamped_axis >= SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS) {
        clamped_axis = SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS - 1;
    }

    return palette[index * depth + clamped_axis];
}

static float
sixel_lookup_policy_mahalanobis_float32_distance(
    sixel_lookup_policy_mahalanobis_float32_t const *lut,
    float const *sample,
    int palette_index)
{
    float diff;
    float distance;
    int component;

    diff = 0.0f;
    distance = 0.0f;
    component = 0;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS;
            ++component) {
        diff = sample[component]
             - sixel_lookup_policy_mahalanobis_float32_component(
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

static float
sixel_lookup_policy_mahalanobis_weighted_component(
    sixel_lookup_policy_mahalanobis_float32_t const *lut,
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
sixel_lookup_policy_mahalanobis_quadratic3(float const *m, float const *v)
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
sixel_lookup_policy_mahalanobis_inverse3(float const *src, float *dst)
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

static void
sixel_lookup_policy_mahalanobis_float32_clear_state(
    sixel_lookup_policy_mahalanobis_float32_t *lut)
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

static int
sixel_lookup_policy_mahalanobis_member_index_at(
    sixel_lookup_policy_mahalanobis_float32_t const *lut,
    int position,
    int *member_index)
{
    int member_count;

    member_count = 0;
    if (lut == NULL || member_index == NULL || lut->rbc.member_index == NULL) {
        return 0;
    }

    member_count = lut->ncolors;
    if (member_count <= 0 || position < 0 || position >= member_count) {
        return 0;
    }

    *member_index = lut->rbc.member_index[position];
    return 1;
}

static SIXELSTATUS
sixel_lookup_policy_mahalanobis_float32_prepare_palette(
    sixel_lookup_policy_mahalanobis_float32_t *lut,
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

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }

    total = (size_t)lut->ncolors * (size_t)lut->depth;
    lut->palette = (float *)sixel_allocator_malloc(lut->allocator,
                                                   total * sizeof(float));
    if (lut->palette == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_mahalanobis: "
            "float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_mahalanobis: "
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

static SIXELSTATUS
sixel_lookup_policy_mahalanobis_float32_configure_clusters(
    sixel_lookup_policy_mahalanobis_float32_t *lut)
{
    int pivots;
    int i;
    int j;
    int k;
    int c;
    int cursor;
    int best_pivot;
    int start;
    int end;
    float best_distance;
    float distance;
    float radius;
    float cov[9];
    float mean0;
    float mean1;
    float mean2;
    float d0;
    float d1;
    float d2;

    pivots = 0;
    i = 0;
    j = 0;
    k = 0;
    c = 0;
    cursor = 0;
    best_pivot = 0;
    start = 0;
    end = 0;
    best_distance = 0.0f;
    distance = 0.0f;
    radius = 0.0f;
    mean0 = 0.0f;
    mean1 = 0.0f;
    mean2 = 0.0f;
    d0 = 0.0f;
    d1 = 0.0f;
    d2 = 0.0f;

    sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
    pivots = lut->ncolors;
    if (pivots > 16) {
        pivots = 16;
    }
    if (pivots <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut->rbc.pivots = (int *)calloc((size_t)pivots, sizeof(int));
    lut->rbc.radius = (float *)calloc((size_t)pivots, sizeof(float));
    lut->rbc.member_offset = (int *)calloc((size_t)pivots + 1U, sizeof(int));
    lut->rbc.member_index = (int *)calloc((size_t)lut->ncolors, sizeof(int));
    lut->rbc.mean = (float *)calloc((size_t)pivots * 3U, sizeof(float));
    lut->rbc.inv_cov = (float *)calloc((size_t)pivots * 9U, sizeof(float));
    if (lut->rbc.pivots == NULL || lut->rbc.radius == NULL
            || lut->rbc.member_offset == NULL
            || lut->rbc.member_index == NULL
            || lut->rbc.mean == NULL
            || lut->rbc.inv_cov == NULL) {
        sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
        return SIXEL_BAD_ALLOCATION;
    }

    for (j = 0; j < pivots; ++j) {
        lut->rbc.pivots[j] = (j * lut->ncolors) / pivots;
    }

    for (i = 0; i < lut->ncolors; ++i) {
        best_pivot = 0;
        best_distance = FLT_MAX;
        for (j = 0; j < pivots; ++j) {
            distance = sixel_lookup_policy_mahalanobis_float32_distance(
                lut,
                lut->palette + (size_t)i * 3U,
                lut->rbc.pivots[j]);
            if (distance < best_distance) {
                best_distance = distance;
                best_pivot = j;
            }
        }
        if (best_pivot < 0 || best_pivot >= pivots) {
            sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
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
            distance = sixel_lookup_policy_mahalanobis_float32_distance(
                lut,
                lut->palette + (size_t)i * 3U,
                lut->rbc.pivots[j]);
            if (distance < best_distance) {
                best_distance = distance;
                best_pivot = j;
            }
        }
        if (best_pivot < 0 || best_pivot >= pivots) {
            sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        cursor = lut->rbc.member_offset[best_pivot]++;
        if (cursor < 0 || cursor >= lut->ncolors) {
            sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        lut->rbc.member_index[cursor] = i;
        radius = sqrtf(best_distance);
        if (radius > lut->rbc.radius[best_pivot]) {
            lut->rbc.radius[best_pivot] = radius;
        }
    }

    for (j = pivots; j > 0; --j) {
        lut->rbc.member_offset[j] = lut->rbc.member_offset[j - 1];
    }
    lut->rbc.member_offset[0] = 0;

    for (j = 0; j < pivots; ++j) {
        start = lut->rbc.member_offset[j];
        end = lut->rbc.member_offset[j + 1];
        if (start < 0 || end < start || end > lut->ncolors) {
            sixel_lookup_policy_mahalanobis_float32_clear_state(lut);
            return SIXEL_RUNTIME_ERROR;
        }

        mean0 = 0.0f;
        mean1 = 0.0f;
        mean2 = 0.0f;
        lut->rbc.inv_cov[j * 9 + 0] = 1.0f;
        lut->rbc.inv_cov[j * 9 + 4] = 1.0f;
        lut->rbc.inv_cov[j * 9 + 8] = 1.0f;

        if (start == end) {
            continue;
        }

        for (k = start; k < end; ++k) {
            if (!sixel_lookup_policy_mahalanobis_member_index_at(
                    lut,
                    k,
                    &c)) {
                continue;
            }
            if (c < 0 || c >= lut->ncolors) {
                continue;
            }
            mean0 += lut->palette[c * 3 + 0];
            mean1 += lut->palette[c * 3 + 1];
            mean2 += lut->palette[c * 3 + 2];
        }
        i = end - start;
        mean0 /= (float)i;
        mean1 /= (float)i;
        mean2 /= (float)i;
        lut->rbc.mean[j * 3 + 0] =
            sixel_lookup_policy_mahalanobis_weighted_component(lut, mean0, 0);
        lut->rbc.mean[j * 3 + 1] =
            sixel_lookup_policy_mahalanobis_weighted_component(lut, mean1, 1);
        lut->rbc.mean[j * 3 + 2] =
            sixel_lookup_policy_mahalanobis_weighted_component(lut, mean2, 2);

        if (i < 2) {
            continue;
        }

        memset(cov, 0, sizeof(cov));
        for (k = start; k < end; ++k) {
            if (!sixel_lookup_policy_mahalanobis_member_index_at(
                    lut,
                    k,
                    &c)) {
                continue;
            }
            if (c < 0 || c >= lut->ncolors) {
                continue;
            }
            d0 = sixel_lookup_policy_mahalanobis_weighted_component(
                lut,
                lut->palette[c * 3 + 0],
                0) - lut->rbc.mean[j * 3 + 0];
            d1 = sixel_lookup_policy_mahalanobis_weighted_component(
                lut,
                lut->palette[c * 3 + 1],
                1) - lut->rbc.mean[j * 3 + 1];
            d2 = sixel_lookup_policy_mahalanobis_weighted_component(
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
        if (!sixel_lookup_policy_mahalanobis_inverse3(
                cov,
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
sixel_lookup_policy_mahalanobis_float32_search(
    sixel_lookup_policy_mahalanobis_float32_t const *lut,
    float const *sample)
{
    int j;
    int k;
    int start;
    int end;
    int idx;
    int best_index;
    float best2;
    float dist2;
    float diff[3];

    j = 0;
    k = 0;
    start = 0;
    end = 0;
    idx = 0;
    best_index = 0;
    best2 = FLT_MAX;
    dist2 = 0.0f;
    diff[0] = 0.0f;
    diff[1] = 0.0f;
    diff[2] = 0.0f;

    if (lut->rbc.ready == 0) {
        return 0;
    }

    for (j = 0; j < lut->rbc.pivot_count; ++j) {
        diff[0] = sixel_lookup_policy_mahalanobis_weighted_component(
            lut,
            sample[0],
            0) - lut->rbc.mean[j * 3 + 0];
        diff[1] = sixel_lookup_policy_mahalanobis_weighted_component(
            lut,
            sample[1],
            1) - lut->rbc.mean[j * 3 + 1];
        diff[2] = sixel_lookup_policy_mahalanobis_weighted_component(
            lut,
            sample[2],
            2) - lut->rbc.mean[j * 3 + 2];
        (void)sixel_lookup_policy_mahalanobis_quadratic3(
            lut->rbc.inv_cov + j * 9,
            diff);

        start = lut->rbc.member_offset[j];
        end = lut->rbc.member_offset[j + 1];
        if (start < 0 || end < start || end > lut->ncolors) {
            return 0;
        }
        for (k = start; k < end; ++k) {
            if (!sixel_lookup_policy_mahalanobis_member_index_at(
                    lut,
                    k,
                    &idx)) {
                continue;
            }
            if (idx < 0 || idx >= lut->ncolors) {
                continue;
            }
            dist2 = sixel_lookup_policy_mahalanobis_float32_distance(
                lut,
                sample,
                idx);
            if (dist2 < best2) {
                best2 = dist2;
                best_index = idx;
            }
        }
    }

    return best_index;
}

static SIXELSTATUS
sixel_lookup_policy_mahalanobis_configure_float32(
    sixel_lookup_policy_mahalanobis_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS];
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

    sixel_lookup_policy_mahalanobis_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_MAHALANOBIS;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_MAHALANOBIS_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_policy_mahalanobis_float32_prepare_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_lookup_policy_mahalanobis_float32_configure_clusters(lut);
}

static SIXELSTATUS
sixel_lookup_policy_mahalanobis_configure_8bit(
    sixel_lookup_policy_mahalanobis_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_mahalanobis_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_MAHALANOBIS;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->palette = request->palette;

    return SIXEL_OK;
}

static int
sixel_lookup_policy_mahalanobis_map_8bit(sixel_lookup_policy_mahalanobis_8bit_t const *lut,
                                         unsigned char const *pixel)
{
    int result;
    int diff;
    int i;
    int distant;
    int pixel0;
    int pixel1;
    int pixel2;
    int delta;
    unsigned char const *entry;
    unsigned char const *end;

    result = 0;
    diff = INT_MAX;
    i = 0;
    distant = 0;
    pixel0 = 0;
    pixel1 = 0;
    pixel2 = 0;
    delta = 0;
    entry = NULL;
    end = NULL;
    if (lut == NULL || pixel == NULL || lut->palette == NULL
            || lut->ncolors <= 0 || lut->depth <= 0) {
        return 0;
    }

    entry = lut->palette;
    end = lut->palette + (size_t)lut->ncolors * (size_t)lut->depth;
    pixel0 = (int)pixel[0];
    pixel1 = (int)pixel[1];
    pixel2 = (int)pixel[2];
    for (i = 0; entry < end; ++i, entry += lut->depth) {
        delta = pixel0 - (int)entry[0];
        distant = delta * delta;
        delta = pixel1 - (int)entry[1];
        distant += delta * delta;
        delta = pixel2 - (int)entry[2];
        distant += delta * delta;
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    return result;
}

static void
sixel_lookup_policy_mahalanobis_reset_state(
    sixel_lookup_policy_mahalanobis_object_t *object)
{
    if (object == NULL) {
        return;
    }

    if (object->backend_initialized != 0) {
        sixel_lookup_policy_mahalanobis_8bit_finalize(&object->state_8bit);
        sixel_lookup_policy_mahalanobis_float32_finalize(&object->state_float);
    }

    memset(&object->state_8bit, 0, sizeof(object->state_8bit));
    memset(&object->state_float, 0, sizeof(object->state_float));
    object->backend_initialized = 0;
    object->prepared = 0;
}

static void
sixel_lookup_policy_mahalanobis_detach_state(
    sixel_lookup_policy_mahalanobis_object_t *object)
{
    if (object == NULL) {
        return;
    }

    memset(&object->state_8bit, 0, sizeof(object->state_8bit));
    memset(&object->state_float, 0, sizeof(object->state_float));
    object->backend_initialized = 0;
    object->prepared = 0;
}

static void
sixel_lookup_policy_mahalanobis_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_mahalanobis_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_mahalanobis_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_mahalanobis_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_mahalanobis_object_t *object;
    unsigned int previous;
    sixel_allocator_t *allocator;

    object = NULL;
    previous = 0U;
    allocator = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_mahalanobis_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_mahalanobis_reset_state(object);
        allocator = object->allocator;
        object->allocator = NULL;
        if (allocator != NULL) {
            sixel_allocator_free(allocator, object);
            sixel_allocator_unref(allocator);
        }
    }
}

static SIXELSTATUS
sixel_lookup_policy_mahalanobis_prepare_8bit(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_mahalanobis_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_mahalanobis_object_t *reuse_object;

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_mahalanobis_from_base(policy);
    sixel_lookup_policy_mahalanobis_reset_state(object);
    object->backend_initialized = 1;
    sixel_lookup_policy_mahalanobis_8bit_init(&object->state_8bit, request->allocator);

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


    if (reuse_policy != NULL
            && reuse_policy->vtbl == policy->vtbl) {
        reuse_object = sixel_lookup_policy_mahalanobis_from_base(reuse_policy);
        if (reuse_object->prepared != 0) {
            sixel_lookup_policy_mahalanobis_reset_state(object);
            object->state_8bit = reuse_object->state_8bit;
            object->backend_initialized =
                reuse_object->backend_initialized;
            object->prepared = reuse_object->prepared;
            sixel_lookup_policy_mahalanobis_detach_state(reuse_object);
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    status = sixel_lookup_policy_mahalanobis_configure_8bit(
        &object->state_8bit,
        request);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_mahalanobis_reset_state(object);
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

static SIXELSTATUS
sixel_lookup_policy_mahalanobis_prepare_float32(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_mahalanobis_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_mahalanobis_object_t *reuse_object;

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_mahalanobis_from_base(policy);
    sixel_lookup_policy_mahalanobis_reset_state(object);
    object->backend_initialized = 1;
    sixel_lookup_policy_mahalanobis_float32_init(&object->state_float,
                                                 request->allocator);

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

    if (reuse_policy != NULL
            && reuse_policy->vtbl == policy->vtbl) {
        reuse_object = sixel_lookup_policy_mahalanobis_from_base(reuse_policy);
        if (reuse_object->prepared != 0) {
            sixel_lookup_policy_mahalanobis_reset_state(object);
            object->state_float = reuse_object->state_float;
            object->backend_initialized = reuse_object->backend_initialized;
            object->prepared = reuse_object->prepared;
            sixel_lookup_policy_mahalanobis_detach_state(reuse_object);
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    status = sixel_lookup_policy_mahalanobis_configure_float32(
        &object->state_float,
        request);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_mahalanobis_reset_state(object);
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
sixel_lookup_policy_mahalanobis_map_pixel_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_mahalanobis_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_mahalanobis_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    return sixel_lookup_policy_mahalanobis_map_8bit(
        &object->state_8bit,
        pixel);
}

static int
sixel_lookup_policy_mahalanobis_map_pixel_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_mahalanobis_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_mahalanobis_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    return sixel_lookup_policy_mahalanobis_float32_search(
        &object->state_float,
        (float const *)(void const *)pixel);
}

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_mahalanobis_8bit_vtbl = {
    sixel_lookup_policy_mahalanobis_ref,
    sixel_lookup_policy_mahalanobis_unref,
    sixel_lookup_policy_mahalanobis_prepare_8bit,
    sixel_lookup_policy_mahalanobis_map_pixel_8bit,
};

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_mahalanobis_float32_vtbl = {
    sixel_lookup_policy_mahalanobis_ref,
    sixel_lookup_policy_mahalanobis_unref,
    sixel_lookup_policy_mahalanobis_prepare_float32,
    sixel_lookup_policy_mahalanobis_map_pixel_float32,
};

SIXELSTATUS
sixel_lookup_policy_mahalanobis_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    sixel_lookup_policy_mahalanobis_object_t *object;

    object = NULL;
    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_lookup_policy_mahalanobis_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_mahalanobis_8bit_new: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_mahalanobis_8bit_vtbl;
    object->ref = 1U;
    object->allocator = allocator;
    sixel_allocator_ref(allocator);
    *policy = &object->base;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_policy_mahalanobis_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    sixel_lookup_policy_mahalanobis_object_t *object;

    object = NULL;
    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_lookup_policy_mahalanobis_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_mahalanobis_float32_new:"
            " allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_mahalanobis_float32_vtbl;
    object->ref = 1U;
    object->allocator = allocator;
    sixel_allocator_ref(allocator);
    *policy = &object->base;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
