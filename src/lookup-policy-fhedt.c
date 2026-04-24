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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_MATH_H
# include <math.h>
#endif

#include "compat_stub.h"
#include "lookup-8bit.h"
#include "lookup-common.h"
#include "lookup-float32.h"
#include "lookup-policy-private.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupFHEDT : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 */

typedef struct sixel_lookup_policy_fhedt_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_lut_t *lut;
    int owns_lut;
    int lookup_source_is_float;
} sixel_lookup_policy_fhedt_object_t;

static sixel_lookup_policy_fhedt_object_t *
sixel_lookup_policy_fhedt_from_base(sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_fhedt_object_t *)(void *)policy;
}

static sixel_lookup_policy_fhedt_object_t const *
sixel_lookup_policy_fhedt_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_fhedt_object_t const *)(void const *)policy;
}

static int
sixel_lookup_policy_fhedt_parse_flag(char const *text, int default_value)
{
    long parsed;
    char *endptr;

    parsed = 0L;
    endptr = NULL;
    if (text == NULL || text[0] == '\0') {
        return default_value;
    }

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (errno == ERANGE || endptr == text || *endptr != '\0') {
        return default_value;
    }

    if (parsed == 0L) {
        return 0;
    }
    if (parsed == 1L) {
        return 1;
    }

    return default_value;
}

static int
sixel_lookup_policy_fhedt_env_resolution(void)
{
    char const *env;
    long parsed;
    char *endptr;

    env = NULL;
    parsed = 0L;
    endptr = NULL;
    env = sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_RESOLUTION");
    if (env == NULL || env[0] == '\0') {
        return 64;
    }

    errno = 0;
    parsed = strtol(env, &endptr, 10);
    if (errno == ERANGE || endptr == env || *endptr != '\0') {
        return 64;
    }

    if (parsed == 64L || parsed == 128L || parsed == 256L) {
        return (int)parsed;
    }

    return 64;
}

static int
sixel_lookup_policy_fhedt_env_refine(void)
{
    return sixel_lookup_policy_fhedt_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_REFINE"),
        1);
}

static int
sixel_lookup_policy_fhedt_env_shared(void)
{
    return sixel_lookup_policy_fhedt_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_SHARED"),
        1);
}

static int
sixel_lookup_policy_fhedt_env_use_dist2(void)
{
    return sixel_lookup_policy_fhedt_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_USE_DIST2"),
        0);
}

static int
sixel_lookup_policy_fhedt_env_use_cache(void)
{
    return sixel_lookup_policy_fhedt_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_FHEDT_USE_CACHE"),
        0);
}

static SIXELSTATUS
sixel_lookup_policy_fhedt_prepare_float_palette(
    sixel_lookup_float32_t *lut,
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
            "sixel_lookup_policy_fhedt: float palette allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    cursor = lut->palette;
    float_cursor = palette_float;
    expected_float_depth = lut->depth * (int)sizeof(float);
    if (float_cursor != NULL && float_depth > 0) {
        if (float_depth < expected_float_depth) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_fhedt: float palette depth mismatch.");
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
sixel_lookup_policy_fhedt_configure_8bit(
    sixel_lookup_8bit_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    int resolution;
    int refine;
    int shared_flag;
    int use_dist2;
    int use_cache;
    uint32_t signature;

    status = SIXEL_FALSE;
    resolution = 0;
    refine = 0;
    shared_flag = 0;
    use_dist2 = 0;
    use_cache = 0;
    signature = 0U;

    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_8bit_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_FHEDT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;
    lut->palette = request->palette;

    if (lut->fhedt == NULL) {
        status = sixel_lookup_fhedt_8bit_create(lut->allocator, &lut->fhedt);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_fhedt: FHEDT allocation failed.");
            return status;
        }
    }

    resolution = sixel_lookup_policy_fhedt_env_resolution();
    refine = sixel_lookup_policy_fhedt_env_refine();
    shared_flag = sixel_lookup_policy_fhedt_env_shared();
    use_dist2 = sixel_lookup_policy_fhedt_env_use_dist2();
    use_cache = sixel_lookup_policy_fhedt_env_use_cache();

    signature = sixel_lookup_fhedt_8bit_signature(request->palette,
                                                  request->reqcolor,
                                                  resolution,
                                                  refine,
                                                  1,
                                                  1,
                                                  1,
                                                  request->depth);
    status = sixel_lookup_fhedt_8bit_configure(lut->fhedt,
                                               request->palette,
                                               request->reqcolor,
                                               resolution,
                                               refine,
                                               use_dist2,
                                               use_cache,
                                               shared_flag,
                                               1,
                                               1,
                                               1,
                                               request->pixelformat,
                                               request->depth);
    if (SIXEL_FAILED(status)) {
        lut->fhedt_ready = 0;
        return status;
    }

    sixel_lookup_fhedt_8bit_shared_set_signature(lut->fhedt->shared,
                                                 signature);
    lut->fhedt_ready = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lookup_policy_fhedt_configure_float32(
    sixel_lookup_float32_t *lut,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    float base_weights[SIXEL_LOOKUP_FLOAT_COMPONENTS];
    float range;
    float scale;
    int component;
    int resolution;
    int refine;
    int shared_flag;
    int use_dist2;
    int use_cache;
    uint32_t signature;

    status = SIXEL_FALSE;
    base_weights[0] = 0.0f;
    base_weights[1] = 0.0f;
    base_weights[2] = 0.0f;
    range = 1.0f;
    scale = 1.0f;
    component = 0;
    resolution = 0;
    refine = 0;
    shared_flag = 0;
    use_dist2 = 0;
    use_cache = 0;
    signature = 0U;

    if (lut == NULL || request == NULL || request->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_lookup_float32_clear(lut);
    lut->policy = SIXEL_LUT_POLICY_FHEDT;
    lut->depth = request->depth;
    lut->ncolors = request->reqcolor;
    lut->complexion = 1;

    base_weights[0] = 1.0f;
    base_weights[1] = 1.0f;
    base_weights[2] = 1.0f;
    for (component = 0; component < SIXEL_LOOKUP_FLOAT_COMPONENTS;
            ++component) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat,
                                                      component);
        if (range <= 0.0f) {
            range = 1.0f;
        }
        scale = 1.0f / range;
        lut->weights[component] = base_weights[component] * scale * scale;
    }

    status = sixel_lookup_policy_fhedt_prepare_float_palette(
        lut,
        request->palette,
        request->palette_float,
        request->float_depth,
        request->pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (lut->fhedt == NULL) {
        status = sixel_lookup_fhedt_float32_create(lut->allocator, &lut->fhedt);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_lookup_policy_fhedt: FHEDT allocation failed.");
            return status;
        }
    }

    resolution = sixel_lookup_policy_fhedt_env_resolution();
    refine = sixel_lookup_policy_fhedt_env_refine();
    shared_flag = sixel_lookup_policy_fhedt_env_shared();
    use_dist2 = sixel_lookup_policy_fhedt_env_use_dist2();
    use_cache = sixel_lookup_policy_fhedt_env_use_cache();

    signature = sixel_lookup_fhedt_float32_signature(lut->palette,
                                                     lut->ncolors,
                                                     resolution,
                                                     refine,
                                                     lut->weights[0],
                                                     lut->weights[1],
                                                     lut->weights[2],
                                                     lut->depth,
                                                     request->pixelformat);
    status = sixel_lookup_fhedt_float32_configure(lut->fhedt,
                                                  lut->palette,
                                                  lut->ncolors,
                                                  resolution,
                                                  refine,
                                                  use_dist2,
                                                  use_cache,
                                                  shared_flag,
                                                  lut->weights[0],
                                                  lut->weights[1],
                                                  lut->weights[2],
                                                  request->pixelformat);
    if (SIXEL_FAILED(status)) {
        lut->fhedt_ready = 0;
        return status;
    }

    sixel_lookup_fhedt_float32_shared_set_signature(lut->fhedt->shared,
                                                    signature);
    lut->fhedt_ready = 1;
    return SIXEL_OK;
}

static int
sixel_lookup_policy_fhedt_map_float32(sixel_lookup_float32_t const *lut,
                                      unsigned char const *pixel)
{
    float const *sample;

    sample = NULL;
    if (lut == NULL || pixel == NULL || lut->fhedt_ready == 0
            || lut->fhedt == NULL) {
        return 0;
    }

    sample = (float const *)(void const *)pixel;
    return sixel_lookup_fhedt_float32_map(lut->fhedt, sample);
}

static int
sixel_lookup_policy_fhedt_map_8bit(sixel_lookup_8bit_t const *lut,
                                   unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL || lut->fhedt_ready == 0
            || lut->fhedt == NULL) {
        return 0;
    }

    return sixel_lookup_fhedt_8bit_map(lut->fhedt, pixel);
}

static void
sixel_lookup_policy_fhedt_reset_state(
    sixel_lookup_policy_fhedt_object_t *object)
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
sixel_lookup_policy_fhedt_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_fhedt_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_fhedt_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_fhedt_unref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_fhedt_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_fhedt_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        sixel_lookup_policy_fhedt_reset_state(object);
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_fhedt_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    SIXELSTATUS status;
    sixel_lookup_policy_fhedt_object_t *object;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_fhedt_object_t *reuse_object;
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

    object = sixel_lookup_policy_fhedt_from_base(policy);
    sixel_lookup_policy_fhedt_reset_state(object);

    if (request->depth != 3) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: fast lookup requires RGB pixels.");
        return SIXEL_BAD_ARGUMENT;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        SIXEL_LUT_POLICY_FHEDT);
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
        reuse_object = sixel_lookup_policy_fhedt_from_base(reuse_policy);
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

    if (object->lookup_source_is_float != 0) {
        status = sixel_lookup_policy_fhedt_configure_float32(
            sixel_lut_backend_float32(object->lut),
            request);
    } else {
        status = sixel_lookup_policy_fhedt_configure_8bit(
            sixel_lut_backend_8bit(object->lut),
            request);
    }
    if (SIXEL_FAILED(status)) {
        sixel_lookup_policy_fhedt_reset_state(object);
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
sixel_lookup_policy_fhedt_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_fhedt_object_t const *object;

    object = NULL;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_fhedt_from_base_const(policy);
    if (object->lut == NULL) {
        return 0;
    }

    if (object->lookup_source_is_float != 0) {
        return sixel_lookup_policy_fhedt_map_float32(
            sixel_lut_backend_float32(object->lut),
            pixel);
    }

    return sixel_lookup_policy_fhedt_map_8bit(
        sixel_lut_backend_8bit(object->lut),
        pixel);
}

static int
sixel_lookup_policy_fhedt_lookup_source_is_float(
    sixel_lookup_policy_interface_t const *policy)
{
    sixel_lookup_policy_fhedt_object_t const *object;

    object = NULL;
    if (policy == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_fhedt_from_base_const(policy);
    return object->lookup_source_is_float;
}

static int
sixel_lookup_policy_fhedt_prefer_palette_float_lookup(
    sixel_lookup_policy_interface_t const *policy)
{
    (void)policy;
    return 0;
}

static sixel_lookup_policy_vtbl_t const g_sixel_lookup_policy_fhedt_vtbl = {
    sixel_lookup_policy_fhedt_ref,
    sixel_lookup_policy_fhedt_unref,
    sixel_lookup_policy_fhedt_prepare,
    sixel_lookup_policy_fhedt_map_pixel,
    sixel_lookup_policy_fhedt_lookup_source_is_float,
    sixel_lookup_policy_fhedt_prefer_palette_float_lookup
};

SIXELSTATUS
sixel_lookup_policy_create_fhedt(sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_fhedt_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_fhedt_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_fhedt: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_fhedt_vtbl;
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
