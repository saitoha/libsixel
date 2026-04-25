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

#include "dither-policy-backend.h"
#include "dither.h"
#include "dither-internal.h"
#include "dither-common-pipeline.h"
#include "dither-positional-8bit.h"
#include "dither-positional-float32.h"
#include "dither-fixed-8bit.h"
#include "dither-fixed-float32.h"
#include "dither-varcoeff-8bit.h"
#include "dither-varcoeff-float32.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

typedef struct sixel_dither_policy_backend_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_backend_object_t;

static sixel_dither_policy_backend_object_t *
sixel_dither_policy_backend_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_backend_object_t *)(void *)policy;
}

static sixel_dither_policy_backend_object_t const *
sixel_dither_policy_backend_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_backend_object_t const *)(void const *)policy;
}

void
sixel_dither_policy_backend_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_backend_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_backend_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

void
sixel_dither_policy_backend_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_backend_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_backend_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

SIXELSTATUS
sixel_dither_policy_backend_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_backend_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_backend_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_dither_policy_backend_create(
    sixel_dither_policy_interface_t **policy,
    sixel_dither_policy_vtbl_t const *vtbl)
{
    sixel_dither_policy_backend_object_t *object;

    object = NULL;
    if (policy == NULL || vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_backend_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = vtbl;
    object->ref = 1U;
    object->method_for_scan = SIXEL_SCAN_AUTO;
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;

    *policy = &object->base;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_dither_policy_backend_apply_fixed(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    sixel_dither_policy_backend_object_t const *object;
    sixel_dither_policy_apply_request_t effective;

    object = NULL;
    memset(&effective, 0, sizeof(effective));
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_backend_from_base_const(policy);
    effective = *request;
    effective.method_for_scan = object->method_for_scan;
    effective.pixelformat = object->pixelformat;
    return sixel_dither_policy_apply_fixed_backend(&effective,
                                                   method_for_diffuse);
}

SIXELSTATUS
sixel_dither_policy_backend_apply_varcoeff(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    sixel_dither_policy_backend_object_t const *object;
    sixel_dither_policy_apply_request_t effective;

    object = NULL;
    memset(&effective, 0, sizeof(effective));
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_backend_from_base_const(policy);
    effective = *request;
    effective.method_for_scan = object->method_for_scan;
    effective.pixelformat = object->pixelformat;
    return sixel_dither_policy_apply_varcoeff_backend(&effective,
                                                      method_for_diffuse);
}

SIXELSTATUS
sixel_dither_policy_backend_apply_positional(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    sixel_dither_policy_backend_object_t const *object;
    sixel_dither_policy_apply_request_t effective;

    object = NULL;
    memset(&effective, 0, sizeof(effective));
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_backend_from_base_const(policy);
    effective = *request;
    effective.method_for_scan = object->method_for_scan;
    effective.pixelformat = object->pixelformat;
    return sixel_dither_policy_apply_positional_backend(&effective,
                                                        method_for_diffuse);
}

static SIXELSTATUS
sixel_dither_policy_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_context_t *context,
    unsigned char scratch[SIXEL_MAX_CHANNELS],
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4],
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS],
    unsigned short migration_map[SIXEL_PALETTE_MAX])
{
    sixel_dither_lookup_map_fn lookup_map;
    sixel_dither_t *dither;

    lookup_map = NULL;
    dither = NULL;

    if (request == NULL || context == NULL || request->lookup_policy == NULL
            || request->lookup_policy->vtbl == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (request->reqcolor < 1) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: "
            "a bad argument is detected, reqcolor < 0.");
        return SIXEL_BAD_ARGUMENT;
    }

    memset(context, 0, sizeof(*context));
    context->result = request->result;
    context->width = request->width;
    context->height = request->height;
    context->band_origin = request->band_origin;
    context->output_start = request->output_start;
    context->depth = request->depth;
    context->palette = request->palette;
    context->reqcolor = request->reqcolor;
    context->new_palette = new_palette;
    context->migration_map = migration_map;
    context->ncolors = request->ncolors;
    context->scratch = scratch;
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;
    context->optimize_palette = request->foptimize_palette;
    context->complexion = request->complexion;

    lookup_map = request->lookup_policy->vtbl->map_pixel;
    context->lookup_map = lookup_map;
    context->lookup_source_is_float =
        request->lookup_policy->vtbl->lookup_source_is_float(
            request->lookup_policy);
    context->prefer_palette_float_lookup =
        request->lookup_policy->vtbl->prefer_palette_float_lookup(
            request->lookup_policy);

    if (lookup_map == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: lookup policy is not prepared.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        context->pixels_float = (float *)(void *)request->data;
    }

    dither = request->dither;
    if (dither != NULL && dither->palette != NULL) {
        sixel_palette_t *palette_object;
        int float_components;

        palette_object = dither->palette;
        if (palette_object->entries_float32 != NULL
                && palette_object->float_depth > 0) {
            float_components = palette_object->float_depth
                / (int)sizeof(float);
            if (float_components > 0
                    && (size_t)float_components <= SIXEL_MAX_CHANNELS) {
                context->palette_float = palette_object->entries_float32;
                context->float_depth = float_components;
                context->new_palette_float = new_palette_float;
            }
        }
    }

    if (dither != NULL
            && dither->pipeline_transparent_mask != NULL
            && dither->pipeline_transparent_keycolor >= 0
            && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX) {
        context->transparent_mask = dither->pipeline_transparent_mask;
        context->transparent_mask_size = dither->pipeline_transparent_mask_size;
        context->transparent_keycolor = dither->pipeline_transparent_keycolor;
    }

    if (dither != NULL && dither->bluenoise_gradient_map != NULL) {
        context->bluenoise_gradient_map = dither->bluenoise_gradient_map;
        context->bluenoise_gradient_map_size =
            dither->bluenoise_gradient_map_size;
        context->bluenoise_gradient_width = dither->bluenoise_gradient_width;
        context->bluenoise_gradient_height = dither->bluenoise_gradient_height;
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_dither_policy_apply_fixed_backend(
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    SIXELSTATUS status;
    sixel_dither_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned short migration_map[SIXEL_PALETTE_MAX];

    status = SIXEL_FALSE;
    memset(scratch, 0, sizeof(scratch));
    memset(new_palette, 0, sizeof(new_palette));
    memset(new_palette_float, 0, sizeof(new_palette_float));
    memset(migration_map, 0, sizeof(migration_map));

    status = sixel_dither_policy_build_context(request,
                                               &context,
                                               scratch,
                                               new_palette,
                                               new_palette_float,
                                               migration_map);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    context.method_for_diffuse = method_for_diffuse;

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(context.pixelformat)
            && context.pixels_float != NULL
            && context.depth == 3
            && request->dither != NULL
            && request->dither->prefer_float32 != 0) {
        status = sixel_dither_apply_fixed_float32(request->dither, &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_fixed_8bit(request->dither,
                                                   &context);
        }
    } else {
        status = sixel_dither_apply_fixed_8bit(request->dither, &context);
    }

    return status;
}

SIXELSTATUS
sixel_dither_policy_apply_varcoeff_backend(
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    SIXELSTATUS status;
    sixel_dither_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned short migration_map[SIXEL_PALETTE_MAX];

    status = SIXEL_FALSE;
    memset(scratch, 0, sizeof(scratch));
    memset(new_palette, 0, sizeof(new_palette));
    memset(new_palette_float, 0, sizeof(new_palette_float));
    memset(migration_map, 0, sizeof(migration_map));

    status = sixel_dither_policy_build_context(request,
                                               &context,
                                               scratch,
                                               new_palette,
                                               new_palette_float,
                                               migration_map);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    context.method_for_diffuse = method_for_diffuse;

    if (context.pixels_float != NULL
            && request->dither != NULL
            && request->dither->prefer_float32 != 0) {
        status = sixel_dither_apply_varcoeff_float32(request->dither,
                                                     &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_varcoeff_8bit(request->dither,
                                                      &context);
        }
    } else {
        status = sixel_dither_apply_varcoeff_8bit(request->dither, &context);
    }

    return status;
}

SIXELSTATUS
sixel_dither_policy_apply_positional_backend(
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    SIXELSTATUS status;
    sixel_dither_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned short migration_map[SIXEL_PALETTE_MAX];

    status = SIXEL_FALSE;
    memset(scratch, 0, sizeof(scratch));
    memset(new_palette, 0, sizeof(new_palette));
    memset(new_palette_float, 0, sizeof(new_palette_float));
    memset(migration_map, 0, sizeof(migration_map));

    status = sixel_dither_policy_build_context(request,
                                               &context,
                                               scratch,
                                               new_palette,
                                               new_palette_float,
                                               migration_map);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    context.method_for_diffuse = method_for_diffuse;

    if (context.pixels_float != NULL
            && request->dither != NULL
            && request->dither->prefer_float32 != 0) {
        status = sixel_dither_apply_positional_float32(request->dither,
                                                       &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_positional_8bit(request->dither,
                                                        &context);
        }
    } else {
        status = sixel_dither_apply_positional_8bit(request->dither, &context);
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
