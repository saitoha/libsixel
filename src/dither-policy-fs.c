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
#include "dither-policy-fs.h"
#include "dither.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * Private dither context for this policy implementation.
 * Keep only members used by this translation unit.
 */
typedef struct sixel_dither_policy_fs_context {
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
    sixel_dither_perturb_t perturb;
} sixel_dither_policy_fs_context_t;

/*
 * lowbias32 by Chris Wellons (hash-prospector, public domain).
 * Low bias on sequential integer inputs avoids coordinate-aligned patterns.
 */
static uint32_t
sixel_dither_perturb_mix32(uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v;
}

uint32_t
sixel_dither_perturb_hash(int x, int absolute_y, uint32_t seed, int pair)
{
    uint32_t h;

    h = sixel_dither_perturb_mix32((uint32_t)x + seed * 0x9e3779b9u);
    h = sixel_dither_perturb_mix32(h + (uint32_t)absolute_y);
    h = sixel_dither_perturb_mix32(h + (uint32_t)(pair + 1) * 0x9e3779b9u);
    return h;
}

void
sixel_dither_perturb_init(sixel_dither_perturb_t *context,
                          float amount, int seed)
{
    context->enabled = amount > 0.0f;
    context->seed = (uint32_t)seed;
    context->float_amp[0] = amount * 5.0f;
    context->float_amp[1] = amount;
    /* Round positive amplitudes once, then truncate signed deltas toward 0. */
    context->amp[0] = (int)(amount * 1280.0f + 0.5f);
    context->amp[1] = (int)(amount * 256.0f + 0.5f);
}

void
sixel_dither_perturb_weights(sixel_dither_perturb_t const *context,
                             int x, int absolute_y, int num[4])
{
    int pair;
    int u16;
    int delta;
    uint32_t h;

    num[0] = 7 << 8;
    num[1] = 3 << 8;
    num[2] = 5 << 8;
    num[3] = 1 << 8;
    for (pair = 0; pair < 2; ++pair) {
        h = sixel_dither_perturb_hash(x, absolute_y, context->seed, pair);
        u16 = (int)(h >> 16) - 32768;
        delta = (int)(((int64_t)u16 * context->amp[pair]) / 32768);
        num[pair] += delta;
        num[pair + 2] -= delta;
    }
}

void
sixel_dither_perturb_float(sixel_dither_perturb_t const *context,
                           int x, int absolute_y, float weights[4])
{
    int pair;
    int u16;
    float delta;
    uint32_t h;

    weights[0] = 7.0f / 16.0f;
    weights[1] = 3.0f / 16.0f;
    weights[2] = 5.0f / 16.0f;
    weights[3] = 1.0f / 16.0f;
    for (pair = 0; pair < 2; ++pair) {
        h = sixel_dither_perturb_hash(x, absolute_y, context->seed, pair);
        u16 = (int)(h >> 16) - 32768;
        delta = ((float)u16 / 32768.0f) * context->float_amp[pair]
            / 16.0f;
        weights[pair] += delta;
        weights[pair + 2] -= delta;
    }
}

static void
fs_sixel_dither_scanline_params_fixed_8bit(int serpentine,
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

static void
fs_error_diffuse_normal(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = *data + (error * numerator * 2 / denominator + 1) / 2;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

/* Shared diffusion helper kernels. */

static void fs_diffuse_fs(unsigned char *data,
                       int width,
                       int height,
                       int x,
                       int y,
                       int depth,
                       int error,
                       int direction,
                       int absolute_y,
                       sixel_dither_perturb_t const *perturb);

static SIXELSTATUS
sixel_dither_apply_fs_8bit(
    sixel_index_t *result,
    unsigned char *data,
    int width,
    int height,
    int band_origin,
    int output_start,
    int depth,
    unsigned char *palette,
    int method_for_scan,
    sixel_lookup_policy_interface_t const *lookup_policy,
    sixel_dither_t *dither,
    sixel_dither_perturb_t const *perturb)
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
    unsigned char const *accumulation_pixel;
    int accumulation_keycolor;
    int is_6delta_keep;
    int record_result;
    int diffuse_6delta_error;

    status = SIXEL_FALSE;

    if (dither == NULL || result == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (data == NULL || palette == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (lookup_policy == NULL || lookup_policy->vtbl == NULL
            || lookup_policy->vtbl->map_pixel == NULL) {
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
    accumulation_pixel = NULL;
    accumulation_keycolor = (-1);
    is_6delta_keep = 0;
    record_result = 0;
    diffuse_6delta_error =
        sixel_dither_pipeline_6delta_error_mode(dither)
        == SIXEL_6DELTA_ERROR_DIFFUSE ? 1 : 0;
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

    for (y = 0; y < height; ++y) {
        absolute_y = band_origin + y;
        fs_sixel_dither_scanline_params_fixed_8bit(
            serpentine, absolute_y, width,
            &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * width + x;
            base = (size_t)pos * (size_t)depth;
            is_transparent = 0;
            absolute_index = 0U;
            if (absolute_y >= 0) {
                absolute_index = (size_t)absolute_y * (size_t)width
                    + (size_t)x;
            }
            if (use_transparent_fence && absolute_y >= 0) {
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
            record_result = absolute_y >= output_start ? 1 : 0;
            is_6delta_keep = 0;
            if (absolute_y >= 0) {
                is_6delta_keep =
                    sixel_dither_pipeline_6delta_try_keep_rgb888(
                        dither,
                        absolute_index,
                        x,
                        absolute_y,
                        source_pixel,
                        record_result,
                        &accumulation_pixel,
                        &accumulation_keycolor);
            }
            if (is_6delta_keep != 0) {
                if (record_result != 0) {
                    result[pos] = (sixel_index_t)accumulation_keycolor;
                }
                if (diffuse_6delta_error != 0) {
                    for (n = 0; n < depth; ++n) {
                        offset = (int)source_pixel[n]
                            - (int)accumulation_pixel[n];
                        fs_diffuse_fs(data + n, width, height, x, y,
                                          depth, offset, direction,
                              absolute_y, perturb);
                    }
                }
                continue;
            }
            color_index = lookup_policy->vtbl->map_pixel(
                lookup_policy,
                source_pixel);
            /*
             * Now that the palette has had its say, let the color already on
             * screen compete with it.  Keeping wins ties because it costs no
             * output at all, and it can only be chosen when it is at least as
             * close to the source as the entry the lookup picked -- so this
             * never trades quality for bytes.
             */
            if (absolute_y >= 0 && depth >= 3) {
                is_6delta_keep =
                    sixel_dither_pipeline_6delta_try_keep_after_lookup(
                        dither,
                        absolute_index,
                        x,
                        absolute_y,
                        source_pixel,
                        palette + (size_t)color_index * (size_t)depth,
                        record_result,
                        &accumulation_pixel,
                        &accumulation_keycolor);
                if (is_6delta_keep != 0) {
                    if (record_result != 0) {
                        result[pos] = (sixel_index_t)accumulation_keycolor;
                    }
                    if (diffuse_6delta_error != 0) {
                        for (n = 0; n < depth; ++n) {
                            offset = (int)source_pixel[n]
                                - (int)accumulation_pixel[n];
                            fs_diffuse_fs(data + n, width, height, x, y,
                                          depth, offset, direction,
                                          absolute_y, perturb);
                        }
                    }
                    continue;
                }
            }
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
                fs_diffuse_fs(data + n, width, height, x, y,
                          depth, offset, direction,
                                      absolute_y, perturb);
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
fs_diffuse_fs(unsigned char *data, int width, int height,
           int x, int y, int depth, int error, int direction,
           int absolute_y, sixel_dither_perturb_t const *perturb)
{
    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/16    1/16
     */
    int pos;
    int forward;
    int num[4];
    int step;

    pos = y * width + x;
    forward = direction >= 0;

    if (perturb->enabled) {
        /* Absolute coordinates give overlapping bands identical weights.
         * Mirror only the horizontal offsets on serpentine return rows.
         * Pair sums remain exact before edge clipping and pixel rounding.
         */
        sixel_dither_perturb_weights(perturb, x, absolute_y, num);
        step = forward ? 1 : -1;
        if (x + step >= 0 && x + step < width) {
            fs_error_diffuse_normal(data, pos + step, depth, error,
                                    num[0], 16 << 8);
        }
        if (y < height - 1) {
            if (x - step >= 0 && x - step < width) {
                fs_error_diffuse_normal(data, pos + width - step,
                                        depth, error, num[1], 16 << 8);
            }
            fs_error_diffuse_normal(data, pos + width, depth, error,
                                    num[2], 16 << 8);
            if (x + step >= 0 && x + step < width) {
                fs_error_diffuse_normal(data, pos + width + step,
                                        depth, error, num[3], 16 << 8);
            }
        }
        return;
    }
    /* Preserve the original constants and arithmetic when disabled. */

    if (forward) {
        if (x < width - 1) {
            fs_error_diffuse_normal(data, pos + 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x > 0) {
                fs_error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 3, 16);
            }
            fs_error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x < width - 1) {
                fs_error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 1, 16);
            }
        }
    } else {
        if (x > 0) {
            fs_error_diffuse_normal(data, pos - 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                fs_error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 3, 16);
            }
            fs_error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x > 0) {
                fs_error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 1, 16);
            }
        }
    }
}

static void
fs_error_diffuse_float(float *data,
                    int pos,
                    int depth,
                    float error,
                    float numerator,
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
fs_sixel_dither_scanline_params_fixed_float32(int serpentine,
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

static void
fs_diffuse_fs_float(float *data,
                 int width,
                 int height,
                 int x,
                 int y,
                 int depth,
                 float error,
                 int direction,
                 int pixelformat,
                 int channel_index,
                 int absolute_y,
                 sixel_dither_perturb_t const *perturb)
{
    int pos;
    int forward;
    float weights[4];
    int step;

    pos = y * width + x;
    forward = direction >= 0;

    if (perturb->enabled) {
        sixel_dither_perturb_float(perturb, x, absolute_y, weights);
        step = forward ? 1 : -1;
        if (x + step >= 0 && x + step < width) {
            fs_error_diffuse_float(data, pos + step, depth, error,
                                  weights[0], 1, pixelformat, channel_index);
        }
        if (y < height - 1) {
            if (x - step >= 0 && x - step < width) {
                fs_error_diffuse_float(data, pos + width - step,
                                      depth, error, weights[1], 1,
                                      pixelformat, channel_index);
            }
            fs_error_diffuse_float(data, pos + width, depth, error,
                                  weights[2], 1, pixelformat, channel_index);
            if (x + step >= 0 && x + step < width) {
                fs_error_diffuse_float(data, pos + width + step,
                                      depth, error, weights[3], 1,
                                      pixelformat, channel_index);
            }
        }
        return;
    }
    /* Preserve the original constants and arithmetic when disabled. */

    if (forward) {
        if (x < width - 1) {
            fs_error_diffuse_float(data,
                                pos + 1,
                                depth,
                                error,
                                7,
                                16,
                                pixelformat,
                                channel_index);
        }
        if (y < height - 1) {
            if (x > 0) {
                fs_error_diffuse_float(data,
                                    pos + width - 1,
                                    depth,
                                    error,
                                    3,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
            fs_error_diffuse_float(data,
                                pos + width,
                                depth,
                                error,
                                5,
                                16,
                                pixelformat,
                                channel_index);
            if (x < width - 1) {
                fs_error_diffuse_float(data,
                                    pos + width + 1,
                                    depth,
                                    error,
                                    1,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
        }
    } else {
        if (x > 0) {
            fs_error_diffuse_float(data,
                                pos - 1,
                                depth,
                                error,
                                7,
                                16,
                                pixelformat,
                                channel_index);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                fs_error_diffuse_float(data,
                                    pos + width + 1,
                                    depth,
                                    error,
                                    3,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
            fs_error_diffuse_float(data,
                                pos + width,
                                depth,
                                error,
                                5,
                                16,
                                pixelformat,
                                channel_index);
            if (x > 0) {
                fs_error_diffuse_float(data,
                                    pos + width - 1,
                                    depth,
                                    error,
                                    1,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
        }
    }
}

static SIXELSTATUS
sixel_dither_apply_fs_float32(
    sixel_dither_t *dither,
    sixel_dither_policy_fs_context_t *context
    )
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
            || context->lookup_policy->vtbl->map_pixel == NULL) {
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

    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        fs_sixel_dither_scanline_params_fixed_float32(
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

            lookup_pixel = (unsigned char const *)(void const *)
                working_float;
            color_index = context->lookup_policy->vtbl->map_pixel(
                context->lookup_policy,
                lookup_pixel);

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
                fs_diffuse_fs_float(data + (size_t)n,
                          context->width,
                          context->height,
                          x,
                          y,
                          context->depth,
                          error,
                          direction,
                          context->pixelformat,
                          n,
                          absolute_y,
                          &context->perturb);
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

typedef struct sixel_dither_policy_fs_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_fs_object_t;

static sixel_dither_policy_fs_object_t *
sixel_dither_policy_fs_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_fs_object_t *)(void *)policy;
}

static sixel_dither_policy_fs_object_t const *
sixel_dither_policy_fs_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_fs_object_t const *)(void const *)policy;
}

static void
sixel_dither_policy_fs_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_fs_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_fs_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_fs_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_fs_object_t *object;
    unsigned int previous;
    sixel_allocator_t *allocator;

    object = NULL;
    previous = 0U;
    allocator = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_fs_from_base(policy);
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
sixel_dither_policy_fs_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_fs_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_fs_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_fs_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective)
{
    sixel_dither_policy_fs_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_fs_from_base_const(policy);
    *effective = *request;
    effective->method_for_scan = object->method_for_scan;
    effective->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_fs_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_fs_context_t *context)
{
    sixel_dither_t *dither;
    sixel_palette_t *palette_object;
    sixel_palette_float32_entries_view_t float32_view;
    int float_components;

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

    if (request->lookup_policy->vtbl->map_pixel == NULL) {
        sixel_helper_set_additional_message(
            "sixel_dither_map_pixels: lookup policy is not prepared.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat)) {
        context->pixels_float = (float *)(void *)request->data;
    }

    dither = request->dither;
    if (dither != NULL) {
        sixel_dither_perturb_init(&context->perturb,
                                  dither->diffusion_perturb,
                                  dither->diffusion_perturb_seed);
    }
    if (dither != NULL && dither->palette != NULL) {
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
sixel_dither_policy_fs_apply_8bit(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_fs_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_fs_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_fs_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_apply_fs_8bit(
        context.result,
        context.pixels,
        context.width,
        context.height,
        context.band_origin,
        context.output_start,
        context.depth,
        context.palette,
        context.method_for_scan,
        context.lookup_policy,
        effective.dither,
        &context.perturb);
}

static SIXELSTATUS
sixel_dither_policy_fs_apply_float32(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_fs_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_fs_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_fs_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_apply_fs_float32(
        effective.dither,
        &context);
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_fs_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_fs_vtbl = {
    sixel_dither_policy_fs_ref,
    sixel_dither_policy_fs_unref,
    sixel_dither_policy_fs_prepare,
    sixel_dither_policy_fs_apply_8bit,
    sixel_dither_policy_fs_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_fs_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    sixel_dither_policy_fs_object_t *object;

    object = NULL;
    if (allocator == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_fs_object_t *)
        sixel_allocator_malloc(allocator, sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_fs_vtbl;
    object->ref = 1U;
    object->allocator = allocator;
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
    g_sixel_dither_policy_fs_8bit_vtbl = {
    sixel_dither_policy_fs_ref,
    sixel_dither_policy_fs_unref,
    sixel_dither_policy_fs_prepare,
    sixel_dither_policy_fs_apply_8bit,
    sixel_dither_policy_fs_supports_parallel_bands
};

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_fs_float32_vtbl = {
    sixel_dither_policy_fs_ref,
    sixel_dither_policy_fs_unref,
    sixel_dither_policy_fs_prepare,
    sixel_dither_policy_fs_apply_float32,
    sixel_dither_policy_fs_supports_parallel_bands
};

SIXELSTATUS
sixel_dither_policy_fs_8bit_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_fs_new(allocator, policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        ((sixel_dither_policy_interface_t *)(*policy))->vtbl = &g_sixel_dither_policy_fs_8bit_vtbl;
    }

    return status;
}

SIXELSTATUS
sixel_dither_policy_fs_float32_new(
    sixel_allocator_t *allocator,
    void **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_fs_new(allocator, policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        ((sixel_dither_policy_interface_t *)(*policy))->vtbl = &g_sixel_dither_policy_fs_float32_vtbl;
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
