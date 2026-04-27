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

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lookup-policy-none.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupNone : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 * }
 *
 * Ownership/lifetime:
 * - create_none() returns refcount=1 objects.
 * - unref() frees the object when refcount reaches zero.
 */

typedef struct sixel_lookup_policy_none_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    unsigned char const *palette;
    float const *palette_float;
    int depth;
    int float_stride;
    int reqcolor;
    int complexion;
    int lookup_source_is_float;
} sixel_lookup_policy_none_object_t;

static sixel_lookup_policy_none_object_t *
sixel_lookup_policy_none_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_none_object_t *)(void *)policy;
}

static sixel_lookup_policy_none_object_t const *
sixel_lookup_policy_none_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_none_object_t const *)(void const *)policy;
}

static int
sixel_lookup_policy_none_map_core(unsigned char const *pixel,
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

static int
sixel_lookup_policy_none_map_core_float_palette(
    float const *pixel,
    int depth,
    float const *palette,
    int palette_stride,
    int reqcolor,
    int complexion)
{
    int result;
    double diff;
    double distant;
    double r;
    int i;
    int n;
    int offset;

    result = -1;
    diff = DBL_MAX;

    for (i = 0; i < reqcolor; ++i) {
        offset = i * palette_stride;
        distant = 0.0;
        r = (double)pixel[0] - (double)palette[offset + 0];
        distant += r * r * (double)complexion;
        for (n = 1; n < depth; ++n) {
            r = (double)pixel[n] - (double)palette[offset + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    return result;
}

static int
sixel_lookup_policy_none_map_core_float_byte_palette(
    float const *pixel,
    int depth,
    unsigned char const *palette,
    int reqcolor,
    int complexion)
{
    int result;
    double diff;
    double distant;
    double r;
    int i;
    int n;

    result = -1;
    diff = DBL_MAX;

    for (i = 0; i < reqcolor; ++i) {
        distant = 0.0;
        r = (double)pixel[0] - (double)palette[i * depth + 0];
        distant += r * r * (double)complexion;
        for (n = 1; n < depth; ++n) {
            r = (double)pixel[n] - (double)palette[i * depth + n];
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
sixel_lookup_policy_none_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_none_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_none_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_none_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_none_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_none_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_none_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_none_object_t *object;

    object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_none_from_base(policy);
    object->palette = request->palette;
    object->palette_float = request->palette_float;
    object->depth = request->depth;
    object->float_stride = 0;
    object->reqcolor = request->reqcolor;
    object->complexion = 1;
    object->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);

    if (request->palette_float != NULL
            && request->float_depth >= request->depth * (int)sizeof(float)) {
        object->float_stride = request->float_depth / (int)sizeof(float);
        if (object->float_stride < request->depth) {
            object->float_stride = 0;
        }
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_none_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_none_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_none_from_base_const(policy);
    if (object->palette == NULL || object->depth <= 0
            || object->reqcolor <= 0) {
        return 0;
    }

    if (object->lookup_source_is_float != 0) {
        if (object->palette_float != NULL
                && object->float_stride >= object->depth) {
            return sixel_lookup_policy_none_map_core_float_palette(
                (float const *)(void const *)pixel,
                object->depth,
                object->palette_float,
                object->float_stride,
                object->reqcolor,
                object->complexion);
        }

        return sixel_lookup_policy_none_map_core_float_byte_palette(
            (float const *)(void const *)pixel,
            object->depth,
            object->palette,
            object->reqcolor,
            object->complexion);
    }

    return sixel_lookup_policy_none_map_core(pixel,
                                             object->depth,
                                             object->palette,
                                             object->reqcolor,
                                             object->complexion);
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_none_vtbl = {
    sixel_lookup_policy_none_ref,
    sixel_lookup_policy_none_unref,
    sixel_lookup_policy_none_prepare,
    sixel_lookup_policy_none_map_pixel,
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_lookup_policy_create_none(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_none_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_none_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_none: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_none_vtbl;
    object->ref = 1U;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
