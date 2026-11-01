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
#include <stdint.h>
#include <limits.h>
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#include "compat_stub.h"
#include "dither-policy-jajuni.h"
#include "dither.h"
#include "dither-policy-private.h"
#include "dither-carry.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * Private dither context for this policy implementation.
 * Keep only members used by this translation unit.
 */
typedef struct sixel_dither_policy_jajuni_context {
    sixel_dither_clamp_t clamp;
    sixel_index_t *result;
    unsigned char *pixels;
    float *pixels_float;
    int width;
    int height;
    int band_origin;
    int output_start;
    int depth;
    unsigned char *palette;
    float *palette_float;
    int method_for_scan;
    struct sixel_lookup_policy_interface *lookup_policy;
    int pixelformat;
    int float_depth;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
} sixel_dither_policy_jajuni_context_t;

static void
jajuni_error_diffuse_float(float *data,
                    int pos,
                    int depth,
                    float error,
                    int numerator,
                    int denominator,
                    int pixelformat,
                    int channel_index)
{
    float *channel;
    float delta;

    channel = data + ((size_t)pos * (size_t)depth);
    delta = error * ((float)numerator / (float)denominator);
    *channel += delta;
    /* Limiting happens once before lookup, after all incoming taps. */
    (void)pixelformat;
    (void)channel_index;
}

static void
jajuni_sixel_dither_scanline_params_fixed_float32(int serpentine,
                             int index,
                             int limit,
                             int *start,
                             int *end,
                             int *step,
                             int *direction)
{
    if (serpentine && (index & 1)) {
        *start = limit - 1;
        *end = -1;
        *step = -1;
        *direction = -1;
    } else {
        *start = 0;
        *end = limit;
        *step = 1;
        *direction = 1;
    }
}

/*
 * Shared helper that applies a row of diffusion weights to neighbors on the
 * current or subsequent scanlines.  Each caller provides the offset table and
 * numerator/denominator pairs so the classic kernels can be described using a
 * compact table instead of open-coded loops.
 */
static void
jajuni_diffuse_weighted_row(float *data,
                     int pos,
                     int depth,
                     float error,
                     int direction,
                     int pixelformat,
                     int channel_index,
                     int x,
                     int width,
                     int row_offset,
                     int const *offsets,
                     int const *numerators,
                     int const *denominators,
                     int count)
{
    int i;
    int neighbor;
    int row_base;
    int sign;

    sign = direction >= 0 ? 1 : -1;
    row_base = pos + row_offset;
    for (i = 0; i < count; ++i) {
        neighbor = x + sign * offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        jajuni_error_diffuse_float(data,
                            row_base + (neighbor - x),
                            depth,
                            error,
                            numerators[i],
                            denominators[i],
                            pixelformat,
                            channel_index);
    }
}

/*
 * Jarvis, Judice, and Ninke kernel using the canonical 5x3 mask.  Three rows
 * of weights are applied with consistent 1/48 denominators to preserve the
 * reference diffusion matrix.
 */
static void
jajuni_diffuse_jajuni_float(float *data,
                     int width,
                     int height,
                     int x,
                     int y,
                     int depth,
                     float error,
                     int direction,
                     int pixelformat,
                     int channel_index)
{
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 7, 5 };
    static const int row0_den[] = { 48, 48 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 3, 5, 7, 5, 3 };
    static const int row1_den[] = { 48, 48, 48, 48, 48 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 3, 5, 3, 1 };
    static const int row2_den[] = { 48, 48, 48, 48, 48 };
    int pos;

    pos = y * width + x;
    jajuni_diffuse_weighted_row(data,
                         pos,
                         depth,
                         error,
                         direction,
                         pixelformat,
                         channel_index,
                         x,
                         width,
                         0,
                         row0_offsets,
                         row0_num,
                         row0_den,
                         2);
    if (y < height - 1) {
        jajuni_diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width,
                             row1_offsets,
                             row1_num,
                             row1_den,
                             5);
    }
    if (y < height - 2) {
        jajuni_diffuse_weighted_row(data,
                             pos,
                             depth,
                             error,
                             direction,
                             pixelformat,
                             channel_index,
                             x,
                             width,
                             width * 2,
                             row2_offsets,
                             row2_num,
                             row2_den,
                             5);
    }
}

static SIXELSTATUS
sixel_dither_apply_jajuni_float32(
    sixel_dither_t *dither,
    sixel_dither_policy_jajuni_context_t *context
    )
{
    int lookup_hint;
    SIXELSTATUS status;
    float selected[SIXEL_MAX_CHANNELS];
    float retained[SIXEL_MAX_CHANNELS];
    int keep;
    float *palette_float;
    int float_depth;
    int serpentine;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    float *source_pixel;
    float working_float[SIXEL_MAX_CHANNELS] = { 0.0f };
    int color_index;
    int output_index;
    unsigned char palette_value_u8;
    float palette_value_float;
    float error;
    int n;
    float *data;
    unsigned char *palette;
    unsigned char const *lookup_pixel;
    int have_palette_float;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;

    palette_float = NULL;
    float_depth = 0;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    data = context->pixels_float;
    if (data == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_policy->vtbl == NULL
            || context->lookup_policy->vtbl->map_pixel == NULL
            || context->lookup_policy->vtbl->map_pixel_hint == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = context->palette;
    palette_float = context->palette_float;
    float_depth = context->float_depth;
    if (context->depth > SIXEL_MAX_CHANNELS || context->depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }

    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    use_transparent_fence = 0;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    /*
     * Remember whether each palette buffer exposes float32 components so
     * later loops can preserve precision instead of converting back to
     * bytes before computing the diffusion error.
     */
    if (palette_float != NULL && float_depth >= context->depth) {
        have_palette_float = 1;
    } else {
        have_palette_float = 0;
    }

    lookup_hint = -1;
    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        jajuni_sixel_dither_scanline_params_fixed_float32(
            serpentine, absolute_y, context->width,
            &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            base = (size_t)pos * (size_t)context->depth;
            is_transparent = 0;
            if (use_transparent_fence && absolute_y >= 0) {
                absolute_index = (size_t)absolute_y
                    * (size_t)context->width + (size_t)x;
                if (absolute_index < transparent_mask_size
                        && transparent_mask[absolute_index] != 0U) {
                    is_transparent = 1;
                }
            }
            if (is_transparent) {
                if (absolute_y >= context->output_start) {
                    context->result[pos]
                        = (sixel_index_t)transparent_keycolor;
                }
                continue;
            }

            source_pixel = data + base;
            for (n = 0; n < context->depth; ++n) {
                working_float[n] = source_pixel[n];
            }

            status = sixel_dither_clamp_pixel(
                &context->clamp, (size_t)pos, working_float);
            if (SIXEL_FAILED(status)) {
                return status;
            }

            lookup_pixel = (unsigned char const *)(void const *)
                working_float;
            color_index = context->lookup_policy->vtbl->map_pixel_hint(
                context->lookup_policy,
                lookup_pixel,
                lookup_hint);
            lookup_hint = color_index;

            /* Native and promoted-byte delta share retained-color
             * competition without rounding the corrected working sample. */
            for (n = 0; n < context->depth; ++n) {
                selected[n] = have_palette_float
                    ? palette_float[color_index * float_depth + n]
                    : sixel_pixelformat_byte_to_float(context->pixelformat,
                        n, palette[color_index * context->depth + n]);
            }
            keep = 0;
            if (absolute_y >= 0) {
                keep = sixel_dither_6delta_keep_float(dither,
                    (size_t)absolute_y * (size_t)context->width + (size_t)x,
                    x, absolute_y, context->pixelformat, working_float,
                    selected, color_index,
                    absolute_y >= context->output_start, retained);
            }

            output_index = keep
                ? dither->pipeline_accumulation_keycolor : color_index;
            if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)output_index;
            }

            if (keep && sixel_dither_pipeline_6delta_error_mode(dither)
                    != SIXEL_6DELTA_ERROR_DIFFUSE) {
                continue;
            }

            for (n = 0; n < context->depth; ++n) {
                palette_value_u8 =
                    palette[color_index * context->depth + n];
                if (have_palette_float) {
                    palette_value_float =
                        palette_float[color_index * float_depth + n];
                } else {
                    palette_value_float
                        = sixel_pixelformat_byte_to_float(
                              context->pixelformat,
                              n,
                              palette_value_u8);
                }
                if (keep) {
                    palette_value_float = retained[n];
                }
                error = working_float[n] - palette_value_float;
                source_pixel[n] = palette_value_float;
                jajuni_diffuse_jajuni_float(data + (size_t)n,
                          context->width,
                          context->height,
                          x,
                          y,
                          context->depth,
                          error,
                          direction,
                          context->pixelformat,
                          n);
            }
        }
        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    status = SIXEL_OK;
    return status;
}

/*
 * IDL (internal contract)
 *
 * class DitherPolicy : IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 */

typedef sixel_dither_policy_object_t sixel_dither_policy_jajuni_object_t;

static sixel_dither_policy_jajuni_object_t *
sixel_dither_policy_jajuni_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_jajuni_object_t *)(void *)policy;
}

static sixel_dither_policy_jajuni_object_t const *
sixel_dither_policy_jajuni_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_jajuni_object_t const *)(void const *)policy;
}

static void
sixel_dither_policy_jajuni_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_jajuni_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_jajuni_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_jajuni_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_jajuni_object_t *object;
    unsigned int previous;
    sixel_allocator_t *allocator;

    object = NULL;
    previous = 0U;
    allocator = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_jajuni_from_base(policy);
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
sixel_dither_policy_jajuni_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_jajuni_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_jajuni_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    object->options = request->options;
    object->options.method_for_diffuse = SIXEL_DIFFUSE_JAJUNI;
    object->prepared = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_jajuni_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_effective_t *effective)
{
    sixel_dither_policy_jajuni_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_jajuni_from_base_const(policy);
    return sixel_dither_policy_bind(object, request, effective);
}

static SIXELSTATUS
sixel_dither_policy_jajuni_build_context(
    sixel_dither_policy_effective_t const *request,
    sixel_dither_policy_jajuni_context_t *context)
{
    sixel_dither_t *dither;

    dither = NULL;

    if (request == NULL || context == NULL || request->lookup_policy == NULL
            || request->lookup_policy->vtbl == NULL) {
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
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;

    if (request->lookup_policy->vtbl->map_pixel == NULL
            || request->lookup_policy->vtbl->map_pixel_hint == NULL) {
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
        sixel_palette_float32_entries_view_t float32_view;
        int float_components;

        palette_object = dither->palette;
        memset(&float32_view, 0, sizeof(float32_view));
        if (palette_object->vtbl != NULL
                && palette_object->vtbl->get_entries_float32 != NULL
                && SIXEL_SUCCEEDED(
                    palette_object->vtbl->get_entries_float32(
                        palette_object,
                        &float32_view))
                && float32_view.entries != NULL
                && float32_view.depth > 0) {
            float_components = float32_view.depth / (int)sizeof(float);
            if (float_components > 0
                    && (size_t)float_components <= SIXEL_MAX_CHANNELS) {
                context->palette_float = float32_view.entries;
                context->float_depth = float_components;
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

    return SIXEL_OK;
}

 static SIXELSTATUS
sixel_dither_policy_jajuni_apply_8bit(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_effective_t effective;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_jajuni_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_carry_apply(&effective,
                                    SIXEL_DIFFUSE_JAJUNI);
}

static SIXELSTATUS
sixel_dither_policy_jajuni_apply_float32(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_effective_t effective;
    sixel_dither_policy_jajuni_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_jajuni_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_jajuni_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (effective.dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_dither_clamp_init(
        &context.clamp, context.pixels_float, context.width, context.height,
        context.depth, context.pixelformat, effective.dither->clamp_policy,
        effective.dither->clamp_threshold, effective.dither->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_dither_apply_jajuni_float32(
        effective.dither,
        &context);
    sixel_dither_clamp_finish(&context.clamp);
    return status;
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_jajuni_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_jajuni_vtbl = {
    sixel_dither_policy_jajuni_ref,
    sixel_dither_policy_jajuni_unref,
    sixel_dither_policy_jajuni_prepare,
    sixel_dither_policy_get_configuration,
    sixel_dither_policy_jajuni_apply_8bit,
    sixel_dither_policy_jajuni_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_jajuni_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    sixel_dither_policy_jajuni_object_t *object;

    object = NULL;
    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_jajuni_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_jajuni_vtbl;
    object->ref = 1U;
    object->allocator = allocator;
    object->prepared = 0;
    sixel_allocator_ref(allocator);
    object->method_for_scan = SIXEL_SCAN_AUTO;
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_jajuni_8bit_vtbl = {
    sixel_dither_policy_jajuni_ref,
    sixel_dither_policy_jajuni_unref,
    sixel_dither_policy_jajuni_prepare,
    sixel_dither_policy_get_configuration,
    sixel_dither_policy_jajuni_apply_8bit,
    sixel_dither_policy_jajuni_supports_parallel_bands
};

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_jajuni_float32_vtbl = {
    sixel_dither_policy_jajuni_ref,
    sixel_dither_policy_jajuni_unref,
    sixel_dither_policy_jajuni_prepare,
    sixel_dither_policy_get_configuration,
    sixel_dither_policy_jajuni_apply_float32,
    sixel_dither_policy_jajuni_supports_parallel_bands
};

SIXELSTATUS
sixel_dither_policy_jajuni_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_jajuni_new(allocator, policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        ((sixel_dither_policy_interface_t *)(*policy))->vtbl = &g_sixel_dither_policy_jajuni_8bit_vtbl;
    }

    return status;
}

SIXELSTATUS
sixel_dither_policy_jajuni_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_jajuni_new(allocator, policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        ((sixel_dither_policy_interface_t *)(*policy))->vtbl = &g_sixel_dither_policy_jajuni_float32_vtbl;
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
