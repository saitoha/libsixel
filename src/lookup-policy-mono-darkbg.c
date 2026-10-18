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

#include "lookup-policy-mono-darkbg.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupMonoDarkBg : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 * }
 */

typedef struct sixel_lookup_policy_mono_darkbg_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int depth;
    int reqcolor;
    double midpoint[3];
    double direction[3];
} sixel_lookup_policy_mono_darkbg_object_t;

static sixel_lookup_policy_mono_darkbg_object_t *
sixel_lookup_policy_mono_darkbg_from_base(
    sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_mono_darkbg_object_t *)(void *)policy;
}

static sixel_lookup_policy_mono_darkbg_object_t const *
sixel_lookup_policy_mono_darkbg_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_mono_darkbg_object_t const *)(void const *)
        policy;
}

static void
sixel_lookup_policy_mono_darkbg_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_mono_darkbg_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_mono_darkbg_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_mono_darkbg_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_mono_darkbg_object_t *object;
    unsigned int previous;
    sixel_allocator_t *allocator;

    object = NULL;
    previous = 0U;
    allocator = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_mono_darkbg_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        allocator = object->allocator;
        object->allocator = NULL;
        if (allocator != NULL) {
            sixel_allocator_free(allocator, object);
            sixel_allocator_unref(allocator);
        }
    }
}

static SIXELSTATUS
sixel_lookup_policy_mono_darkbg_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_mono_darkbg_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_mono_darkbg_from_base(policy);
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_mono_darkbg_prepare_float32(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_mono_darkbg_object_t *object;
    double first;
    double second;
    int stride;
    int n;

    object = NULL;
    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->depth > 3
            || request->reqcolor != 2) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_mono_darkbg_from_base(policy);
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;

    /* A float query uses the prepared coordinate scale, which need not be
     * byte RGB. Copy the two-entry decision plane so reprepare and shared
     * read-only queries never borrow mutable palette state. The byte class
     * retains its legacy threshold; only the float callback uses this. */
    stride = request->float_depth / (int)sizeof(float);
    for (n = 0; n < object->depth; ++n) {
        if (request->palette_float != NULL && stride >= object->depth) {
            first = request->palette_float[n];
            second = request->palette_float[stride + n];
        } else {
            first = request->palette[n];
            second = request->palette[object->depth + n];
        }
        object->midpoint[n] = (first + second) * .5;
        object->direction[n] = second - first;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_mono_darkbg_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_mono_darkbg_object_t const *object;
    int n;
    int distant;

    object = NULL;
    n = 0;
    distant = 0;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_mono_darkbg_from_base_const(policy);
    if (object->depth <= 0 || object->reqcolor <= 0) {
        return 0;
    }

    for (n = 0; n < object->depth; ++n) {
        distant += pixel[n];
    }

    return distant >= 128 * object->reqcolor ? 1 : 0;
}

static int
sixel_lookup_policy_mono_darkbg_map_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_mono_darkbg_object_t const *object;
    float const *values;
    double projection;
    int n;

    if (policy == NULL || pixel == NULL) {
        return 0;
    }
    object = sixel_lookup_policy_mono_darkbg_from_base_const(policy);
    values = (float const *)(void const *)pixel;
    projection = 0;
    /* Difference of squared distances, factored around the midpoint.
     * Double intermediates retain finite float excursions; reading the
     * first bytes of a float instead produces representation-dependent
     * noise. A tie keeps palette entry zero, as in exhaustive lookup. */
    for (n = 0; n < object->depth; ++n) {
        projection += ((double)values[n] - object->midpoint[n]) *
            object->direction[n];
    }
    return projection > 0 ? 1 : 0;
}

/* This policy has no use for a scan hint; the result is map_pixel()'s. */
static int
sixel_lookup_policy_mono_darkbg_map_pixel_hint(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel,
    int hint)
{
    (void)hint;
    return sixel_lookup_policy_mono_darkbg_map_pixel(policy, pixel);
}

/* This policy has no use for a scan hint; the result is map_pixel()'s. */
static int
sixel_lookup_policy_mono_darkbg_map_pixel_hint_float32(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel,
    int hint)
{
    (void)hint;
    return sixel_lookup_policy_mono_darkbg_map_float32(policy, pixel);
}

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_mono_darkbg_8bit_vtbl = {
    sixel_lookup_policy_mono_darkbg_ref,
    sixel_lookup_policy_mono_darkbg_unref,
    sixel_lookup_policy_mono_darkbg_prepare,
    sixel_lookup_policy_mono_darkbg_map_pixel,
    sixel_lookup_policy_mono_darkbg_map_pixel_hint,
    sixel_lookup_policy_map_pixels_default,
    NULL,
};

static sixel_lookup_policy_vtbl_t
    g_sixel_lookup_policy_mono_darkbg_float32_vtbl = {
    sixel_lookup_policy_mono_darkbg_ref,
    sixel_lookup_policy_mono_darkbg_unref,
    sixel_lookup_policy_mono_darkbg_prepare_float32,
    sixel_lookup_policy_mono_darkbg_map_float32,
    sixel_lookup_policy_mono_darkbg_map_pixel_hint_float32,
    sixel_lookup_policy_map_pixels_default,
    NULL,
};

SIXELSTATUS
sixel_lookup_policy_mono_darkbg_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    sixel_lookup_policy_mono_darkbg_object_t *object;

    object = NULL;
    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_lookup_policy_mono_darkbg_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_mono_darkbg_8bit_new: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_mono_darkbg_8bit_vtbl;
    object->ref = 1U;
    object->allocator = allocator;
    sixel_allocator_ref(allocator);
    *policy = &object->base;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_lookup_policy_mono_darkbg_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    sixel_lookup_policy_mono_darkbg_object_t *object;

    object = NULL;
    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_lookup_policy_mono_darkbg_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_mono_darkbg_float32_new: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_mono_darkbg_float32_vtbl;
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
