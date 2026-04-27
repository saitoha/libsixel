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

#include "lookup-policy-vptree.h"
#include "lookup-vptree-8bit.h"
#include "lookup-vptree-float32.h"
#include "pixelformat.h"
#include "sixel_atomic.h"


/*
 * IDL (internal contract)
 *
 * class LookupVPTree : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 * }
 */

enum { SIXEL_LOOKUP_POLICY_VPTREE_FLOAT_COMPONENTS = 3 };

typedef struct sixel_lookup_policy_vptree_8bit {
    int policy;
    int depth;
    int ncolors;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    sixel_lookup_vptree_8bit_t *vptree;
    int vptree_ready;
} sixel_lookup_policy_vptree_8bit_t;

typedef struct sixel_lookup_policy_vptree_float32 {
    int policy;
    int depth;
    int ncolors;
    float weights[SIXEL_LOOKUP_POLICY_VPTREE_FLOAT_COMPONENTS];
    float *palette;
    sixel_allocator_t *allocator;
    sixel_lookup_vptree_float32_t *vptree;
    int vptree_ready;
} sixel_lookup_policy_vptree_float32_t;

static void
sixel_lookup_policy_vptree_8bit_init(sixel_lookup_policy_vptree_8bit_t *lut,
                       sixel_allocator_t *allocator)
{
    if (lut == NULL) {
        return;
    }

    memset(lut, 0, sizeof(*lut));
    lut->allocator = allocator;
}

static void
sixel_lookup_policy_vptree_8bit_clear(sixel_lookup_policy_vptree_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->vptree != NULL) {
        sixel_lookup_vptree_8bit_unref(lut->vptree);
        lut->vptree = NULL;
    }
    lut->vptree_ready = 0;
    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
}

static void
sixel_lookup_policy_vptree_8bit_finalize(sixel_lookup_policy_vptree_8bit_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_vptree_8bit_clear(lut);
    lut->allocator = NULL;
}

static void
sixel_lookup_policy_vptree_float32_init(sixel_lookup_policy_vptree_float32_t *lut,
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
sixel_lookup_policy_vptree_float32_clear(sixel_lookup_policy_vptree_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->palette != NULL) {
        sixel_allocator_free(lut->allocator, lut->palette);
        lut->palette = NULL;
    }
    if (lut->vptree != NULL) {
        sixel_lookup_vptree_float32_unref(lut->vptree);
        lut->vptree = NULL;
    }
    lut->vptree_ready = 0;
    lut->depth = 0;
    lut->ncolors = 0;
}

static void
sixel_lookup_policy_vptree_float32_finalize(sixel_lookup_policy_vptree_float32_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_policy_vptree_float32_clear(lut);
    lut->allocator = NULL;
}

typedef struct sixel_lookup_policy_vptree_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int backend_initialized;
    int prepared;
    sixel_lookup_policy_vptree_8bit_t state_8bit;
    sixel_lookup_policy_vptree_float32_t state_float;
    int lookup_source_is_float;
} sixel_lookup_policy_vptree_object_t;

static sixel_lookup_policy_vptree_object_t *
sixel_lookup_policy_vptree_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_vptree_object_t *)(void *)policy;
}

static sixel_lookup_policy_vptree_object_t const *
sixel_lookup_policy_vptree_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_vptree_object_t const *)(void const *)policy;
}

static SIXELSTATUS
sixel_lookup_policy_vptree_prepare_float_palette(
    sixel_lookup_policy_vptree_float32_t *lut,
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
            "sixel_lookup_policy_vptree: float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_vptree: float palette depth mismatch.");
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
sixel_lookup_policy_vptree_configure_float32(
    sixel_lookup_policy_vptree_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_POLICY_VPTREE_FLOAT_COMPONENTS];
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

    sixel_lookup_policy_vptree_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_VPTREE;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_POLICY_VPTREE_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_policy_vptree_prepare_float_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (lut->vptree == NULL) {
        status = sixel_lookup_vptree_float32_create(lut->allocator,
                                                    &lut->vptree);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_vptree: VP-tree allocation failed.");
            return status;
        }
    }

    status = sixel_lookup_vptree_float32_configure(lut->vptree,
                                                   lut->palette,
                                                   lut->ncolors,
                                                   lut->depth,
                                                   lut->weights,
                                                   request->parallel_dither_active);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_vptree: VP-tree configure failed.");
        return status;
    }

    lut->vptree_ready = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_vptree_configure_8bit(
    sixel_lookup_policy_vptree_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_vptree_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_VPTREE;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->palette = request->palette;

    if (lut->vptree == NULL) {
        status = sixel_lookup_vptree_8bit_create(lut->allocator, &lut->vptree);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_vptree: VP-tree allocation failed.");
            return status;
        }
    }

    status = sixel_lookup_vptree_8bit_configure(lut->vptree,
                                                request->palette,
                                                request->reqcolor,
                                                request->depth,
                                                request->parallel_dither_active);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_vptree: VP-tree configure failed.");
        return status;
    }

    lut->vptree_ready = 1;
    return SIXEL_OK;
}

static int
sixel_lookup_policy_vptree_map_float32(
    sixel_lookup_policy_vptree_float32_t const *lut,
    unsigned char const *pixel)
{
    float const *sample;

    sample = NULL;
    if (lut == NULL || pixel == NULL || lut->vptree_ready == 0
            || lut->vptree == NULL) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    return sixel_lookup_vptree_float32_map(lut->vptree, sample);
}

static int
sixel_lookup_policy_vptree_map_8bit(sixel_lookup_policy_vptree_8bit_t const *lut,
                                    unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL || lut->vptree_ready == 0
            || lut->vptree == NULL) {
        return 0;
    }

    return sixel_lookup_vptree_8bit_map(lut->vptree, pixel);
}

static void
sixel_lookup_policy_vptree_reset_state(
    sixel_lookup_policy_vptree_object_t *object)
{
    if (object == NULL) {
        return;
    }

    if (object->backend_initialized != 0) {
        sixel_lookup_policy_vptree_8bit_finalize(&object->state_8bit);
        sixel_lookup_policy_vptree_float32_finalize(&object->state_float);
    }

    memset(&object->state_8bit, 0, sizeof(object->state_8bit));
    memset(&object->state_float, 0, sizeof(object->state_float));
    object->backend_initialized = 0;
    object->prepared = 0;
    object->lookup_source_is_float = 0;
}

static void
sixel_lookup_policy_vptree_detach_state(
    sixel_lookup_policy_vptree_object_t *object)
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
sixel_lookup_policy_vptree_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_vptree_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_vptree_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_vptree_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_vptree_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_vptree_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_vptree_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_vptree_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_vptree_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_vptree_object_t *reuse_object;

    status = SIXEL_FALSE;
    object = NULL;
    reuse_policy = NULL;
    reuse_object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0
            || request->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_vptree_from_base(policy);
    sixel_lookup_policy_vptree_reset_state(object);
    object->backend_initialized = 1;
    sixel_lookup_policy_vptree_8bit_init(&object->state_8bit, request->allocator);
    sixel_lookup_policy_vptree_float32_init(&object->state_float, request->allocator);

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
        reuse_object = sixel_lookup_policy_vptree_from_base(reuse_policy);
        if (reuse_object->prepared != 0
                && reuse_object->lookup_source_is_float
                == object->lookup_source_is_float) {
            sixel_lookup_policy_vptree_reset_state(object);
            object->state_8bit = reuse_object->state_8bit;
            object->state_float = reuse_object->state_float;
            object->backend_initialized = reuse_object->backend_initialized;
            object->prepared = reuse_object->prepared;
            object->lookup_source_is_float =
                reuse_object->lookup_source_is_float;
            sixel_lookup_policy_vptree_detach_state(reuse_object);
            if (request->reuse_policy_slot != NULL
                    && *request->reuse_policy_slot == NULL) {
                *request->reuse_policy_slot = policy;
                policy->vtbl->ref(policy);
            }
            return SIXEL_OK;
        }
    }

    if (object->lookup_source_is_float != 0) {
        status = sixel_lookup_policy_vptree_configure_float32(
            &object->state_float,
            request);
    } else {
        status = sixel_lookup_policy_vptree_configure_8bit(
            &object->state_8bit,
            request);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_vptree_reset_state(object);
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
sixel_lookup_policy_vptree_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_vptree_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_vptree_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    if (object->lookup_source_is_float != 0) {
        return sixel_lookup_policy_vptree_map_float32(
            &object->state_float,
            pixel);
    }

    return sixel_lookup_policy_vptree_map_8bit(
        &object->state_8bit,
        pixel);
}

static int
sixel_lookup_policy_vptree_map_pixel_8bit(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_vptree_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_vptree_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    return sixel_lookup_policy_vptree_map_8bit(
        &object->state_8bit,
        pixel);
}

static int
sixel_lookup_policy_vptree_map_pixel_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_vptree_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_vptree_from_base_const(policy);
    if (object->prepared == 0) {
        return 0;
    }

    return sixel_lookup_policy_vptree_map_float32(
        &object->state_float,
        pixel);
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_vptree_vtbl = {
    sixel_lookup_policy_vptree_ref,
    sixel_lookup_policy_vptree_unref,
    sixel_lookup_policy_vptree_prepare,
    sixel_lookup_policy_vptree_map_pixel,
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_lookup_policy_create_vptree(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_vptree_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_vptree_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_vptree: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_vptree_vtbl;
    object->ref = 1U;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif



static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_vptree_8bit_vtbl = {
    sixel_lookup_policy_vptree_ref,
    sixel_lookup_policy_vptree_unref,
    sixel_lookup_policy_vptree_prepare,
    sixel_lookup_policy_vptree_map_pixel_8bit,
};

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_vptree_float32_vtbl = {
    sixel_lookup_policy_vptree_ref,
    sixel_lookup_policy_vptree_unref,
    sixel_lookup_policy_vptree_prepare,
    sixel_lookup_policy_vptree_map_pixel_float32,
};

SIXELSTATUS
sixel_lookup_policy_create_vptree_8bit(
    sixel_lookup_policy_interface_t **policy)
{
    SIXELSTATUS status;

    status = sixel_lookup_policy_create_vptree(policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        (*policy)->vtbl = &g_sixel_lookup_policy_vptree_8bit_vtbl;
    }

    return status;
}

SIXELSTATUS
sixel_lookup_policy_create_vptree_float32(
    sixel_lookup_policy_interface_t **policy)
{
    SIXELSTATUS status;

    status = sixel_lookup_policy_create_vptree(policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        (*policy)->vtbl = &g_sixel_lookup_policy_vptree_float32_vtbl;
    }

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
