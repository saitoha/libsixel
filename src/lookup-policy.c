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

#include "filter-lookup.h"
#include "lookup-common.h"
#include "lookup-policy.h"

typedef struct sixel_lookup_policy_object {
    sixel_lookup_policy_t base;
    sixel_lookup_policy_mode_t mode;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
    sixel_allocator_t *allocator;
} sixel_lookup_policy_object_t;

typedef struct sixel_lookup_policy_lookup_object {
    sixel_lookup_policy_object_t object;
    sixel_lookup_policy_lookup_fn lookup_fn;
    unsigned char const *palette;
    int depth;
    int reqcolor;
    unsigned short *indextable;
    int complexion;
} sixel_lookup_policy_lookup_object_t;

typedef struct sixel_lookup_policy_fast_lut_object {
    sixel_lookup_policy_object_t object;
    sixel_lut_t *lut;
    int owns_lut;
} sixel_lookup_policy_fast_lut_object_t;

static int
lookup_normal(unsigned char const *pixel,
              int depth,
              unsigned char const *palette,
              int reqcolor,
              unsigned short *cachetable,
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

    /* The normal strategy does not use the short-lived index cache. */
    (void)cachetable;

    for (i = 0; i < reqcolor; i++) {
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
lookup_mono_darkbg(unsigned char const *pixel,
                   int depth,
                   unsigned char const *palette,
                   int reqcolor,
                   unsigned short *cachetable,
                   int complexion)
{
    int n;
    int distant;

    (void)palette;
    (void)cachetable;
    (void)complexion;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }

    return distant >= 128 * reqcolor ? 1 : 0;
}

static int
lookup_mono_lightbg(unsigned char const *pixel,
                    int depth,
                    unsigned char const *palette,
                    int reqcolor,
                    unsigned short *cachetable,
                    int complexion)
{
    int n;
    int distant;

    (void)palette;
    (void)cachetable;
    (void)complexion;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }

    return distant < 128 * reqcolor ? 1 : 0;
}

static SIXELSTATUS
sixel_lookup_policy_validate_complexion_limit(int depth, int complexion)
{
    enum { max_channel_diff_sq = 255 * 255 };
    int non_weighted_components;
    long long weighted_budget;
    long long max_complexion;

    if (complexion <= 1) {
        return SIXEL_OK;
    }

    non_weighted_components = (depth > 1) ? (depth - 1) : 0;
    weighted_budget = (long long)INT_MAX
        - (long long)max_channel_diff_sq * (long long)non_weighted_components;
    if (weighted_budget <= 0) {
        max_complexion = 0;
    } else {
        max_complexion = weighted_budget / (long long)max_channel_diff_sq;
    }

    if ((long long)complexion > max_complexion) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: complexion parameter is too large.");
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
}

static int
sixel_lookup_policy_normalize_fast_lut_policy(int lut_policy)
{
    int normalized;

    normalized = lut_policy;
    if (normalized != SIXEL_LUT_POLICY_CERTLUT
        && normalized != SIXEL_LUT_POLICY_5BIT
        && normalized != SIXEL_LUT_POLICY_6BIT
        && normalized != SIXEL_LUT_POLICY_EYTZINGER
        && normalized != SIXEL_LUT_POLICY_FHEDT
        && normalized != SIXEL_LUT_POLICY_VPTREE
        && normalized != SIXEL_LUT_POLICY_RBC
        && normalized != SIXEL_LUT_POLICY_MAHALANOBIS) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    return normalized;
}

static int
sixel_lookup_policy_shared_cache_enabled(int lut_policy)
{
    int shared;

    shared = 1;
    if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        shared = sixel_lookup_env_shared_certlut();
    } else if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        shared = sixel_lookup_env_shared_5bit();
    } else if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        shared = sixel_lookup_env_shared_6bit();
    }

    return shared;
}

static sixel_lookup_policy_mode_t
sixel_lookup_policy_select_mode(
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_mode_t mode;
    int sum1;
    int sum2;
    int n;

    mode = SIXEL_DITHER_LOOKUP_MODE_NORMAL;

    if (request->reqcolor == 2 && request->palette != NULL
            && request->depth > 0) {
        sum1 = 0;
        sum2 = 0;
        for (n = 0; n < request->depth; ++n) {
            sum1 += request->palette[n];
        }
        for (n = request->depth;
                n < request->depth + request->depth;
                ++n) {
            sum2 += request->palette[n];
        }
        if (sum1 == 0 && sum2 == 255 * 3) {
            mode = SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG;
        } else if (sum1 == 255 * 3 && sum2 == 0) {
            mode = SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG;
        }
    }

    if (request->optimize_lookup != 0
            && request->depth == 3
            && mode == SIXEL_DITHER_LOOKUP_MODE_NORMAL
            && request->lut_policy != SIXEL_LUT_POLICY_NONE) {
        mode = SIXEL_DITHER_LOOKUP_MODE_FAST_LUT;
    }

    return mode;
}

static sixel_lookup_policy_object_t const *
sixel_lookup_policy_object_from_base_const(
    sixel_lookup_policy_t const *policy)
{
    return (sixel_lookup_policy_object_t const *)(void const *)policy;
}

static void *
sixel_lookup_policy_object_alloc(size_t bytes, sixel_allocator_t *allocator)
{
    void *memory;

    memory = NULL;
    if (allocator != NULL) {
        memory = sixel_allocator_malloc(allocator, bytes);
        if (memory != NULL) {
            sixel_allocator_ref(allocator);
        }
        return memory;
    }

    memory = malloc(bytes);
    return memory;
}

static void
sixel_lookup_policy_object_free(sixel_lookup_policy_object_t *object)
{
    sixel_allocator_t *allocator;

    if (object == NULL) {
        return;
    }

    allocator = object->allocator;
    if (allocator != NULL) {
        sixel_allocator_free(allocator, object);
        sixel_allocator_unref(allocator);
        return;
    }

    free(object);
}

static int
sixel_lookup_policy_map_lookup_object(sixel_lookup_policy_t const *policy,
                                      unsigned char const *pixel)
{
    sixel_lookup_policy_lookup_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = (sixel_lookup_policy_lookup_object_t const *)(void const *)policy;
    if (object->lookup_fn == NULL) {
        return 0;
    }

    return object->lookup_fn(pixel,
                             object->depth,
                             object->palette,
                             object->reqcolor,
                             object->indextable,
                             object->complexion);
}

static int
sixel_lookup_policy_map_fast_lut_object(sixel_lookup_policy_t const *policy,
                                        unsigned char const *pixel)
{
    sixel_lookup_policy_fast_lut_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object =
        (sixel_lookup_policy_fast_lut_object_t const *)(void const *)policy;
    if (object->lut == NULL) {
        return 0;
    }

    return sixel_lut_map_pixel(object->lut, pixel);
}

static void
sixel_lookup_policy_destroy_lookup_object(sixel_lookup_policy_t *policy)
{
    sixel_lookup_policy_lookup_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = (sixel_lookup_policy_lookup_object_t *)(void *)policy;
    sixel_lookup_policy_object_free(&object->object);
}

static void
sixel_lookup_policy_destroy_fast_lut_object(sixel_lookup_policy_t *policy)
{
    sixel_lookup_policy_fast_lut_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = (sixel_lookup_policy_fast_lut_object_t *)(void *)policy;
    if (object->owns_lut != 0 && object->lut != NULL) {
        sixel_lut_unref(object->lut);
    }
    object->lut = NULL;
    object->owns_lut = 0;
    sixel_lookup_policy_object_free(&object->object);
}

static sixel_lookup_policy_vtbl_t const sixel_lookup_policy_normal_vtbl = {
    "lookup-normal",
    sixel_lookup_policy_destroy_lookup_object,
    sixel_lookup_policy_map_lookup_object
};

static sixel_lookup_policy_vtbl_t const sixel_lookup_policy_fast_lut_vtbl = {
    "lookup-fast-lut",
    sixel_lookup_policy_destroy_fast_lut_object,
    sixel_lookup_policy_map_fast_lut_object
};

static sixel_lookup_policy_vtbl_t const sixel_lookup_policy_mono_darkbg_vtbl = {
    "lookup-mono-darkbg",
    sixel_lookup_policy_destroy_lookup_object,
    sixel_lookup_policy_map_lookup_object
};

static sixel_lookup_policy_vtbl_t const
sixel_lookup_policy_mono_lightbg_vtbl = {
    "lookup-mono-lightbg",
    sixel_lookup_policy_destroy_lookup_object,
    sixel_lookup_policy_map_lookup_object
};

static SIXELSTATUS
sixel_lookup_policy_build_lookup_object(
    sixel_lookup_policy_t **out_policy,
    sixel_lookup_policy_prepare_request_t const *request,
    sixel_lookup_policy_mode_t mode,
    sixel_lookup_policy_lookup_fn lookup_fn,
    sixel_lookup_policy_vtbl_t const *vtbl,
    int lookup_source_is_float,
    int prefer_palette_float_lookup)
{
    sixel_lookup_policy_lookup_object_t *object;

    object = NULL;
    if (out_policy == NULL || request == NULL || lookup_fn == NULL
            || vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_lookup_object_t *)
        sixel_lookup_policy_object_alloc(sizeof(*object), request->allocator);
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->object.base.vtbl = vtbl;
    object->object.mode = mode;
    object->object.lookup_source_is_float = lookup_source_is_float;
    object->object.prefer_palette_float_lookup =
        prefer_palette_float_lookup;
    object->object.allocator = request->allocator;
    object->lookup_fn = lookup_fn;
    object->palette = request->palette;
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;
    object->indextable = NULL;
    object->complexion = request->complexion;

    *out_policy = &object->object.base;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_build_normal(
    sixel_lookup_policy_t **out_policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    int prefer_palette_float_lookup;

    prefer_palette_float_lookup = 0;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)
            && request->palette_float != NULL
            && request->float_depth >= request->depth) {
        prefer_palette_float_lookup = 1;
    }

    return sixel_lookup_policy_build_lookup_object(
        out_policy,
        request,
        SIXEL_DITHER_LOOKUP_MODE_NORMAL,
        lookup_normal,
        &sixel_lookup_policy_normal_vtbl,
        0,
        prefer_palette_float_lookup);
}

static SIXELSTATUS
sixel_lookup_policy_build_mono_darkbg(
    sixel_lookup_policy_t **out_policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    return sixel_lookup_policy_build_lookup_object(
        out_policy,
        request,
        SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG,
        lookup_mono_darkbg,
        &sixel_lookup_policy_mono_darkbg_vtbl,
        0,
        0);
}

static SIXELSTATUS
sixel_lookup_policy_build_mono_lightbg(
    sixel_lookup_policy_t **out_policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    return sixel_lookup_policy_build_lookup_object(
        out_policy,
        request,
        SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG,
        lookup_mono_lightbg,
        &sixel_lookup_policy_mono_lightbg_vtbl,
        0,
        0);
}

static SIXELSTATUS
sixel_lookup_policy_build_fast_lut(
    sixel_lookup_policy_t **out_policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_fast_lut_object_t *object;
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t lookup_result;
    sixel_lut_t *reuse_lut;
    int normalized_lut_policy;
    int shared_lut;
    int reuse_lut_preconfigured;
    int wcomp1;
    int wcomp2;
    int wcomp3;

    status = SIXEL_FALSE;
    object = NULL;
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&lookup_result, 0, sizeof(lookup_result));
    reuse_lut = NULL;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;
    shared_lut = 1;
    reuse_lut_preconfigured = 0;
    wcomp1 = request->complexion;
    wcomp2 = 1;
    wcomp3 = 1;

    if (out_policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        request->lut_policy);
    shared_lut =
        sixel_lookup_policy_shared_cache_enabled(normalized_lut_policy);

    reuse_lut = request->reuse_lut;
    if (request->parallel_active != 0
            && shared_lut == 0
            && request->reuse_lut_is_shared != 0) {
        reuse_lut = NULL;
    }
    if (reuse_lut != NULL && request->reuse_lut_preconfigured != 0) {
        reuse_lut_preconfigured = 1;
    }

    if (normalized_lut_policy == SIXEL_LUT_POLICY_CERTLUT
            && request->method_for_largest == SIXEL_LARGE_LUM) {
        wcomp1 = request->complexion * 299;
        wcomp2 = 587;
        wcomp3 = 114;
    }

    object = (sixel_lookup_policy_fast_lut_object_t *)
        sixel_lookup_policy_object_alloc(sizeof(*object), request->allocator);
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->object.base.vtbl = &sixel_lookup_policy_fast_lut_vtbl;
    object->object.mode = SIXEL_DITHER_LOOKUP_MODE_FAST_LUT;
    object->object.lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);
    object->object.prefer_palette_float_lookup = 0;
    object->object.allocator = request->allocator;

    if (reuse_lut_preconfigured != 0) {
        /*
         * Shared and worker-local caches can arrive fully configured from the
         * caller. Reusing them here avoids configure-time races and keeps the
         * pixel loop focused on map dispatch.
         */
        object->lut = reuse_lut;
        object->owns_lut = 0;
        *out_policy = &object->object.base;
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
        sixel_lookup_policy_destroy_fast_lut_object(&object->object.base);
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

    /* Keep computed weights visible for future backend-specific policies. */
    (void)wcomp1;
    (void)wcomp2;
    (void)wcomp3;

    *out_policy = &object->object.base;

    return SIXEL_OK;
}

void
sixel_lookup_policy_init(sixel_lookup_policy_t **policy)
{
    if (policy == NULL) {
        return;
    }

    *policy = NULL;
}

void
sixel_lookup_policy_clear(sixel_lookup_policy_t **policy)
{
    sixel_lookup_policy_t *current;

    current = NULL;
    if (policy == NULL || *policy == NULL) {
        return;
    }

    current = *policy;
    *policy = NULL;
    if (current->vtbl != NULL && current->vtbl->destroy != NULL) {
        current->vtbl->destroy(current);
    }
}

SIXELSTATUS
sixel_lookup_policy_prepare(
    sixel_lookup_policy_t **policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_mode_t mode;
    sixel_lookup_policy_t *prepared;

    status = SIXEL_FALSE;
    mode = SIXEL_DITHER_LOOKUP_MODE_NORMAL;
    prepared = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_clear(policy);

    mode = sixel_lookup_policy_select_mode(request);

    if ((mode == SIXEL_DITHER_LOOKUP_MODE_NORMAL
            || mode == SIXEL_DITHER_LOOKUP_MODE_FAST_LUT)
            && !SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        status = sixel_lookup_policy_validate_complexion_limit(
            request->depth,
            request->complexion);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    switch (mode) {
    case SIXEL_DITHER_LOOKUP_MODE_FAST_LUT:
        status = sixel_lookup_policy_build_fast_lut(&prepared, request);
        break;
    case SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG:
        status = sixel_lookup_policy_build_mono_darkbg(&prepared, request);
        break;
    case SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG:
        status = sixel_lookup_policy_build_mono_lightbg(&prepared, request);
        break;
    case SIXEL_DITHER_LOOKUP_MODE_NORMAL:
    default:
        status = sixel_lookup_policy_build_normal(&prepared, request);
        break;
    }
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *policy = prepared;

    return SIXEL_OK;
}

sixel_lookup_policy_mode_t
sixel_lookup_policy_get_mode(sixel_lookup_policy_t const *policy)
{
    sixel_lookup_policy_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_DITHER_LOOKUP_MODE_NORMAL;
    }

    object = sixel_lookup_policy_object_from_base_const(policy);
    return object->mode;
}

sixel_lookup_policy_map_fn
sixel_lookup_policy_get_map_fn(sixel_lookup_policy_t const *policy)
{
    if (policy == NULL || policy->vtbl == NULL
            || policy->vtbl->map_pixel == NULL) {
        return NULL;
    }

    return policy->vtbl->map_pixel;
}

int
sixel_lookup_policy_map_pixel(sixel_lookup_policy_t const *policy,
                              unsigned char const *pixel)
{
    sixel_lookup_policy_map_fn map_pixel;

    map_pixel = sixel_lookup_policy_get_map_fn(policy);
    if (map_pixel == NULL) {
        return 0;
    }

    return map_pixel(policy, pixel);
}

int
sixel_lookup_policy_lookup_source_is_float(sixel_lookup_policy_t const *policy)
{
    sixel_lookup_policy_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_object_from_base_const(policy);
    return object->lookup_source_is_float;
}

int
sixel_lookup_policy_prefer_palette_float_lookup(
    sixel_lookup_policy_t const *policy)
{
    sixel_lookup_policy_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_object_from_base_const(policy);
    return object->prefer_palette_float_lookup;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
