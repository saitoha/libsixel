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
#include <stdint.h>
#if HAVE_FLOAT_H
# include <float.h>
#endif

#include "compat_stub.h"
#include "lookup-policy-certlut.h"
#include "pixelformat.h"
#include "sixel_atomic.h"
#include "threading.h"


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

enum { SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS = 3 };
enum { SIXEL_CERTLUT_COMPONENTS = 3 };

#define SIXEL_LUT_BRANCH_FLAG 0x40000000U

typedef struct sixel_certlut sixel_certlut_t;

typedef struct sixel_certlut_color {
    uint8_t comp[SIXEL_CERTLUT_COMPONENTS];
} sixel_certlut_color_t;

typedef struct sixel_certlut_node {
    int index;
    int left;
    int right;
    unsigned char axis;
} sixel_certlut_node_t;

struct sixel_certlut {
    uint32_t *level0;
    uint8_t *pool;
    uint32_t pool_size;
    uint32_t pool_capacity;
    int weights[SIXEL_CERTLUT_COMPONENTS];
    uint64_t weights_sq[SIXEL_CERTLUT_COMPONENTS];
    int32_t scales[SIXEL_CERTLUT_COMPONENTS][256];
    int32_t *weight_palette[SIXEL_CERTLUT_COMPONENTS];
    sixel_certlut_color_t const *palette;
    int ncolors;
    sixel_certlut_node_t *kdnodes;
    int kdnodes_count;
    int kdtree_root;
    sixel_mutex_t lock;
    int lock_ready;
};

typedef struct sixel_lookup_policy_certlut_float32_node {
    int index;
    int left;
    int right;
    int axis;
} sixel_lookup_policy_certlut_float32_node_t;

typedef struct sixel_lookup_policy_certlut_8bit {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    sixel_certlut_t *cert;
    int cert_ready;
} sixel_lookup_policy_certlut_8bit_t;

typedef struct sixel_lookup_policy_certlut_float32 {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    float weights[SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS];
    float *palette;
    sixel_lookup_policy_certlut_float32_node_t *kdnodes;
    int kdtree_root;
    int kdnodes_count;
    sixel_allocator_t *allocator;
} sixel_lookup_policy_certlut_float32_t;

static void
sixel_lookup_policy_certlut_release_kdtree(sixel_lookup_policy_certlut_float32_t *lut);

static void
sixel_certlut_free(sixel_certlut_t *lut);

static void
sixel_certlut_disable_locking(sixel_certlut_t *lut);

static void
sixel_lookup_policy_certlut_8bit_init(sixel_lookup_policy_certlut_8bit_t *lut,
                       sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(*lut));
    lut->allocator = allocator;
    lut->complexion = 1;
    lut->cert_ready = 0;
}

static void
sixel_lookup_policy_certlut_8bit_clear(sixel_lookup_policy_certlut_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->cert != NULL) {
        sixel_certlut_free(lut->cert);
    }
    lut->cert_ready = 0;
    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
}

static void
sixel_lookup_policy_certlut_8bit_finalize(sixel_lookup_policy_certlut_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_certlut_8bit_clear(lut);
    if (lut->cert != NULL) {
        sixel_certlut_free(lut->cert);
        free(lut->cert);
        lut->cert = NULL;
    }
    lut->allocator = NULL;
}

static void
sixel_lookup_policy_certlut_float32_init(sixel_lookup_policy_certlut_float32_t *lut,
                          sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(*lut));
    lut->allocator = allocator;
    lut->complexion = 1;
    lut->kdtree_root = -1;
    lut->weights[0] = 1.0f;
    lut->weights[1] = 1.0f;
    lut->weights[2] = 1.0f;
}

static void
sixel_lookup_policy_certlut_float32_clear(sixel_lookup_policy_certlut_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
    sixel_lookup_policy_certlut_release_kdtree(lut);
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
}

static void
sixel_lookup_policy_certlut_float32_finalize(sixel_lookup_policy_certlut_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_certlut_float32_clear(lut);
    lut->allocator = NULL;
}

typedef struct sixel_lookup_policy_certlut_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int backend_initialized;
    int prepared;
    sixel_lookup_policy_certlut_8bit_t state_8bit;
    sixel_lookup_policy_certlut_float32_t state_float;
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

static int
sixel_lookup_policy_certlut_parse_shared_default_off(void)
{
    char const *env;

    env = sixel_compat_getenv("SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE");
    if (env == NULL || env[0] == '\0') {
        return 0;
    }
    if (env[0] == '0' && env[1] == '\0') {
        return 0;
    }
    if (env[0] == '1' && env[1] == '\0') {
        return 1;
    }

    return 0;
}

int
sixel_lookup_policy_certlut_shared_instance_enabled(void)
{
    static int cached = -1;

    if (cached < 0) {
        cached = sixel_lookup_policy_certlut_parse_shared_default_off();
    }

    return cached;
}

static SIXELSTATUS
sixel_certlut_build(sixel_certlut_t *lut,
                    sixel_certlut_color_t const *palette,
                    int ncolors,
                    int wcomp1,
                    int wcomp2,
                    int wcomp3);

static uint8_t
sixel_certlut_lookup(sixel_certlut_t *lut,
                     uint8_t comp1,
                     uint8_t comp2,
                     uint8_t comp3);

static void
sixel_lookup_policy_certlut_release_kdtree(sixel_lookup_policy_certlut_float32_t *lut)
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
    } else if (clamped_axis >= SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS) {
        clamped_axis = SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS - 1;
    }

    return palette[index * depth + clamped_axis];
}

static float
sixel_lookup_policy_certlut_float_distance(
    sixel_lookup_policy_certlut_float32_t const *lut,
    float const *sample,
    int palette_index)
{
    float diff;
    float distance;
    int component;

    diff = 0.0f;
    distance = 0.0f;
    component = 0;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS;
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
    sixel_lookup_policy_certlut_float32_t *lut,
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
sixel_lookup_policy_certlut_build_kdtree(sixel_lookup_policy_certlut_float32_t *lut,
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

    axis = depth % SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS;
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
sixel_lookup_policy_certlut_prepare_kdtree(sixel_lookup_policy_certlut_float32_t *lut)
{
    sixel_lookup_policy_certlut_float32_node_t *nodes;
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

    nodes = (sixel_lookup_policy_certlut_float32_node_t *)calloc(
        (size_t)lut->ncolors,
        sizeof(sixel_lookup_policy_certlut_float32_node_t));
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
    sixel_lookup_policy_certlut_float32_t const *lut,
    int node_index,
    float const *sample,
    int *best_index,
    float *best_distance)
{
    sixel_lookup_policy_certlut_float32_node_t const *node;
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
    sixel_lookup_policy_certlut_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS];
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

    sixel_lookup_policy_certlut_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_CERTLUT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_CERTLUT_FLOAT_COMPONENTS;
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
    sixel_lookup_policy_certlut_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_certlut_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_CERTLUT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;
    lut->palette = request->palette;
    if (lut->cert == NULL) {
        lut->cert = (sixel_certlut_t *)calloc(1, sizeof(*lut->cert));
        if (lut->cert == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_certlut: cert allocation failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (sixel_lookup_policy_certlut_shared_instance_enabled() == 0) {
        sixel_certlut_disable_locking(lut->cert);
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
    sixel_lookup_policy_certlut_float32_t const *lut,
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
    sixel_lookup_policy_certlut_8bit_t const *lut,
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

    if (object->backend_initialized != 0) {
        sixel_lookup_policy_certlut_8bit_finalize(&object->state_8bit);
        sixel_lookup_policy_certlut_float32_finalize(&object->state_float);
    }

    memset(&object->state_8bit, 0, sizeof(object->state_8bit));
    memset(&object->state_float, 0, sizeof(object->state_float));
    object->backend_initialized = 0;
    object->prepared = 0;
    object->lookup_source_is_float = 0;
}

static void
sixel_lookup_policy_certlut_detach_state(
    sixel_lookup_policy_certlut_object_t *object)
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

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_certlut_from_base(policy);
    sixel_lookup_policy_certlut_reset_state(object);
    object->backend_initialized = 1;
    sixel_lookup_policy_certlut_8bit_init(&object->state_8bit, request->allocator);
    sixel_lookup_policy_certlut_float32_init(&object->state_float, request->allocator);

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
        reuse_object = sixel_lookup_policy_certlut_from_base(reuse_policy);
        if (reuse_object->prepared != 0
                && reuse_object->lookup_source_is_float
                == object->lookup_source_is_float) {
            sixel_lookup_policy_certlut_reset_state(object);
            object->state_8bit = reuse_object->state_8bit;
            object->state_float = reuse_object->state_float;
            object->backend_initialized = reuse_object->backend_initialized;
            object->prepared = reuse_object->prepared;
            object->lookup_source_is_float =
                reuse_object->lookup_source_is_float;
            sixel_lookup_policy_certlut_detach_state(reuse_object);
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    if (object->lookup_source_is_float != 0) {
        status = sixel_lookup_policy_certlut_configure_float32(
            &object->state_float,
            request);
    } else {
        status = sixel_lookup_policy_certlut_configure_8bit(
            &object->state_8bit,
            request);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_certlut_reset_state(object);
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
    if (object->prepared == 0) {
        return 0;
    }

    if (object->lookup_source_is_float != 0) {
        return sixel_lookup_policy_certlut_map_float32(
            &object->state_float,
            pixel);
    }

    return sixel_lookup_policy_certlut_map_8bit(
        &object->state_8bit,
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

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
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
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

/* CERTLUT backend moved from lookup-8bit.c */
static int
sixel_certlut_init(sixel_certlut_t *lut)
{
    int status;
    int component;

    status = SIXEL_FALSE;
    if (lut == NULL) {
        goto end;
    }

    lut->lock_ready = 0;
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        lut->weights[component] = 1;
        lut->weights_sq[component] = 1U;
        memset(lut->scales[component], 0,
               sizeof(lut->scales[component]));
        lut->weight_palette[component] = NULL;
    }
    lut->palette = NULL;
    lut->ncolors = 0;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;

#if SIXEL_ENABLE_THREADS
    status = sixel_mutex_init(&lut->lock);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    lut->lock_ready = 1;
#endif

    /*
     * Single-threaded builds intentionally skip mutex initialization.
     * CERTLUT still works because lock/unlock helpers already guard on
     * lock_ready and become no-ops when synchronization is unnecessary.
     */
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_certlut_release(sixel_certlut_t *lut)
{
    int component;

    if (lut == NULL) {
        return;
    }
    free(lut->level0);
    free(lut->pool);
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        free(lut->weight_palette[component]);
        lut->weight_palette[component] = NULL;
    }
    free(lut->kdnodes);
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    if (lut->lock_ready != 0) {
        sixel_mutex_destroy(&lut->lock);
        lut->lock_ready = 0;
    }
}

static int
sixel_certlut_prepare_palette_terms(sixel_certlut_t *lut)
{
    int status;
    size_t count;
    int index;
    int component;
    int32_t *terms[SIXEL_CERTLUT_COMPONENTS];

    status = SIXEL_FALSE;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        terms[component] = NULL;
    }
    if (lut == NULL) {
        goto end;
    }
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    count = (size_t)lut->ncolors;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        terms[component] = (int32_t *)malloc(count * sizeof(int32_t));
        if (terms[component] == NULL) {
            goto end;
        }
    }
    for (index = 0; index < lut->ncolors; ++index) {
        for (component = 0; component < SIXEL_CERTLUT_COMPONENTS;
                ++component) {
            terms[component][index]
                = lut->weights[component]
                * (int)lut->palette[index].comp[component];
        }
    }
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        free(lut->weight_palette[component]);
        lut->weight_palette[component] = terms[component];
        terms[component] = NULL;
    }
    status = SIXEL_OK;

end:
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        free(terms[component]);
    }
    return status;
}

static void
sixel_certlut_cell_center(int comp1_min,
                          int comp2_min,
                          int comp3_min,
                          int size,
                          int *comp1_center,
                          int *comp2_center,
                          int *comp3_center)
{
    int half;

    half = size / 2;
    *comp1_center = comp1_min + half;
    *comp2_center = comp2_min + half;
    *comp3_center = comp3_min + half;
    if (size == 1) {
        *comp1_center = comp1_min;
        *comp2_center = comp2_min;
        *comp3_center = comp3_min;
    }
}

static void
sixel_certlut_weight_init(sixel_certlut_t *lut,
                          int comp1_weight,
                          int comp2_weight,
                          int comp3_weight)
{
    int component;
    int i;
    int input[SIXEL_CERTLUT_COMPONENTS];

    input[0] = comp1_weight;
    input[1] = comp2_weight;
    input[2] = comp3_weight;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        lut->weights[component] = input[component];
        lut->weights_sq[component]
            = (uint64_t)input[component] * (uint64_t)input[component];
        for (i = 0; i < 256; ++i) {
            lut->scales[component][i] = input[component] * i;
        }
    }
}

static uint64_t
sixel_certlut_distance_precomputed(sixel_certlut_t const *lut,
                                   int index,
                                   int32_t const scaled_components[])
{
    uint64_t distance;
    int64_t diff;
    int component;

    distance = 0U;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        diff = (int64_t)scaled_components[component]
             - (int64_t)lut->weight_palette[component][index];
        distance += (uint64_t)(diff * diff);
    }

    return distance;
}

static int
sixel_certlut_is_cell_safe(sixel_certlut_t const *lut, int best_idx,
                         int second_idx, int size, uint64_t best_dist,
                         uint64_t second_dist)
{
    uint64_t delta_sq;
    uint64_t rhs;
    uint64_t weight_term;
    int64_t delta;
    int component;

    if (best_idx < 0 || second_idx < 0) {
        return 1;
    }

    /*
     * The certification bound compares the squared distance gap against the
     * palette separation scaled by the cube diameter.  If the gap wins the
     * entire cube maps to the current best palette entry.
     */
    delta_sq = second_dist - best_dist;
    weight_term = 0U;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        delta = (int64_t)lut->weight_palette[component][second_idx]
              - (int64_t)lut->weight_palette[component][best_idx];
        weight_term += (uint64_t)(delta * delta);
    }
    rhs = (uint64_t)SIXEL_CERTLUT_COMPONENTS
        * (uint64_t)size * (uint64_t)size * weight_term;

    return delta_sq * delta_sq > rhs;
}

static void
sixel_certlut_lock(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    if (lut->lock_ready != 0) {
        sixel_mutex_lock(&lut->lock);
    }
}

static void
sixel_certlut_unlock(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    if (lut->lock_ready != 0) {
        sixel_mutex_unlock(&lut->lock);
    }
}

static void
sixel_certlut_disable_locking(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    if (lut->lock_ready == 0) {
        return;
    }

    /*
     * Thread-local CERTLUT instances never contend for the shared pool, so
     * the mutex only adds overhead.  Tear it down to skip lock/unlock in the
     * lookup hot path.
     */
    sixel_mutex_destroy(&lut->lock);
    lut->lock_ready = 0;
}

static uint32_t
sixel_certlut_pool_alloc(sixel_certlut_t *lut, int *status)
{
    uint32_t required;
    uint32_t next_capacity;
    uint32_t offset;
    uint8_t *resized;

    offset = 0U;
    if (status != NULL) {
        *status = SIXEL_FALSE;
    }
    required = lut->pool_size + (uint32_t)(8 * sizeof(uint32_t));
    if (required > lut->pool_capacity) {
        next_capacity = lut->pool_capacity;
        if (next_capacity == 0U) {
            next_capacity = (uint32_t)(8 * sizeof(uint32_t));
        }
        while (next_capacity < required) {
            if (next_capacity > UINT32_MAX / 2U) {
                return 0U;
            }
            next_capacity *= 2U;
        }
        resized = (uint8_t *)realloc(lut->pool, next_capacity);
        if (resized == NULL) {
            return 0U;
        }
        lut->pool = resized;
        lut->pool_capacity = next_capacity;
    }
    offset = lut->pool_size;
    if (lut->pool == NULL) {
        return 0U;
    }
    memset(lut->pool + offset, 0, 8 * sizeof(uint32_t));
    lut->pool_size = required;
    if (status != NULL) {
        *status = SIXEL_OK;
    }

    return offset;
}

static void
sixel_certlut_assign_leaf(uint32_t *cell, int palette_index)
{
    *cell = 0x80000000U | (uint32_t)(palette_index & 0xff);
}

static void
sixel_certlut_assign_branch(uint32_t *cell, uint32_t offset)
{
    *cell = SIXEL_LUT_BRANCH_FLAG | (offset & 0x3fffffffU);
}

static int
sixel_certlut_palette_component(sixel_certlut_t const *lut,
                                int index, int axis)
{
    sixel_certlut_color_t const *color;

    color = &lut->palette[index];
    if (axis < 0) {
        axis = 0;
    } else if (axis >= SIXEL_CERTLUT_COMPONENTS) {
        axis = SIXEL_CERTLUT_COMPONENTS - 1;
    }

    return (int)color->comp[axis];
}

static void
sixel_certlut_sort_indices(sixel_certlut_t const *lut,
                           int *indices, int count, int axis)
{
    int i;
    int j;
    int key;
    int key_value;
    int current_value;

    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_certlut_palette_component(lut, key, axis);
        j = i - 1;
        while (j >= 0) {
            current_value = sixel_certlut_palette_component(lut,
                                                            indices[j],
                                                            axis);
            if (current_value <= key_value) {
                break;
            }
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int
sixel_certlut_kdtree_build_recursive(sixel_certlut_t *lut,
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

    axis = depth % SIXEL_CERTLUT_COMPONENTS;
    sixel_certlut_sort_indices(lut, indices, count, axis);
    median = count / 2;
    node_index = lut->kdnodes_count;
    if (node_index >= lut->ncolors) {
        return -1;
    }
    lut->kdnodes_count++;
    lut->kdnodes[node_index].index = indices[median];
    lut->kdnodes[node_index].axis = (unsigned char)axis;
    lut->kdnodes[node_index].left =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices,
                                             median,
                                             depth + 1);
    lut->kdnodes[node_index].right =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices + median + 1,
                                             count - median - 1,
                                             depth + 1);

    return node_index;
}

static SIXELSTATUS
sixel_certlut_kdtree_build(sixel_certlut_t *lut)
{
    SIXELSTATUS status;
    int *indices;
    int i;

    status = SIXEL_FALSE;
    indices = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    lut->kdnodes = (sixel_certlut_node_t *)
        calloc((size_t)lut->ncolors, sizeof(sixel_certlut_node_t));
    if (lut->kdnodes == NULL) {
        goto end;
    }
    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        goto end;
    }
    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_certlut_kdtree_build_recursive(lut,
                                                            indices,
                                                            lut->ncolors,
                                                            0);
    if (lut->kdtree_root < 0) {
        goto end;
    }
    status = SIXEL_OK;

end:
    free(indices);
    if (SIXEL_FAILED(status)) {
        free(lut->kdnodes);
        lut->kdnodes = NULL;
        lut->kdnodes_count = 0;
        lut->kdtree_root = -1;
    }

    return status;
}

static uint64_t
sixel_certlut_axis_distance(sixel_certlut_t const *lut, int diff, int axis)
{
    uint64_t weight;
    uint64_t abs_diff;

    abs_diff = (uint64_t)(diff < 0 ? -diff : diff);
    if (axis < 0) {
        axis = 0;
    } else if (axis >= SIXEL_CERTLUT_COMPONENTS) {
        axis = SIXEL_CERTLUT_COMPONENTS - 1;
    }
    weight = lut->weights_sq[axis];

    return weight * abs_diff * abs_diff;
}

static void
sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                 int candidate,
                                 int32_t const scaled_components[],
                                 int *best_idx,
                                 uint64_t *best_dist,
                                 int *second_idx,
                                 uint64_t *second_dist)
{
    uint64_t distance;

    distance = sixel_certlut_distance_precomputed(lut,
                                                  candidate,
                                                  scaled_components);
    if (distance < *best_dist) {
        *second_dist = *best_dist;
        *second_idx = *best_idx;
        *best_dist = distance;
        *best_idx = candidate;
    } else if (distance < *second_dist) {
        *second_dist = distance;
        *second_idx = candidate;
    }
}

static void
sixel_certlut_kdtree_search(sixel_certlut_t const *lut,
                            int node_index,
                            int const components[],
                            int32_t const scaled_components[],
                            int *best_idx,
                            uint64_t *best_dist,
                            int *second_idx,
                            uint64_t *second_dist)
{
    sixel_certlut_node_t const *node;
    int axis;
    int value;
    int diff;
    int near_child;
    int far_child;
    uint64_t axis_bound;
    int component_value;

    if (node_index < 0) {
        return;
    }
    node = &lut->kdnodes[node_index];
    sixel_certlut_consider_candidate(lut,
                                     node->index,
                                     scaled_components,
                                     best_idx,
                                     best_dist,
                                     second_idx,
                                     second_dist);

    axis = (int)node->axis;
    value = sixel_certlut_palette_component(lut, node->index, axis);
    component_value = components[axis];
    diff = component_value - value;
    if (diff <= 0) {
        near_child = node->left;
        far_child = node->right;
    } else {
        near_child = node->right;
        far_child = node->left;
    }
    if (near_child >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    near_child,
                                    components,
                                    scaled_components,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
    axis_bound = sixel_certlut_axis_distance(lut, diff, axis);
    if (far_child >= 0 && axis_bound <= *second_dist) {
        sixel_certlut_kdtree_search(lut,
                                    far_child,
                                    components,
                                    scaled_components,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
}

static void
sixel_certlut_distance_pair(sixel_certlut_t const *lut,
                            int comp1,
                            int comp2,
                            int comp3,
                            int *best_idx,
                            int *second_idx,
                            uint64_t *best_dist,
                            uint64_t *second_dist)
{
    int i;
    int best_candidate;
    int second_candidate;
    uint64_t best_value;
    uint64_t second_value;
    uint64_t distance;
    int components[SIXEL_CERTLUT_COMPONENTS];
    int clamped[SIXEL_CERTLUT_COMPONENTS];
    int component;
    int32_t scaled[SIXEL_CERTLUT_COMPONENTS];

    best_candidate = (-1);
    second_candidate = (-1);
    best_value = UINT64_MAX;
    second_value = UINT64_MAX;
    components[0] = comp1;
    components[1] = comp2;
    components[2] = comp3;
    for (component = 0; component < SIXEL_CERTLUT_COMPONENTS; ++component) {
        clamped[component] = components[component];
        if (clamped[component] < 0) {
            clamped[component] = 0;
        } else if (clamped[component] > 255) {
            clamped[component] = 255;
        }
        scaled[component]
            = lut->scales[component][clamped[component]];
    }
    if (lut->kdnodes != NULL && lut->kdtree_root >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    lut->kdtree_root,
                                    components,
                                    scaled,
                                    &best_candidate,
                                    &best_value,
                                    &second_candidate,
                                    &second_value);
    } else {
        for (i = 0; i < lut->ncolors; ++i) {
            distance = sixel_certlut_distance_precomputed(lut,
                                                          i,
                                                          scaled);
            if (distance < best_value) {
                second_value = best_value;
                second_candidate = best_candidate;
                best_value = distance;
                best_candidate = i;
            } else if (distance < second_value) {
                second_value = distance;
                second_candidate = i;
            }
        }
    }
    if (second_candidate < 0) {
        second_candidate = best_candidate;
        second_value = best_value;
    }
    *best_idx = best_candidate;
    *second_idx = second_candidate;
    *best_dist = best_value;
    *second_dist = second_value;
}

static uint8_t
sixel_certlut_fallback(sixel_certlut_t const *lut,
                       int comp1,
                       int comp2,
                       int comp3)
{
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;

    best_idx = -1;
    second_idx = -1;
    best_dist = 0U;
    second_dist = 0U;
    if (lut == NULL) {
        return 0U;
    }
    /*
     * The lazy builder may fail when allocations run out.  Fall back to a
     * direct brute-force palette search so lookups still succeed even in low
     * memory conditions.
     */
    sixel_certlut_distance_pair(lut,
                                comp1,
                                comp2,
                                comp3,
                                &best_idx,
                                &second_idx,
                                &best_dist,
                                &second_dist);
    if (best_idx < 0) {
        return 0U;
    }

    return (uint8_t)best_idx;
}

static SIXELSTATUS
sixel_certlut_build_cell(sixel_certlut_t *lut, uint32_t *cell,
                         int comp1_min,
                         int comp2_min,
                         int comp3_min,
                         int size)
{
    SIXELSTATUS status;
    int center1;
    int center2;
    int center3;
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;
    uint32_t offset;
    int branch_status;
    uint8_t *pool_before;
    size_t pool_size_before;
    uint32_t cell_offset;
    int cell_in_pool;

    if (cell == NULL || lut == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (*cell == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "build_cell comp1=%d comp2=%d comp3=%d size=%d\n",
                comp1_min,
                comp2_min,
                comp3_min,
                size);
#endif
    }
    if (*cell != 0U) {
        status = SIXEL_OK;
        goto end;
    }

    /*
     * Each node represents an axis-aligned cube in component space.  The
     * builder certifies the dominant palette index by checking the distance
     * gap at the cell center.  When certification fails the cube is split into
     * eight octants backed by a pool block.  Children remain unbuilt until
     * lookups descend into them, keeping the workload proportional to actual
     * queries.
     */
    status = SIXEL_FALSE;
    sixel_certlut_cell_center(comp1_min,
                              comp2_min,
                              comp3_min,
                              size,
                              &center1,
                              &center2,
                              &center3);
    sixel_certlut_distance_pair(lut,
                                center1,
                                center2,
                                center3,
                                &best_idx,
                                &second_idx,
                                &best_dist,
                                &second_dist);
    if (best_idx < 0) {
        best_idx = 0;
    }
    if (size == 1) {
        sixel_certlut_assign_leaf(cell, best_idx);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "  leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    if (sixel_certlut_is_cell_safe(lut,
                                   best_idx,
                                   second_idx,
                                   size,
                                   best_dist,
                                   second_dist)) {
        sixel_certlut_assign_leaf(cell, best_idx);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "  safe leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    pool_before = lut->pool;
    pool_size_before = lut->pool_size;
    cell_in_pool = 0;
    cell_offset = 0U;
    /*
     * The pool may grow while building descendants.  Remember the caller's
     * offset so the cell pointer can be refreshed after realloc moves the
     * backing storage.
     */
    if (pool_before != NULL) {
        if ((uint8_t *)(void *)cell >= pool_before
                && (size_t)((uint8_t *)(void *)cell - pool_before)
                        < pool_size_before) {
            cell_in_pool = 1;
            cell_offset = (uint32_t)((uint8_t *)(void *)cell - pool_before);
        }
    }
    offset = sixel_certlut_pool_alloc(lut, &branch_status);
    if (branch_status != SIXEL_OK) {
        goto end;
    }
    if (cell_in_pool != 0) {
        cell = (uint32_t *)(void *)(lut->pool + cell_offset);
    }
    sixel_certlut_assign_branch(cell, offset);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr, "  branch offset=%u\n", offset);
#endif
    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_certlut_build(sixel_certlut_t *lut,
                    sixel_certlut_color_t const *palette,
                    int ncolors,
                    int wcomp1,
                    int wcomp2,
                    int wcomp3)
{
    SIXELSTATUS status;
    int initialized;
    size_t level0_count;
    status = SIXEL_FALSE;
    initialized = sixel_certlut_init(lut);
    if (SIXEL_FAILED(initialized)) {
        goto end;
    }
    lut->palette = palette;
    lut->ncolors = ncolors;
    sixel_certlut_weight_init(lut, wcomp1, wcomp2, wcomp3);
    status = sixel_certlut_prepare_palette_terms(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_certlut_kdtree_build(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    level0_count = (size_t)64 * (size_t)64 * (size_t)64;
    lut->level0 = (uint32_t *)calloc(level0_count, sizeof(uint32_t));
    if (lut->level0 == NULL) {
        goto end;
    }
    /*
     * Level 0 cells start uninitialized.  The lookup routine materializes
     * individual subtrees on demand so we avoid evaluating the entire
     * 64x64x64 grid upfront.
     */
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_certlut_release(lut);
    }
    return status;
}

static uint8_t
sixel_certlut_lookup(sixel_certlut_t *lut,
                     uint8_t comp1,
                     uint8_t comp2,
                     uint8_t comp3)
{
    uint8_t result;
    uint32_t entry;
    uint32_t offset;
    uint32_t index;
    uint32_t *children;
    uint32_t *cell;
    int shift;
    int child;
    int status;
    int size;
    int comp1_min;
    int comp2_min;
    int comp3_min;
    int step;
    int locked;

    result = 0U;
    locked = 0;
    if (lut == NULL || lut->level0 == NULL) {
        return 0U;
    }
    /*
     * Lazy cell materialization reallocates the shared pool.  Serialize the
     * lookup so realloc cannot race with concurrent threads traversing or
     * expanding neighbouring cells.
     */
    sixel_certlut_lock(lut);
    locked = 1;
    /*
     * Cells are created lazily.  A zero entry indicates an uninitialized
     * subtree, so the builder is invoked with the cube bounds of the current
     * traversal.  Should allocation fail we fall back to a direct brute-force
     * palette search for the queried pixel.
     */
    index = ((uint32_t)(comp1 >> 2) << 12)
          | ((uint32_t)(comp2 >> 2) << 6)
          | (uint32_t)(comp3 >> 2);
    cell = lut->level0 + index;
    size = 4;
    comp1_min = (int)(comp1 & 0xfc);
    comp2_min = (int)(comp2 & 0xfc);
    comp3_min = (int)(comp3 & 0xfc);
    entry = *cell;
    if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "lookup build level0 comp1=%u comp2=%u comp3=%u\n",
                (unsigned int)comp1,
                (unsigned int)comp2,
                (unsigned int)comp3);
#endif
        status = sixel_certlut_build_cell(lut,
                                          cell,
                                          comp1_min,
                                          comp2_min,
                                          comp3_min,
                                          size);
        if (SIXEL_FAILED(status)) {
            result = sixel_certlut_fallback(lut,
                                            (int)comp1,
                                            (int)comp2,
                                            (int)comp3);
            goto end;
        }
        entry = *cell;
    }
    shift = 1;
    while ((entry & 0x80000000U) == 0U) {
        offset = entry & 0x3fffffffU;
        children = (uint32_t *)(void *)(lut->pool + offset);
        child = (((int)(comp1 >> shift) & 1) << 2)
              | (((int)(comp2 >> shift) & 1) << 1)
              | ((int)(comp3 >> shift) & 1);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "descend child=%d size=%d offset=%u\n",
                child,
                size,
                offset);
#endif
        step = size / 2;
        if (step <= 0) {
            step = 1;
        }
        comp1_min += step * ((child >> 2) & 1);
        comp2_min += step * ((child >> 1) & 1);
        comp3_min += step * (child & 1);
        size = step;
        cell = children + (size_t)child;
        entry = *cell;
        if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
            fprintf(stderr,
                    "lookup build child size=%d comp1=%d comp2=%d comp3=%d\n",
                    size,
                    comp1_min,
                    comp2_min,
                    comp3_min);
#endif
            status = sixel_certlut_build_cell(lut,
                                              cell,
                                              comp1_min,
                                              comp2_min,
                                              comp3_min,
                                              size);
            if (SIXEL_FAILED(status)) {
                result = sixel_certlut_fallback(lut,
                                                (int)comp1,
                                                (int)comp2,
                                                (int)comp3);
                goto end;
            }
            children = (uint32_t *)(void *)(lut->pool + offset);
            cell = children + (size_t)child;
            entry = *cell;
        }
        if (size == 1) {
            break;
        }
        if (shift == 0) {
            break;
        }
        --shift;
    }

    result = (uint8_t)(entry & 0xffU);

end:
    if (locked != 0) {
        sixel_certlut_unlock(lut);
    }

    return result;
}

static void
sixel_certlut_free(sixel_certlut_t *lut)
{
    sixel_certlut_release(lut);
    if (lut != NULL) {
        lut->palette = NULL;
        lut->ncolors = 0;
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
