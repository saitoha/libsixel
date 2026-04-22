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
#include <string.h>

#include "filter-lookup.h"
#include "lookup-common.h"
#include "lookup-policy.h"

static SIXELSTATUS
sixel_lookup_policy_prepare_normal(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request);

static SIXELSTATUS
sixel_lookup_policy_prepare_fast_lut(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request);

static SIXELSTATUS
sixel_lookup_policy_prepare_mono_darkbg(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request);

static SIXELSTATUS
sixel_lookup_policy_prepare_mono_lightbg(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request);

static int
sixel_lookup_policy_map_with_lookup_fn(sixel_lookup_policy_t const *policy,
                                       unsigned char const *pixel);

static int
sixel_lookup_policy_map_fast_lut(sixel_lookup_policy_t const *policy,
                                 unsigned char const *pixel);

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

static sixel_lookup_policy_vtbl_t const sixel_lookup_policy_normal_vtbl = {
    "lookup-normal",
    sixel_lookup_policy_prepare_normal,
    sixel_lookup_policy_map_with_lookup_fn
};

static sixel_lookup_policy_vtbl_t const sixel_lookup_policy_fast_lut_vtbl = {
    "lookup-fast-lut",
    sixel_lookup_policy_prepare_fast_lut,
    sixel_lookup_policy_map_fast_lut
};

static sixel_lookup_policy_vtbl_t const sixel_lookup_policy_mono_darkbg_vtbl = {
    "lookup-mono-darkbg",
    sixel_lookup_policy_prepare_mono_darkbg,
    sixel_lookup_policy_map_with_lookup_fn
};

static sixel_lookup_policy_vtbl_t const
sixel_lookup_policy_mono_lightbg_vtbl = {
    "lookup-mono-lightbg",
    sixel_lookup_policy_prepare_mono_lightbg,
    sixel_lookup_policy_map_with_lookup_fn
};

static SIXELSTATUS
sixel_lookup_policy_prepare_normal(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    (void)request;

    policy->lookup_fn = lookup_normal;
    policy->lookup_source_is_float = 0;
    policy->prefer_palette_float_lookup = 0;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(policy->pixelformat)
            && policy->palette_float != NULL
            && policy->float_depth >= policy->depth) {
        policy->prefer_palette_float_lookup = 1;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_prepare_fast_lut(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t lookup_result;
    sixel_lut_t *reuse_lut;
    int shared_lut;
    int reuse_lut_preconfigured;
    int wcomp1;
    int wcomp2;
    int wcomp3;

    status = SIXEL_FALSE;
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&lookup_result, 0, sizeof(lookup_result));
    reuse_lut = NULL;
    shared_lut = 1;
    reuse_lut_preconfigured = 0;
    wcomp1 = policy->complexion;
    wcomp2 = 1;
    wcomp3 = 1;

    if (policy->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    policy->lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        request->lut_policy);
    shared_lut = sixel_lookup_policy_shared_cache_enabled(policy->lut_policy);

    reuse_lut = request->reuse_lut;
    if (request->parallel_active != 0
            && shared_lut == 0
            && request->reuse_lut_is_shared != 0) {
        reuse_lut = NULL;
    }
    if (reuse_lut != NULL && request->reuse_lut_preconfigured != 0) {
        reuse_lut_preconfigured = 1;
    }

    if (policy->lut_policy == SIXEL_LUT_POLICY_CERTLUT
            && policy->method_for_largest == SIXEL_LARGE_LUM) {
        wcomp1 = policy->complexion * 299;
        wcomp2 = 587;
        wcomp3 = 114;
    }

    lookup_config.palette = policy->palette;
    lookup_config.palette_float = policy->palette_float;
    lookup_config.depth = policy->depth;
    lookup_config.float_depth = policy->float_depth;
    lookup_config.ncolors = policy->reqcolor;
    lookup_config.complexion = policy->complexion;
    lookup_config.method_for_largest = policy->method_for_largest;
    lookup_config.lut_policy = policy->lut_policy;
    lookup_config.pixelformat = policy->pixelformat;
    lookup_config.reuse_lut = reuse_lut;

    if (reuse_lut_preconfigured != 0) {
        /*
         * Shared and worker-local caches can arrive fully configured from the
         * caller.  Reusing them here avoids configure-time races and keeps the
         * pixel loop focused on map dispatch.
         */
        policy->lut = reuse_lut;
        policy->owns_lut = 0;
        policy->lookup_source_is_float =
            SIXEL_PIXELFORMAT_IS_FLOAT32(policy->pixelformat);
        policy->prefer_palette_float_lookup = 0;
        return SIXEL_OK;
    }

    status = sixel_filter_lookup_build(&lookup_config,
                                       request->allocator,
                                       NULL,
                                       &lookup_result);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    policy->lut = lookup_result.lut;
    policy->owns_lut = lookup_result.owned;

    if (policy->owns_lut != 0
            && request->reuse_lut_slot != NULL
            && *request->reuse_lut_slot == NULL) {
        *request->reuse_lut_slot = policy->lut;
        policy->owns_lut = 0;
    }

    policy->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(policy->pixelformat);
    policy->prefer_palette_float_lookup = 0;

    /* Keep computed weights visible for future backend-specific policies. */
    (void)wcomp1;
    (void)wcomp2;
    (void)wcomp3;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_prepare_mono_darkbg(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    (void)request;

    policy->lookup_fn = lookup_mono_darkbg;
    policy->lookup_source_is_float = 0;
    policy->prefer_palette_float_lookup = 0;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_prepare_mono_lightbg(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    (void)request;

    policy->lookup_fn = lookup_mono_lightbg;
    policy->lookup_source_is_float = 0;
    policy->prefer_palette_float_lookup = 0;

    return SIXEL_OK;
}

static int
sixel_lookup_policy_map_with_lookup_fn(sixel_lookup_policy_t const *policy,
                                       unsigned char const *pixel)
{
    if (policy == NULL || pixel == NULL || policy->lookup_fn == NULL) {
        return 0;
    }

    return policy->lookup_fn(pixel,
                             policy->depth,
                             policy->palette,
                             policy->reqcolor,
                             policy->indextable,
                             policy->complexion);
}

static int
sixel_lookup_policy_map_fast_lut(sixel_lookup_policy_t const *policy,
                                 unsigned char const *pixel)
{
    if (policy == NULL || pixel == NULL || policy->lut == NULL) {
        return 0;
    }

    return sixel_lut_map_pixel(policy->lut, pixel);
}

void
sixel_lookup_policy_init(sixel_lookup_policy_t *policy)
{
    if (policy == NULL) {
        return;
    }

    memset(policy, 0, sizeof(*policy));
    policy->vtbl = &sixel_lookup_policy_normal_vtbl;
    policy->mode = SIXEL_DITHER_LOOKUP_MODE_NORMAL;
}

void
sixel_lookup_policy_clear(sixel_lookup_policy_t *policy)
{
    if (policy == NULL) {
        return;
    }

    if (policy->owns_lut != 0 && policy->lut != NULL) {
        sixel_lut_unref(policy->lut);
    }

    memset(policy, 0, sizeof(*policy));
    policy->vtbl = &sixel_lookup_policy_normal_vtbl;
    policy->mode = SIXEL_DITHER_LOOKUP_MODE_NORMAL;
}

SIXELSTATUS
sixel_lookup_policy_prepare(
    sixel_lookup_policy_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_mode_t mode;

    status = SIXEL_FALSE;
    mode = SIXEL_DITHER_LOOKUP_MODE_NORMAL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_policy_clear(policy);

    policy->palette = request->palette;
    policy->palette_float = request->palette_float;
    policy->depth = request->depth;
    policy->float_depth = request->float_depth;
    policy->reqcolor = request->reqcolor;
    policy->complexion = request->complexion;
    policy->pixelformat = request->pixelformat;
    policy->lut_policy = request->lut_policy;
    policy->method_for_largest = request->method_for_largest;
    policy->indextable = NULL;

    mode = sixel_lookup_policy_select_mode(request);
    policy->mode = mode;

    if ((mode == SIXEL_DITHER_LOOKUP_MODE_NORMAL
            || mode == SIXEL_DITHER_LOOKUP_MODE_FAST_LUT)
            && !SIXEL_PIXELFORMAT_IS_FLOAT32(policy->pixelformat)) {
        status = sixel_lookup_policy_validate_complexion_limit(
            policy->depth,
            policy->complexion);
        if (SIXEL_FAILED(status)) {
            sixel_lookup_policy_clear(policy);
            return status;
        }
    }

    switch (mode) {
    case SIXEL_DITHER_LOOKUP_MODE_FAST_LUT:
        policy->vtbl = &sixel_lookup_policy_fast_lut_vtbl;
        break;
    case SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG:
        policy->vtbl = &sixel_lookup_policy_mono_darkbg_vtbl;
        break;
    case SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG:
        policy->vtbl = &sixel_lookup_policy_mono_lightbg_vtbl;
        break;
    case SIXEL_DITHER_LOOKUP_MODE_NORMAL:
    default:
        policy->vtbl = &sixel_lookup_policy_normal_vtbl;
        break;
    }

    status = policy->vtbl->prepare(policy, request);
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_clear(policy);
        return status;
    }

    return SIXEL_OK;
}

sixel_lookup_policy_mode_t
sixel_lookup_policy_get_mode(sixel_lookup_policy_t const *policy)
{
    if (policy == NULL) {
        return SIXEL_DITHER_LOOKUP_MODE_NORMAL;
    }

    return policy->mode;
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
    if (policy == NULL) {
        return 0;
    }

    return policy->lookup_source_is_float;
}

int
sixel_lookup_policy_prefer_palette_float_lookup(
    sixel_lookup_policy_t const *policy)
{
    if (policy == NULL) {
        return 0;
    }

    return policy->prefer_palette_float_lookup;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
