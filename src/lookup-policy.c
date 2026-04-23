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
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * interface ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 *
 * Ownership/lifetime:
 * - create_by_name() returns refcount=1 objects.
 * - unref() releases runtime LUT ownership before freeing the object.
 */

typedef enum sixel_lookup_policy_kind {
    SIXEL_LOOKUP_POLICY_KIND_NORMAL = 0,
    SIXEL_LOOKUP_POLICY_KIND_MONO_DARKBG,
    SIXEL_LOOKUP_POLICY_KIND_MONO_LIGHTBG,
    SIXEL_LOOKUP_POLICY_KIND_FAST_LUT
} sixel_lookup_policy_kind_t;

typedef int (*sixel_lookup_policy_lookup_impl_t)(
    unsigned char const *pixel,
    int depth,
    unsigned char const *palette,
    int reqcolor,
    unsigned short *cachetable,
    int complexion);

typedef struct sixel_lookup_policy_class {
    char const *name;
    sixel_lookup_policy_kind_t kind;
    int lut_policy;
} sixel_lookup_policy_class_t;

typedef struct sixel_lookup_policy_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_lookup_policy_class_t const *policy_class;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
    sixel_lookup_policy_lookup_impl_t lookup_fn;
    unsigned char const *palette;
    int depth;
    int reqcolor;
    unsigned short *indextable;
    int complexion;
    sixel_lut_t *lut;
    int owns_lut;
} sixel_lookup_policy_object_t;

static char const g_lookup_policy_name_normal[] = "lookup/normal";
static char const g_lookup_policy_name_mono_darkbg[] = "lookup/mono-darkbg";
static char const g_lookup_policy_name_mono_lightbg[] = "lookup/mono-lightbg";
static char const g_lookup_policy_name_certlut[] = "lookup/certlut";
static char const g_lookup_policy_name_5bit[] = "lookup/5bit";
static char const g_lookup_policy_name_6bit[] = "lookup/6bit";
static char const g_lookup_policy_name_eytzinger[] = "lookup/eytzinger";
static char const g_lookup_policy_name_fhedt[] = "lookup/fhedt";
static char const g_lookup_policy_name_vptree[] = "lookup/vptree";
static char const g_lookup_policy_name_rbc[] = "lookup/rbc";
static char const g_lookup_policy_name_mahalanobis[] = "lookup/mahalanobis";

static sixel_lookup_policy_class_t const g_lookup_policy_classes[] = {
    { g_lookup_policy_name_normal, SIXEL_LOOKUP_POLICY_KIND_NORMAL,
      SIXEL_LUT_POLICY_NONE },
    { g_lookup_policy_name_mono_darkbg,
      SIXEL_LOOKUP_POLICY_KIND_MONO_DARKBG,
      SIXEL_LUT_POLICY_NONE },
    { g_lookup_policy_name_mono_lightbg,
      SIXEL_LOOKUP_POLICY_KIND_MONO_LIGHTBG,
      SIXEL_LUT_POLICY_NONE },
    { g_lookup_policy_name_certlut, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_CERTLUT },
    { g_lookup_policy_name_5bit, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_5BIT },
    { g_lookup_policy_name_6bit, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_6BIT },
    { g_lookup_policy_name_eytzinger, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_EYTZINGER },
    { g_lookup_policy_name_fhedt, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_FHEDT },
    { g_lookup_policy_name_vptree, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_VPTREE },
    { g_lookup_policy_name_rbc, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_RBC },
    { g_lookup_policy_name_mahalanobis, SIXEL_LOOKUP_POLICY_KIND_FAST_LUT,
      SIXEL_LUT_POLICY_MAHALANOBIS }
};

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

static char const *
sixel_lookup_policy_name_from_lut_policy(int lut_policy)
{
    switch (lut_policy) {
    case SIXEL_LUT_POLICY_CERTLUT:
        return g_lookup_policy_name_certlut;
    case SIXEL_LUT_POLICY_5BIT:
        return g_lookup_policy_name_5bit;
    case SIXEL_LUT_POLICY_6BIT:
        return g_lookup_policy_name_6bit;
    case SIXEL_LUT_POLICY_EYTZINGER:
        return g_lookup_policy_name_eytzinger;
    case SIXEL_LUT_POLICY_FHEDT:
        return g_lookup_policy_name_fhedt;
    case SIXEL_LUT_POLICY_VPTREE:
        return g_lookup_policy_name_vptree;
    case SIXEL_LUT_POLICY_RBC:
        return g_lookup_policy_name_rbc;
    case SIXEL_LUT_POLICY_MAHALANOBIS:
        return g_lookup_policy_name_mahalanobis;
    default:
        return g_lookup_policy_name_6bit;
    }
}

char const *
sixel_lookup_policy_select_name(
    sixel_lookup_policy_prepare_request_t const *request)
{
    int sum1;
    int sum2;
    int n;
    int normalized_lut_policy;

    sum1 = 0;
    sum2 = 0;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;

    if (request == NULL) {
        return g_lookup_policy_name_normal;
    }

    if (request->reqcolor == 2 && request->palette != NULL
            && request->depth > 0) {
        for (n = 0; n < request->depth; ++n) {
            sum1 += request->palette[n];
        }
        for (n = request->depth;
                n < request->depth + request->depth;
                ++n) {
            sum2 += request->palette[n];
        }
        if (sum1 == 0 && sum2 == 255 * 3) {
            return g_lookup_policy_name_mono_darkbg;
        }
        if (sum1 == 255 * 3 && sum2 == 0) {
            return g_lookup_policy_name_mono_lightbg;
        }
    }

    if (request->optimize_lookup == 0
            || request->depth != 3
            || request->lut_policy == SIXEL_LUT_POLICY_NONE) {
        return g_lookup_policy_name_normal;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        request->lut_policy);
    return sixel_lookup_policy_name_from_lut_policy(normalized_lut_policy);
}

static sixel_lookup_policy_class_t const *
sixel_lookup_policy_find_class(char const *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < sizeof(g_lookup_policy_classes)
            / sizeof(g_lookup_policy_classes[0]); ++i) {
        if (strcmp(g_lookup_policy_classes[i].name, name) == 0) {
            return &g_lookup_policy_classes[i];
        }
    }

    return NULL;
}

static sixel_lookup_policy_object_t *
sixel_lookup_policy_object_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_object_t *)(void *)policy;
}

static sixel_lookup_policy_object_t const *
sixel_lookup_policy_object_from_base_const(sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_object_t const *)(void const *)policy;
}

static void
sixel_lookup_policy_reset_state(sixel_lookup_policy_object_t *object)
{
    if (object == NULL) {
        return;
    }

    if (object->owns_lut != 0 && object->lut != NULL) {
        sixel_lut_unref(object->lut);
    }

    object->lookup_source_is_float = 0;
    object->prefer_palette_float_lookup = 0;
    object->lookup_fn = NULL;
    object->palette = NULL;
    object->depth = 0;
    object->reqcolor = 0;
    object->indextable = NULL;
    object->complexion = 0;
    object->lut = NULL;
    object->owns_lut = 0;
}

static SIXELSTATUS
sixel_lookup_policy_prepare_normal(sixel_lookup_policy_object_t *object,
                                   sixel_lookup_policy_prepare_request_t const
                                       *request)
{
    if (object == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object->lookup_fn = lookup_normal;
    object->palette = request->palette;
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;
    object->indextable = NULL;
    object->complexion = request->complexion;
    object->lookup_source_is_float = 0;
    object->prefer_palette_float_lookup = 0;

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)
            && request->palette_float != NULL
            && request->float_depth >= request->depth) {
        object->prefer_palette_float_lookup = 1;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_prepare_mono(sixel_lookup_policy_object_t *object,
                                 sixel_lookup_policy_prepare_request_t const
                                 *request,
                                 sixel_lookup_policy_lookup_impl_t mono_fn)
{
    if (object == NULL || request == NULL || mono_fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object->lookup_fn = mono_fn;
    object->palette = request->palette;
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;
    object->indextable = NULL;
    object->complexion = request->complexion;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_prepare_fast_lut(sixel_lookup_policy_object_t *object,
                                     sixel_lookup_policy_prepare_request_t const
                                         *request)
{
    SIXELSTATUS status;
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t lookup_result;
    sixel_lut_t *reuse_lut;
    int normalized_lut_policy;
    int shared_lut;
    int reuse_lut_preconfigured;

    status = SIXEL_FALSE;
    memset(&lookup_config, 0, sizeof(lookup_config));
    memset(&lookup_result, 0, sizeof(lookup_result));
    reuse_lut = NULL;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;
    shared_lut = 1;
    reuse_lut_preconfigured = 0;

    if (object == NULL || request == NULL || object->policy_class == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        object->policy_class->lut_policy);
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

static void
sixel_lookup_policy_ref_impl(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_object_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_unref_impl(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_object_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_prepare_impl(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_object_t *object;

    status = SIXEL_FALSE;
    object = NULL;

    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_object_from_base(policy);
    if (object->policy_class == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if ((object->policy_class->kind == SIXEL_LOOKUP_POLICY_KIND_NORMAL
            || object->policy_class->kind
                == SIXEL_LOOKUP_POLICY_KIND_FAST_LUT)
            && !SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        status = sixel_lookup_policy_validate_complexion_limit(
            request->depth,
            request->complexion);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    sixel_lookup_policy_reset_state(object);

    switch (object->policy_class->kind) {
    case SIXEL_LOOKUP_POLICY_KIND_MONO_DARKBG:
        status = sixel_lookup_policy_prepare_mono(object,
                                                  request,
                                                  lookup_mono_darkbg);
        break;
    case SIXEL_LOOKUP_POLICY_KIND_MONO_LIGHTBG:
        status = sixel_lookup_policy_prepare_mono(object,
                                                  request,
                                                  lookup_mono_lightbg);
        break;
    case SIXEL_LOOKUP_POLICY_KIND_FAST_LUT:
        status = sixel_lookup_policy_prepare_fast_lut(object, request);
        break;
    case SIXEL_LOOKUP_POLICY_KIND_NORMAL:
    default:
        status = sixel_lookup_policy_prepare_normal(object, request);
        break;
    }

    return status;
}

static int
sixel_lookup_policy_map_pixel_impl(sixel_lookup_policy_interface_t const *policy,
                                   unsigned char const *pixel)
{
    sixel_lookup_policy_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_object_from_base_const(policy);
    if (object->policy_class == NULL) {
        return 0;
    }

    if (object->policy_class->kind == SIXEL_LOOKUP_POLICY_KIND_FAST_LUT) {
        if (object->lut == NULL) {
            return 0;
        }
        return sixel_lut_map_pixel(object->lut, pixel);
    }

    if (object->lookup_fn == NULL || object->palette == NULL
            || object->depth <= 0 || object->reqcolor <= 0) {
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
sixel_lookup_policy_lookup_source_is_float_impl(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_object_from_base_const(policy);
    return object->lookup_source_is_float;
}

static int
sixel_lookup_policy_prefer_palette_float_lookup_impl(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_object_from_base_const(policy);
    return object->prefer_palette_float_lookup;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_vtbl = {
    sixel_lookup_policy_ref_impl,
    sixel_lookup_policy_unref_impl,
    sixel_lookup_policy_prepare_impl,
    sixel_lookup_policy_map_pixel_impl,
    sixel_lookup_policy_lookup_source_is_float_impl,
    sixel_lookup_policy_prefer_palette_float_lookup_impl
};

SIXELSTATUS
sixel_lookup_policy_create_by_name(
    char const *name,
    sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_object_t *object;
    sixel_lookup_policy_class_t const *policy_class;

    object = NULL;
    policy_class = NULL;

    if (policy != NULL) {
        *policy = NULL;
    }

    if (name == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    policy_class = sixel_lookup_policy_find_class(name);
    if (policy_class == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_by_name: unknown lookup class.");
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_by_name: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_vtbl;
    object->ref = 1U;
    object->policy_class = policy_class;

    *policy = &object->base;
    return SIXEL_OK;
}

void
sixel_lookup_policy_ref(sixel_lookup_policy_interface_t *policy)
{
    if (policy == NULL || policy->vtbl == NULL || policy->vtbl->ref == NULL) {
        return;
    }

    policy->vtbl->ref(policy);
}

void
sixel_lookup_policy_unref(sixel_lookup_policy_interface_t *policy)
{
    if (policy == NULL || policy->vtbl == NULL || policy->vtbl->unref == NULL) {
        return;
    }

    policy->vtbl->unref(policy);
}

SIXELSTATUS
sixel_lookup_policy_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    if (policy == NULL || policy->vtbl == NULL
            || policy->vtbl->prepare == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return policy->vtbl->prepare(policy, request);
}

int
sixel_lookup_policy_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    if (policy == NULL || policy->vtbl == NULL
            || policy->vtbl->map_pixel == NULL) {
        return 0;
    }

    return policy->vtbl->map_pixel(policy, pixel);
}

int
sixel_lookup_policy_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    if (policy == NULL || policy->vtbl == NULL
            || policy->vtbl->lookup_source_is_float == NULL) {
        return 0;
    }

    return policy->vtbl->lookup_source_is_float(policy);
}

int
sixel_lookup_policy_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    if (policy == NULL || policy->vtbl == NULL
            || policy->vtbl->prefer_palette_float_lookup == NULL) {
        return 0;
    }

    return policy->vtbl->prefer_palette_float_lookup(policy);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
