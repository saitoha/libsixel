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

#include "filter-lookup.h"
#include "lookup-policy-private.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupVPTree : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 */

typedef struct sixel_lookup_policy_vptree_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_lut_t *lut;
    int owns_lut;
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

static void
sixel_lookup_policy_vptree_reset_state(
    sixel_lookup_policy_vptree_object_t *object)
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
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t lookup_result;
    sixel_lookup_policy_vptree_object_t *object;
    sixel_lut_t *reuse_lut;
    int normalized_lut_policy;
    int shared_lut;
    int reuse_lut_preconfigured;

    status = SIXEL_FALSE;
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&lookup_result, 0, sizeof(lookup_result));
    object = NULL;
    reuse_lut = NULL;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;
    shared_lut = 1;
    reuse_lut_preconfigured = 0;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
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

    object = sixel_lookup_policy_vptree_from_base(policy);
    sixel_lookup_policy_vptree_reset_state(object);

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        SIXEL_LUT_POLICY_VPTREE);
    shared_lut = sixel_lookup_policy_shared_cache_enabled(
        normalized_lut_policy);

    reuse_lut = request->reuse_lut;
    if (request->parallel_active != 0
            && shared_lut == 0
            && request->reuse_lut_is_shared != 0) {
        reuse_lut = NULL;
    }
    if (reuse_lut != NULL && request->reuse_lut_preconfigured != 0) {
        reuse_lut_preconfigured = 1;
    }

    object->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);

    if (reuse_lut_preconfigured != 0) {
        object->lut = reuse_lut;
        object->owns_lut = 0;
        return SIXEL_OK;
    }

    lookup_config.palette = request->palette;
    lookup_config.palette_float = request->palette_float;
    lookup_config.depth = request->depth;
    lookup_config.float_depth = request->float_depth;
    lookup_config.ncolors = request->reqcolor;
    lookup_config.complexion = request->complexion;
    lookup_config.method_for_largest = request->method_for_largest;
    lookup_config.lut_policy = normalized_lut_policy;
    lookup_config.pixelformat = request->pixelformat;
    lookup_config.reuse_lut = reuse_lut;

    status = sixel_filter_lookup_build(&lookup_config,
                                       request->allocator,
                                       NULL,
                                       &lookup_result);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    object->lut = lookup_result.lut;
    object->owns_lut = lookup_result.owned;

    if (object->owns_lut != 0
            && request->reuse_lut_slot != NULL
            && *request->reuse_lut_slot == NULL) {
        *request->reuse_lut_slot = object->lut;
        object->owns_lut = 0;
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
    if (object->lut == NULL) {
        return 0;
    }

    return sixel_lut_map_pixel(object->lut, pixel);
}

static int
sixel_lookup_policy_vptree_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_vptree_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_vptree_from_base_const(policy);
    return object->lookup_source_is_float;
}

static int
sixel_lookup_policy_vptree_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_vptree_vtbl = {
    sixel_lookup_policy_vptree_ref,
    sixel_lookup_policy_vptree_unref,
    sixel_lookup_policy_vptree_prepare,
    sixel_lookup_policy_vptree_map_pixel,
    sixel_lookup_policy_vptree_lookup_source_is_float,
    sixel_lookup_policy_vptree_prefer_palette_float_lookup
};

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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
