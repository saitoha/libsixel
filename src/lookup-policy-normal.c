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

#include "lookup-policy-private.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupNormal : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 *
 * Ownership/lifetime:
 * - create_normal() returns refcount=1 objects.
 * - unref() frees the object when refcount reaches zero.
 */

typedef struct sixel_lookup_policy_normal_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    unsigned char const *palette;
    int depth;
    int reqcolor;
    int complexion;
    int prefer_palette_float_lookup;
} sixel_lookup_policy_normal_object_t;

static sixel_lookup_policy_normal_object_t *
sixel_lookup_policy_normal_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_normal_object_t *)(void *)policy;
}

static sixel_lookup_policy_normal_object_t const *
sixel_lookup_policy_normal_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_normal_object_t const *)(void const *)policy;
}

static int
sixel_lookup_policy_normal_map_core(unsigned char const *pixel,
                                    int depth,
                                    unsigned char const *palette,
                                    int reqcolor,
                                    int complexion)
{
    int result;
    int diff;
    int r;
    int i;
    int n;
    int distant;

    result = -1;
    diff = INT_MAX;

    for (i = 0; i < reqcolor; ++i) {
        distant = 0;
        r = pixel[0] - palette[i * depth + 0];
        distant += r * r * complexion;
        for (n = 1; n < depth; ++n) {
            r = pixel[n] - palette[i * depth + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    return result;
}

static void
sixel_lookup_policy_normal_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_normal_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_normal_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_normal_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_normal_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_normal_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_normal_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_normal_object_t *object;

    status = SIXEL_FALSE;
    object = NULL;

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

    object = sixel_lookup_policy_normal_from_base(policy);
    object->palette = request->palette;
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;
    object->complexion = 1;
    object->prefer_palette_float_lookup = 0;

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)
            && request->palette_float != NULL
            && request->float_depth >= request->depth) {
        object->prefer_palette_float_lookup = 1;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_normal_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_normal_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_normal_from_base_const(policy);
    if (object->palette == NULL || object->depth <= 0
            || object->reqcolor <= 0) {
        return 0;
    }

    return sixel_lookup_policy_normal_map_core(pixel,
                                               object->depth,
                                               object->palette,
                                               object->reqcolor,
                                               object->complexion);
}

static int
sixel_lookup_policy_normal_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static int
sixel_lookup_policy_normal_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_normal_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_normal_from_base_const(policy);
    return object->prefer_palette_float_lookup;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_normal_vtbl = {
    sixel_lookup_policy_normal_ref,
    sixel_lookup_policy_normal_unref,
    sixel_lookup_policy_normal_prepare,
    sixel_lookup_policy_normal_map_pixel,
    sixel_lookup_policy_normal_lookup_source_is_float,
    sixel_lookup_policy_normal_prefer_palette_float_lookup
};

SIXELSTATUS
sixel_lookup_policy_create_normal(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_normal_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_normal_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_normal: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_normal_vtbl;
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
