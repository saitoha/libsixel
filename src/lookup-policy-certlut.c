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

#include "lookup-8bit.h"
#include "lookup-common.h"
#include "lookup-float32.h"
#include "lookup-policy-private.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupCertLut : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 */

typedef struct sixel_lookup_policy_certlut_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_lut_t *lut;
    int owns_lut;
    int lookup_source_is_float;
} sixel_lookup_policy_certlut_object_t;

static sixel_lookup_policy_certlut_object_t *
sixel_lookup_policy_certlut_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_certlut_object_t *)(void *)policy;
}

static sixel_lookup_policy_certlut_object_t const *
sixel_lookup_policy_certlut_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_certlut_object_t const *)(void const *)policy;
}

#ifndef SIXEL_LOOKUP_FLOAT32_NODE_DEFINED
#define SIXEL_LOOKUP_FLOAT32_NODE_DEFINED 1
struct sixel_lookup_float32_node {
    int index;
    int left;
    int right;
    int axis;
};
#endif

#ifndef SIXEL_CERTLUT_COLOR_T_DEFINED
#define SIXEL_CERTLUT_COLOR_T_DEFINED 1
typedef struct sixel_certlut_color sixel_certlut_color_t;
#endif

SIXELSTATUS
sixel_certlut_build(sixel_certlut_t *lut,
                    sixel_certlut_color_t const *palette,
                    int ncolors,
                    int wcomp1,
                    int wcomp2,
                    int wcomp3);

uint8_t
sixel_certlut_lookup(sixel_certlut_t *lut,
                     uint8_t comp1,
                     uint8_t comp2,
                     uint8_t comp3);

static void
sixel_lookup_policy_certlut_release_kdtree(sixel_lookup_float32_t *lut)
{
    free(lut->kdnodes);
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

static float
sixel_lookup_policy_certlut_float_component(float const *palette,
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
sixel_lookup_policy_certlut_float_distance(
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
            - sixel_lookup_policy_certlut_float_component(
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
sixel_lookup_policy_certlut_prepare_float_palette(
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
            "sixel_lookup_policy_certlut: "
            "float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_certlut: "
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

static void
sixel_lookup_policy_certlut_sort_indices(float const *palette,
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

    i = 0;
    j = 0;
    key = 0;
    key_value = 0.0f;
    current = 0.0f;
    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_lookup_policy_certlut_float_component(
            palette,
            depth,
            key,
            axis);
        j = i - 1;
        while (j >= 0) {
            current = sixel_lookup_policy_certlut_float_component(
                palette,
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
sixel_lookup_policy_certlut_build_kdtree(sixel_lookup_float32_t *lut,
                                         int *indices,
                                         int count,
                                         int depth)
{
    int axis;
    int median;
    int node_index;

    axis = 0;
    median = 0;
    node_index = 0;
    if (count <= 0) {
        return -1;
    }

    axis = depth % SIXEL_LOOKUP_FLOAT_COMPONENTS;
    sixel_lookup_policy_certlut_sort_indices(lut->palette,
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
        sixel_lookup_policy_certlut_build_kdtree(lut,
                                                 indices,
                                                 median,
                                                 depth + 1);
    lut->kdnodes[node_index].right =
        sixel_lookup_policy_certlut_build_kdtree(lut,
                                                 indices + median + 1,
                                                 count - median - 1,
                                                 depth + 1);

    return node_index;
}

static SIXELSTATUS
sixel_lookup_policy_certlut_prepare_kdtree(sixel_lookup_float32_t *lut)
{
    sixel_lookup_float32_node_t *nodes;
    int *indices;
    SIXELSTATUS status;
    int i;

    nodes = NULL;
    indices = NULL;
    status = SIXEL_FALSE;
    i = 0;
    if (lut == NULL || lut->ncolors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    nodes = (sixel_lookup_float32_node_t *)calloc(
        (size_t)lut->ncolors,
        sizeof(sixel_lookup_float32_node_t));
    if (nodes == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    lut->kdnodes = nodes;

    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        sixel_lookup_policy_certlut_release_kdtree(lut);
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_lookup_policy_certlut_build_kdtree(lut,
                                                                indices,
                                                                lut->ncolors,
                                                                0);
    status = SIXEL_OK;
    if (lut->kdtree_root < 0) {
        status = SIXEL_BAD_ALLOCATION;
    }

    free(indices);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_certlut_release_kdtree(lut);
    }

    return status;
}

static void
sixel_lookup_policy_certlut_search_kdtree(
    sixel_lookup_float32_t const *lut,
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

    node = NULL;
    pivot = 0.0f;
    diff = 0.0f;
    distance = 0.0f;
    plane_distance = 0.0f;
    next = -1;
    other = -1;
    if (node_index < 0) {
        return;
    }

    node = &lut->kdnodes[node_index];
    pivot = sixel_lookup_policy_certlut_float_component(lut->palette,
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

    sixel_lookup_policy_certlut_search_kdtree(lut,
                                              next,
                                              sample,
                                              best_index,
                                              best_distance);

    distance = sixel_lookup_policy_certlut_float_distance(lut,
                                                          sample,
                                                          node->index);
    if (distance < *best_distance) {
        *best_distance = distance;
        *best_index = node->index;
    }

    plane_distance = diff * diff * lut->weights[node->axis];
    if (plane_distance < *best_distance) {
        sixel_lookup_policy_certlut_search_kdtree(lut,
                                                  other,
                                                  sample,
                                                  best_index,
                                                  best_distance);
    }
}

static SIXELSTATUS
sixel_lookup_policy_certlut_configure_float32(
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
    lut->policy = SIXEL_LUT_POLICY_CERTLUT;
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

    status = sixel_lookup_policy_certlut_prepare_float_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_lookup_policy_certlut_prepare_kdtree(lut);
}

static SIXELSTATUS
sixel_lookup_policy_certlut_configure_8bit(
    sixel_lookup_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_CERTLUT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;
    lut->palette = request->palette;
    if (lut->cert == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_certlut: cert buffer missing.");
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_certlut_build(
        lut->cert,
        (sixel_certlut_color_t const *)request->palette,
        request->reqcolor,
        1,
        1,
        1);
    if (SIXEL_FAILED(status)) {
        lut->cert_ready = 0;
        return status;
    }

    lut->cert_ready = 1;
    return SIXEL_OK;
}

static int
sixel_lookup_policy_certlut_map_float32(
    sixel_lookup_float32_t const *lut,
    unsigned char const *pixel)
{
    float const *sample;
    int best_index;
    float best_distance;

    sample = NULL;
    best_index = 0;
    best_distance = FLT_MAX;
    if (lut == NULL || pixel == NULL || lut->kdnodes == NULL) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    sixel_lookup_policy_certlut_search_kdtree(lut,
                                              lut->kdtree_root,
                                              sample,
                                              &best_index,
                                              &best_distance);
    return best_index;
}

static int
sixel_lookup_policy_certlut_map_8bit(
    sixel_lookup_8bit_t const *lut,
    unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL || lut->cert == NULL
            || lut->cert_ready == 0) {
        return 0;
    }

    return (int)sixel_certlut_lookup(lut->cert,
                                     pixel[0],
                                     pixel[1],
                                     pixel[2]);
}

static void
sixel_lookup_policy_certlut_reset_state(
    sixel_lookup_policy_certlut_object_t *object)
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
sixel_lookup_policy_certlut_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_certlut_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_certlut_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_certlut_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_certlut_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_certlut_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_certlut_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_certlut_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_certlut_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_certlut_object_t *reuse_object;
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

    object = sixel_lookup_policy_certlut_from_base(policy);
    sixel_lookup_policy_certlut_reset_state(object);

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        SIXEL_LUT_POLICY_CERTLUT);
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
        reuse_object = sixel_lookup_policy_certlut_from_base(reuse_policy);
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
        status = sixel_lookup_policy_certlut_configure_float32(
            sixel_lut_backend_float32(object->lut),
            request);
    } else {
        status = sixel_lookup_policy_certlut_configure_8bit(
            sixel_lut_backend_8bit(object->lut),
            request);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_certlut_reset_state(object);
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
sixel_lookup_policy_certlut_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_certlut_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_certlut_from_base_const(policy);
    if (object->lut == NULL) {
        return 0;
    }

    if (object->lookup_source_is_float != 0) {
        return sixel_lookup_policy_certlut_map_float32(
            sixel_lut_backend_float32(object->lut),
            pixel);
    }

    return sixel_lookup_policy_certlut_map_8bit(
        sixel_lut_backend_8bit(object->lut),
        pixel);
}

static int
sixel_lookup_policy_certlut_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_certlut_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_certlut_from_base_const(policy);
    return object->lookup_source_is_float;
}

static int
sixel_lookup_policy_certlut_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_certlut_vtbl = {
    sixel_lookup_policy_certlut_ref,
    sixel_lookup_policy_certlut_unref,
    sixel_lookup_policy_certlut_prepare,
    sixel_lookup_policy_certlut_map_pixel,
    sixel_lookup_policy_certlut_lookup_source_is_float,
    sixel_lookup_policy_certlut_prefer_palette_float_lookup
};

SIXELSTATUS
sixel_lookup_policy_create_certlut(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_certlut_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_certlut_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_certlut: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_certlut_vtbl;
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
