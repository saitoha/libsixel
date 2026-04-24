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

#include "lookup-common.h"
#include "lookup-policy-private.h"
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

    status = sixel_lut_configure(object->lut,
                                 request->palette,
                                 request->palette_float,
                                 request->depth,
                                 request->float_depth,
                                 request->reqcolor,
                                 1,
                                 1,
                                 1,
                                 1,
                                 normalized_lut_policy,
                                 request->pixelformat);
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

    return sixel_lut_map_pixel(object->lut, pixel);
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
