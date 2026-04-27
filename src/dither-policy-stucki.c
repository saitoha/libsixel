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
#include "dither-policy-stucki.h"
#include "dither.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * Private dither context for this policy implementation.
 * Keep only members used by this translation unit.
 */
typedef struct sixel_dither_policy_stucki_context {
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
    int reqcolor;
    int method_for_scan;
    struct sixel_lookup_policy_interface *lookup_policy;
    sixel_dither_lookup_map_fn lookup_map;
    unsigned char *scratch;
    int *ncolors;
    int pixelformat;
    int float_depth;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
} sixel_dither_policy_stucki_context_t;

static void
sixel_dither_scanline_params_fixed_8bit(int serpentine,
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

/* Shared diffusion helper kernels. */
static void
error_diffuse_precise(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = (int)(*data + error * numerator / (double)denominator + 0.5);
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

static void diffuse_stucki(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static SIXELSTATUS
sixel_dither_apply_stucki_8bit(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int band_origin,
    int output_start,
    int depth,
    unsigned char *palette,
    int reqcolor,
    int method_for_scan,
    sixel_lookup_policy_interface_t const *lookup_policy,
    sixel_dither_lookup_map_fn lookup_map,
    int *ncolors,
    sixel_dither_t *dither)
{
    SIXELSTATUS status;
    int serpentine;
    int y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int absolute_y;
    int pos;
    size_t base;
    unsigned char const *source_pixel;
    int color_index;
    int output_index;
    int n;
    int palette_value;
    int offset;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;

    status = SIXEL_FALSE;

    if (dither == NULL || result == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (data == NULL || palette == NULL || ncolors == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (lookup_policy == NULL || lookup_map == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (depth > SIXEL_MAX_CHANNELS) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = (-1);
    use_transparent_fence = 0;
    if (dither != NULL
            && dither->pipeline_transparent_mask != NULL
            && dither->pipeline_transparent_keycolor >= 0
            && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX) {
        transparent_mask = dither->pipeline_transparent_mask;
        transparent_mask_size = dither->pipeline_transparent_mask_size;
        transparent_keycolor = dither->pipeline_transparent_keycolor;
        use_transparent_fence = 1;
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);
    *ncolors = reqcolor;

    for (y = 0; y < height; ++y) {
        absolute_y = band_origin + y;
        sixel_dither_scanline_params_fixed_8bit(
            serpentine, absolute_y, width,
            &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * width + x;
            base = (size_t)pos * (size_t)depth;
            is_transparent = 0;
            if (use_transparent_fence && absolute_y >= 0) {
                absolute_index = (size_t)absolute_y * (size_t)width
                    + (size_t)x;
                if (absolute_index < transparent_mask_size
                        && transparent_mask[absolute_index] != 0U) {
                    is_transparent = 1;
                }
            }
            if (is_transparent) {
                if (absolute_y >= output_start) {
                    result[pos] = (sixel_index_t)transparent_keycolor;
                }
                continue;
            }

            source_pixel = data + base;
            color_index = lookup_map(lookup_policy, source_pixel);
            output_index = color_index;

            if (absolute_y >= output_start) {
                /*
                 * Palette indices are bounded by SIXEL_PALETTE_MAX,
                 * which fits in sixel_index_t (unsigned char).
                 */
                result[pos] = (sixel_index_t)output_index;
            }

            for (n = 0; n < depth; ++n) {
                palette_value = palette[color_index * depth + n];
                offset = (int)source_pixel[n] - palette_value;
                diffuse_stucki(data + n, width, height, x, y,
                          depth, offset, direction);
            }
        }
        if (absolute_y >= output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    status = SIXEL_OK;

end:
    return status;
}

static void
diffuse_stucki(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_precise(data,
                              pos + (neighbor - x),
                              depth, error,
                              row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row1_num[i], row1_den[i]);
        }
    }
    if (y < height - 2) {
        int row;

        row = pos + width * 2;
        for (i = 0; i < 5; ++i) {
            int neighbor;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_precise(data,
                                  row + (neighbor - x),
                                  depth, error,
                                  row2_num[i], row2_den[i]);
        }
    }
}


static void
error_diffuse_float(float *data,
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
    *channel = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                     channel_index,
                                                     *channel);
}

static void
sixel_dither_scanline_params_fixed_float32(int serpentine,
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
 * Atkinson's kernel spreads the error within a 3x3 neighborhood using
 * symmetric 1/8 weights.  The float variant mirrors the integer version
 * but keeps the higher precision samples intact.
 */

/*
 * Shared helper that applies a row of diffusion weights to neighbors on the
 * current or subsequent scanlines.  Each caller provides the offset table and
 * numerator/denominator pairs so the classic kernels can be described using a
 * compact table instead of open-coded loops.
 */
static void
diffuse_weighted_row(float *data,
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
        error_diffuse_float(data,
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

/*
 * Stucki's method spreads the error across a 5x3 neighborhood with larger
 * emphasis on closer pixels.  The numerators/denominators match the classic
 * 8/48, 4/48, and related fractions from the integer backend.
 */
static void
diffuse_stucki_float(float *data,
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
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int pos;

    pos = y * width + x;
    diffuse_weighted_row(data,
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
        diffuse_weighted_row(data,
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
        diffuse_weighted_row(data,
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

/*
 * Burkes' kernel limits the spread to two rows to reduce directional artifacts
 * while keeping the symmetric 1/16-4/16 pattern.
 */

/*
 * Sierra Lite (Sierra1) uses a compact 2x2 mask to reduce ringing while
 * keeping serpentine traversal stable.
 */

/*
 * Sierra Two-row keeps the full 5x3 footprint but halves the lower row weights
 * relative to Sierra-3, matching the 32-denominator formulation.
 */

/*
 * Sierra-3 restores the heavier middle-row contributions (5/32) that
 * characterize the original kernel.
 */

static SIXELSTATUS
sixel_dither_apply_stucki_float32(
    sixel_dither_t *dither,
    sixel_dither_policy_stucki_context_t *context)
{
    SIXELSTATUS status;
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
    /* Keep lookup inputs initialized across all branch combinations. */
    unsigned char quantized[SIXEL_MAX_CHANNELS] = { 0 };
    float working_float[SIXEL_MAX_CHANNELS] = { 0.0f };
    float lookup_pixel_float[SIXEL_MAX_CHANNELS] = { 0.0f };
    int color_index;
    int output_index;
    unsigned char palette_value_u8;
    float palette_value_float;
    float error;
    int n;
    float *data;
    unsigned char *palette;
    int lookup_wants_float;
    int need_float_pixel;
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
    if (context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    palette = context->palette;
    palette_float = context->palette_float;
    float_depth = context->float_depth;
    if (context->depth > SIXEL_MAX_CHANNELS || context->depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->reqcolor < 1) {
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
    lookup_wants_float = (context->lookup_source_is_float != 0);
    need_float_pixel = lookup_wants_float;

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

    *context->ncolors = context->reqcolor;

    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        sixel_dither_scanline_params_fixed_float32(
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
                if (!lookup_wants_float) {
                    quantized[n] = sixel_pixelformat_float_channel_to_byte(
                        context->pixelformat,
                        n,
                        source_pixel[n]);
                }
                if (need_float_pixel) {
                    lookup_pixel_float[n] = working_float[n];
                }
            }

            if (lookup_wants_float) {
                lookup_pixel = (unsigned char const *)(void const *)
                    working_float;
                color_index = context->lookup_map(context->lookup_policy,
                                                  lookup_pixel);
            } else {
                lookup_pixel = quantized;
                color_index = context->lookup_map(context->lookup_policy,
                                                  lookup_pixel);
            }

                output_index = color_index;
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)output_index;
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
                error = working_float[n] - palette_value_float;
                source_pixel[n] = palette_value_float;
                diffuse_stucki_float(data + (size_t)n,
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

typedef struct sixel_dither_policy_stucki_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_stucki_object_t;

static sixel_dither_policy_stucki_object_t *
sixel_dither_policy_stucki_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_stucki_object_t *)(void *)policy;
}

static sixel_dither_policy_stucki_object_t const *
sixel_dither_policy_stucki_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_stucki_object_t const *)(void const *)policy;
}

static void
sixel_dither_policy_stucki_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_stucki_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_stucki_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_stucki_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_stucki_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_stucki_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_dither_policy_stucki_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_stucki_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_stucki_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_stucki_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective)
{
    sixel_dither_policy_stucki_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_stucki_from_base_const(policy);
    *effective = *request;
    effective->method_for_scan = object->method_for_scan;
    effective->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_stucki_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_stucki_context_t *context,
    unsigned char scratch[SIXEL_MAX_CHANNELS])
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
    context->ncolors = request->ncolors;
    context->scratch = scratch;
    context->lookup_policy = request->lookup_policy;
    context->pixels = request->data;
    context->pixelformat = request->pixelformat;
    context->method_for_scan = request->method_for_scan;

    lookup_map = request->lookup_policy->vtbl->map_pixel;
    context->lookup_map = lookup_map;
    context->lookup_source_is_float =
        SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);
    context->prefer_palette_float_lookup = 0;

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
sixel_dither_policy_stucki_apply(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_stucki_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));
    memset(scratch, 0, sizeof(scratch));

    status = sixel_dither_policy_stucki_make_effective_request(policy,
                                                           request,
                                                           &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_stucki_build_context(&effective,
                                                  &context,
                                                  scratch);
    if (SIXEL_FAILED(status)) {
        return status;
    }

        if (SIXEL_PIXELFORMAT_IS_FLOAT32(context.pixelformat)
            && context.pixels_float != NULL
            && context.depth == 3
            && effective.dither != NULL
            && effective.dither->prefer_float32 != 0) {
        status = sixel_dither_apply_stucki_float32(
            effective.dither,
            &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_stucki_8bit(
            context.result,
            context.pixels,
            context.width,
            context.height,
            context.band_origin,
            context.output_start,
            context.depth,
            context.palette,
            context.reqcolor,
            context.method_for_scan,
            context.lookup_policy,
            context.lookup_map,
            context.ncolors,
            effective.dither);
        }
    } else {
        status = sixel_dither_apply_stucki_8bit(
            context.result,
            context.pixels,
            context.width,
            context.height,
            context.band_origin,
            context.output_start,
            context.depth,
            context.palette,
            context.reqcolor,
            context.method_for_scan,
            context.lookup_policy,
            context.lookup_map,
            context.ncolors,
            effective.dither);
    }

    return status;
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_stucki_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_stucki_vtbl = {
    sixel_dither_policy_stucki_ref,
    sixel_dither_policy_stucki_unref,
    sixel_dither_policy_stucki_prepare,
    sixel_dither_policy_stucki_apply,
    sixel_dither_policy_stucki_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_create_stucki(
    sixel_dither_policy_interface_t **policy)
{
    sixel_dither_policy_stucki_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_stucki_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_stucki_vtbl;
    object->ref = 1U;
    object->method_for_scan = SIXEL_SCAN_AUTO;
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;

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
