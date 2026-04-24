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

#include "lookup-8bit.h"
#include "lookup-common.h"
#include "lookup-float32.h"
#include "lookup-policy-private.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupRBC : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 */

typedef struct sixel_lookup_policy_rbc_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_lut_t *lut;
    int owns_lut;
    int lookup_source_is_float;
} sixel_lookup_policy_rbc_object_t;

static sixel_lookup_policy_rbc_object_t *
sixel_lookup_policy_rbc_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_rbc_object_t *)(void *)policy;
}

static sixel_lookup_policy_rbc_object_t const *
sixel_lookup_policy_rbc_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_rbc_object_t const *)(void const *)policy;
}

static float
sixel_lookup_policy_rbc_float32_component(float const *palette,
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

static float
sixel_lookup_policy_rbc_float32_distance(
    sixel_lookup_float32_t const *lut,
    float const *sample,
    int palette_index)
{
    float diff;
    float distance;
    int component;

    diff = 0.0f;
    distance = 0.0f;
    component = 0;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        diff = sample[component]
             - sixel_lookup_policy_rbc_float32_component(
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

static void
sixel_lookup_policy_rbc_float32_clear_state(sixel_lookup_float32_t *lut)
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

static SIXELSTATUS
sixel_lookup_policy_rbc_float32_prepare_palette(
    sixel_lookup_float32_t *lut,
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
            "sixel_lookup_policy_rbc: float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_rbc: float palette depth mismatch.");
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
sixel_lookup_policy_rbc_float32_configure_clusters(sixel_lookup_float32_t *lut)
{
    int pivots;
    int i;
    int j;
    int k;
    int cursor;
    int best_pivot;
    int start;
    int end;
    float best_distance;
    float distance;
    float radius;

    pivots = 0;
    i = 0;
    j = 0;
    k = 0;
    cursor = 0;
    best_pivot = 0;
    start = 0;
    end = 0;
    best_distance = 0.0f;
    distance = 0.0f;
    radius = 0.0f;

    sixel_lookup_policy_rbc_float32_clear_state(lut);
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
    if (lut->rbc.pivots == NULL || lut->rbc.radius == NULL
            || lut->rbc.member_offset == NULL
            || lut->rbc.member_index == NULL) {
        sixel_lookup_policy_rbc_float32_clear_state(lut);
        return SIXEL_BAD_ALLOCATION;
    }

    for (j = 0; j < pivots; ++j) {
        lut->rbc.pivots[j] = (j * lut->ncolors) / pivots;
    }

    for (i = 0; i < lut->ncolors; ++i) {
        best_pivot = 0;
        best_distance = FLT_MAX;
        for (j = 0; j < pivots; ++j) {
            distance = sixel_lookup_policy_rbc_float32_distance(
                lut,
                lut->palette + (size_t)i * 3U,
                lut->rbc.pivots[j]);
            if (distance < best_distance) {
                best_distance = distance;
                best_pivot = j;
            }
        }
        if (best_pivot < 0 || best_pivot >= pivots) {
            sixel_lookup_policy_rbc_float32_clear_state(lut);
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
            distance = sixel_lookup_policy_rbc_float32_distance(
                lut,
                lut->palette + (size_t)i * 3U,
                lut->rbc.pivots[j]);
            if (distance < best_distance) {
                best_distance = distance;
                best_pivot = j;
            }
        }
        if (best_pivot < 0 || best_pivot >= pivots) {
            sixel_lookup_policy_rbc_float32_clear_state(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        cursor = lut->rbc.member_offset[best_pivot]++;
        if (cursor < 0 || cursor >= lut->ncolors) {
            sixel_lookup_policy_rbc_float32_clear_state(lut);
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
            sixel_lookup_policy_rbc_float32_clear_state(lut);
            return SIXEL_RUNTIME_ERROR;
        }
        for (k = start; k < end; ++k) {
            if (k < 0 || k >= lut->ncolors) {
                continue;
            }
            if (lut->rbc.member_index[k] < 0
                    || lut->rbc.member_index[k] >= lut->ncolors) {
                sixel_lookup_policy_rbc_float32_clear_state(lut);
                return SIXEL_RUNTIME_ERROR;
            }
        }
    }

    lut->rbc.pivot_count = pivots;
    lut->rbc.ready = 1;
    return SIXEL_OK;
}

static int
sixel_lookup_policy_rbc_float32_search(sixel_lookup_float32_t const *lut,
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
    float lb2;
    float lb;

    j = 0;
    k = 0;
    start = 0;
    end = 0;
    idx = 0;
    best_index = 0;
    best2 = FLT_MAX;
    dist2 = 0.0f;
    lb2 = 0.0f;
    lb = 0.0f;

    if (lut->rbc.ready == 0) {
        return 0;
    }

    for (j = 0; j < lut->rbc.pivot_count; ++j) {
        dist2 = sixel_lookup_policy_rbc_float32_distance(
            lut,
            sample,
            lut->rbc.pivots[j]);
        lb = sqrtf(dist2) - lut->rbc.radius[j];
        if (lb < 0.0f) {
            lb = 0.0f;
        }
        lb2 = lb * lb;
        if (lb2 >= best2) {
            continue;
        }

        start = lut->rbc.member_offset[j];
        end = lut->rbc.member_offset[j + 1];
        if (start < 0 || end < start || end > lut->ncolors) {
            return 0;
        }
        for (k = start; k < end; ++k) {
            idx = lut->rbc.member_index[k];
            if (idx < 0 || idx >= lut->ncolors) {
                continue;
            }
            dist2 = sixel_lookup_policy_rbc_float32_distance(lut, sample, idx);
            if (dist2 < best2) {
                best2 = dist2;
                best_index = idx;
            }
        }
    }

    return best_index;
}

static SIXELSTATUS
sixel_lookup_policy_rbc_configure_float32(
    sixel_lookup_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
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

    sixel_lookup_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_RBC;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_policy_rbc_float32_prepare_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_lookup_policy_rbc_float32_configure_clusters(lut);
}

static void
sixel_lookup_policy_rbc_reset_state(
    sixel_lookup_policy_rbc_object_t *object)
{
    if (object == NULL) {
        return;
    }

    if (object->owns_lut != 0 && object->lut != NULL) {
        sixel_lut_unref(object->lut);
    }

    object->lut = NULL;
    object->owns_lut = 0;
    object->lookup_source_is_float = 0;
}

static void
sixel_lookup_policy_rbc_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_rbc_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_rbc_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_rbc_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_rbc_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_rbc_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_rbc_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_rbc_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_rbc_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_rbc_object_t *reuse_object;
    int normalized_lut_policy;
    int shared_lut;

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;
    shared_lut = 1;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        status = sixel_lookup_policy_validate_complexion_limit(
            request->depth,
            request->complexion);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    object = sixel_lookup_policy_rbc_from_base(policy);
    sixel_lookup_policy_rbc_reset_state(object);

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        SIXEL_LUT_POLICY_RBC);
    shared_lut = sixel_lookup_policy_shared_cache_enabled(
        normalized_lut_policy);

    reuse_policy = request->reuse_policy;
    if (sixel_lookup_parallel_dither_active() != 0
            /* Reuse slot NULL means shared cache cannot migrate ownership. */
            && shared_lut == 0
            && request->reuse_policy_slot == NULL) {
        reuse_policy = NULL;
    }

    object->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);

    if (reuse_policy != NULL
            && reuse_policy->vtbl == policy->vtbl) {
        reuse_object = sixel_lookup_policy_rbc_from_base(reuse_policy);
        if (reuse_object->lut != NULL) {
            object->lut = reuse_object->lut;
            object->owns_lut = reuse_object->owns_lut;
            object->lookup_source_is_float =
                reuse_object->lookup_source_is_float;
            reuse_object->lut = NULL;
            reuse_object->owns_lut = 0;
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    status = sixel_lut_new(&object->lut,
                           normalized_lut_policy,
                           request->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    object->owns_lut = 1;

    if (object->lookup_source_is_float != 0) {
        status = sixel_lookup_policy_rbc_configure_float32(
            sixel_lut_backend_float32(object->lut),
            request);
    } else {
        status = sixel_lookup_8bit_configure_rbc(
            sixel_lut_backend_8bit(object->lut),
            request->palette,
            request->depth,
            request->reqcolor,
            1,
            1,
            1,
            1,
            request->pixelformat);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_rbc_reset_state(object);
        return status;
    }

    if (request->reuse_policy_slot != NULL
            && *request->reuse_policy_slot == NULL) {
        *request->reuse_policy_slot = policy;
        policy->vtbl->ref(policy);
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_rbc_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_rbc_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_rbc_from_base_const(policy);
    if (object->lut == NULL) {
        return 0;
    }

    if (sixel_lut_uses_float(object->lut) != 0) {
        return sixel_lookup_policy_rbc_float32_search(
            sixel_lut_backend_float32(object->lut),
            (float const *)(void const *)pixel);
    }

    return sixel_lookup_8bit_map_pixel_rbc(
        sixel_lut_backend_8bit(object->lut),
        pixel);
}

static int
sixel_lookup_policy_rbc_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_rbc_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_rbc_from_base_const(policy);
    return object->lookup_source_is_float;
}

static int
sixel_lookup_policy_rbc_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_rbc_vtbl = {
    sixel_lookup_policy_rbc_ref,
    sixel_lookup_policy_rbc_unref,
    sixel_lookup_policy_rbc_prepare,
    sixel_lookup_policy_rbc_map_pixel,
    sixel_lookup_policy_rbc_lookup_source_is_float,
    sixel_lookup_policy_rbc_prefer_palette_float_lookup
};

SIXELSTATUS
sixel_lookup_policy_create_rbc(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_rbc_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_rbc_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_rbc: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_rbc_vtbl;
    object->ref = 1U;

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
