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

#include "dither-policy-lso2.h"
#include "dither.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/*
 * Private dither context for this policy implementation.
 * Keep only members used by this translation unit.
 */
typedef struct sixel_dither_policy_lso2_context {
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
    sixel_dither_lookup_map_fn lookup_map;
    int pixelformat;
    int float_depth;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
} sixel_dither_policy_lso2_context_t;

static void
sixel_dither_scanline_params_varcoeff_8bit(int serpentine,
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

static const int (*
lso2_table_varcoeff_8bit(void))[7]
{
#include "lso2.h"
    return var_coefs;
}

#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

static int32_t
sixel_varcoeff_safe_denom_8bit(int value)
{
    if (value == 0) {
        /*
         * The first row of `lso2.h` ships a zero denominator.  Older builds
         * happened to avoid the division but the refactor now makes the edge
         * visible, so guard it explicitly.
         */
        return 1;
    }
    return value;
}

static int32_t
diffuse_varerr_term(int32_t error, int weight, int denom)
{
    int64_t delta;

    delta = (int64_t)error * (int64_t)weight;
    if (delta >= 0) {
        delta = (delta + denom / 2) / denom;
    } else {
        delta = (delta - denom / 2) / denom;
    }

    return (int32_t)delta;
}

static void
diffuse_varerr_apply_direct(unsigned char *target,
                            int depth,
                            size_t offset,
                            int32_t delta)
{
    int64_t value;
    int result;

    value = (int64_t)target[offset * depth] << VARERR_SCALE_SHIFT;
    value += delta;
    if (value < 0) {
        value = 0;
    } else {
        int64_t max_value;

        max_value = VARERR_MAX_VALUE;
        if (value > max_value) {
            value = max_value;
        }
    }

    result = (int)((value + VARERR_ROUND) >> VARERR_SCALE_SHIFT);
    if (result < 0) {
        result = 0;
    }
    if (result > 255) {
        result = 255;
    }
    target[offset * depth] = (unsigned char)result;
}

static void
diffuse_lso2(unsigned char *data,
             int width,
             int height,
             int x,
             int y,
             int depth,
             int32_t error,
             int index,
             int direction)
{
    const int (*table)[7];
    const int *entry;
    int denom;
    int32_t term_r;
    int32_t term_r2;
    int32_t term_dl;
    int32_t term_d;
    int32_t term_dr;
    int32_t term_d2;
    size_t offset;

    if (error == 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table_varcoeff_8bit();
    entry = table[index];
    denom = sixel_varcoeff_safe_denom_8bit(entry[6]);

    term_r = diffuse_varerr_term(error, entry[0], denom);
    term_r2 = diffuse_varerr_term(error, entry[1], denom);
    term_dl = diffuse_varerr_term(error, entry[2], denom);
    term_d = diffuse_varerr_term(error, entry[3], denom);
    term_dr = diffuse_varerr_term(error, entry[4], denom);
    term_d2 = diffuse_varerr_term(error, entry[5], denom);

    if (direction >= 0) {
        if (x + 1 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_r);
        }
        if (x + 2 < width) {
            offset = (size_t)y * (size_t)width + (size_t)(x + 2);
            diffuse_varerr_apply_direct(data, depth, offset, term_r2);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dl);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dr);
        }
        if (y + 2 < height) {
            offset = (size_t)(y + 2) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d2);
        }
    } else {
        if (x - 1 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_r);
        }
        if (x - 2 >= 0) {
            offset = (size_t)y * (size_t)width + (size_t)(x - 2);
            diffuse_varerr_apply_direct(data, depth, offset, term_r2);
        }
        if (y + 1 < height && x + 1 < width) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x + 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dl);
        }
        if (y + 1 < height) {
            offset = (size_t)(y + 1) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d);
        }
        if (y + 1 < height && x - 1 >= 0) {
            offset = (size_t)(y + 1) * (size_t)width;
            offset += (size_t)(x - 1);
            diffuse_varerr_apply_direct(data, depth, offset, term_dr);
        }
        if (y + 2 < height) {
            offset = (size_t)(y + 2) * (size_t)width + (size_t)x;
            diffuse_varerr_apply_direct(data, depth, offset, term_d2);
        }
    }
}

static SIXELSTATUS
sixel_dither_apply_lso2_8bit(sixel_dither_t *dither,
                             sixel_dither_policy_lso2_context_t *context
                             )
{
    SIXELSTATUS status;
    unsigned char *data;
    unsigned char *palette;
    unsigned char *source_pixel;
    unsigned char palette_value;
    int32_t sample_scaled[SIXEL_MAX_CHANNELS];
    int32_t target_scaled;
    int32_t error_scaled;
    int serpentine;
    int method_for_scan;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    int depth;
    int n;
    int color_index;
    int output_index;
    int diff;
    int table_index;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_BAD_ARGUMENT;
    data = context->pixels;
    palette = context->palette;
    depth = context->depth;
    method_for_scan = context->method_for_scan;
    if (data == NULL || palette == NULL || context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth <= 0 || depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
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

    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        sixel_dither_scanline_params_varcoeff_8bit(serpentine,
                                     absolute_y,
                                     context->width,
                                     &start,
                                     &end,
                                     &step,
                                     &direction);
        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            base = (size_t)pos * (size_t)depth;
            is_transparent = 0;
            if (use_transparent_fence && absolute_y >= 0) {
                absolute_index = (size_t)absolute_y * (size_t)context->width
                    + (size_t)x;
                if (absolute_index < transparent_mask_size
                        && transparent_mask[absolute_index] != 0U) {
                    is_transparent = 1;
                }
            }
            if (is_transparent) {
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)transparent_keycolor;
                }
                continue;
            }
            for (n = 0; n < depth; ++n) {
                sample_scaled[n]
                    = (int32_t)data[base + (size_t)n]
                    << VARERR_SCALE_SHIFT;
            }
            source_pixel = data + base;

            color_index = context->lookup_map(context->lookup_policy,
                                              source_pixel);

                output_index = color_index;
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)output_index;
                }
            
            for (n = 0; n < depth; ++n) {
                palette_value = palette[color_index * depth + n];
                diff = (int)source_pixel[n] - (int)palette_value;
                if (diff < 0) {
                    diff = -diff;
                }
                if (diff > 255) {
                    diff = 255;
                }
                table_index = diff;
                target_scaled = (int32_t)palette_value
                              << VARERR_SCALE_SHIFT;
                error_scaled = sample_scaled[n] - target_scaled;
                diffuse_lso2(data + n,
                             context->width,
                             context->height,
                             x,
                             y,
                             depth,
                             error_scaled,
                             table_index,
                             direction);
            }
        }

        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    status = SIXEL_OK;
    return status;
}

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#include "dither-common-pipeline.h"
#include "pixelformat.h"

static void
sixel_dither_scanline_params_varcoeff_float32(int serpentine,
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

static const int (*
lso2_table_varcoeff_float32(void))[7]
{
#include "lso2.h"
    return var_coefs;
}

static int
sixel_varcoeff_safe_denom_float32(int value)
{
    if (value == 0) {
        return 1;
    }
    return value;
}

static void
diffuse_varerr_apply_direct_float(float *target,
                                  int depth,
                                  size_t offset,
                                  float delta,
                                  int pixelformat,
                                  int channel)
{
    size_t index;

    index = offset * (size_t)depth;
    target[index] += delta;
    target[index] = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                          channel,
                                                          target[index]);
}

static void
diffuse_lso2_float(float *data,
                    int width,
                    int height,
                    int x,
                    int y,
                    int depth,
                    float error,
                    int index,
                    int direction,
                    int pixelformat,
                    int channel)
{
    const int (*table)[7];
    const int *entry;
    int denom;
    float factor;
    float term_r;
    float term_r2;
    float term_dl;
    float term_d;
    float term_dr;
    float term_d2;
    int has_left;
    int has_two_left;
    int has_right;
    int has_two_right;
    int has_next;
    int has_far;
    size_t row_curr;
    size_t row_next;
    size_t row_far;
    size_t base_curr;
    size_t base_next;
    size_t base_far;
    size_t offset;

    if (error == 0.0f) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table_varcoeff_float32();
    entry = table[index];
    denom = sixel_varcoeff_safe_denom_float32(entry[6]);
    /*
     * Reduce the number of costly divisions by sharing a single factor for
     * all weights in the diffusion kernel.  Multiplications are cheaper
     * and keep the overall cost of distributing the error lower when
     * processing wide images.
     */
    factor = error / (float)denom;
    term_r = factor * (float)entry[0];
    term_r2 = factor * (float)entry[1];
    term_dl = factor * (float)entry[2];
    term_d = factor * (float)entry[3];
    term_dr = factor * (float)entry[4];
    term_d2 = factor * (float)entry[5];

    row_curr = (size_t)y * (size_t)width;
    row_next = (size_t)(y + 1) * (size_t)width;
    row_far = (size_t)(y + 2) * (size_t)width;
    base_curr = row_curr + (size_t)x;
    base_next = row_next + (size_t)x;
    base_far = row_far + (size_t)x;
    /*
     * Precompute neighborhood availability and row bases to avoid repeating
     * multiplications or bounds checks for every kernel tap.  Keeping the
     * inner branch conditions simple helps the compiler schedule the
     * diffusion math more effectively when processing wide images.
     */
    has_left = (x - 1) >= 0;
    has_two_left = (x - 2) >= 0;
    has_right = (x + 1) < width;
    has_two_right = (x + 2) < width;
    has_next = (y + 1) < height;
    has_far = (y + 2) < height;

    if (direction >= 0) {
        if (has_right) {
            offset = base_curr + 1;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_r,
                                              pixelformat,
                                              channel);
        }
        if (has_two_right) {
            offset = base_curr + 2;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_r2,
                                              pixelformat,
                                              channel);
        }
        if (has_next && has_left) {
            offset = base_next - 1;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_dl,
                                              pixelformat,
                                              channel);
        }
        if (has_next) {
            offset = base_next;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_d,
                                              pixelformat,
                                              channel);
        }
        if (has_next && has_right) {
            offset = base_next + 1;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_dr,
                                              pixelformat,
                                              channel);
        }
        if (has_far) {
            offset = base_far;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_d2,
                                              pixelformat,
                                              channel);
        }
    } else {
        if (has_left) {
            offset = base_curr - 1;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_r,
                                              pixelformat,
                                              channel);
        }
        if (has_two_left) {
            offset = base_curr - 2;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_r2,
                                              pixelformat,
                                              channel);
        }
        if (has_next && has_right) {
            offset = base_next + 1;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_dl,
                                              pixelformat,
                                              channel);
        }
        if (has_next) {
            offset = base_next;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_d,
                                              pixelformat,
                                              channel);
        }
        if (has_next && has_left) {
            offset = base_next - 1;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_dr,
                                              pixelformat,
                                              channel);
        }
        if (has_far) {
            offset = base_far;
            diffuse_varerr_apply_direct_float(data,
                                              depth,
                                              offset,
                                              term_d2,
                                              pixelformat,
                                              channel);
        }
    }
}

static SIXELSTATUS
sixel_dither_apply_lso2_float32(sixel_dither_t *dither,
                                sixel_dither_policy_lso2_context_t *context
                                )
{
    SIXELSTATUS status;
    float *data;
    unsigned char *palette;
    float *source_pixel;
    float quantized_float;
    float palette_value_float;
    unsigned char palette_value;
    float error;
    int serpentine;
    int method_for_scan;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    int depth;
    int channel_count;
    int n;
    int color_index;
    int output_index;
    int diff;
    int table_index;
    float *palette_float;
    int float_depth;
    float lookup_pixel_float[SIXEL_MAX_CHANNELS] = { 0.0f };
    unsigned char const *lookup_pixel;
    int have_palette_float;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;

    status = SIXEL_BAD_ARGUMENT;
    data = NULL;
    palette = NULL;
    source_pixel = NULL;
    quantized_float = 0.0f;
    palette_value_float = 0.0f;
    palette_value = 0U;
    error = 0.0f;
    serpentine = 0;
    method_for_scan = SIXEL_SCAN_RASTER;
    y = 0;
    absolute_y = 0;
    start = 0;
    end = 0;
    step = 0;
    direction = 0;
    x = 0;
    pos = 0;
    base = 0U;
    depth = 0;
    channel_count = 0;
    n = 0;
    color_index = 0;
    output_index = 0;
    diff = 0;
    table_index = 0;
    palette_float = NULL;
    float_depth = 0;
    lookup_pixel = NULL;
    have_palette_float = 0;
    transparent_mask = NULL;
    transparent_mask_size = 0U;
    transparent_keycolor = -1;
    use_transparent_fence = 0;
    is_transparent = 0;
    absolute_index = 0U;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    data = context->pixels_float;
    palette = context->palette;
    depth = context->depth;
    channel_count = depth;
    method_for_scan = context->method_for_scan;
    palette_float = context->palette_float;
    float_depth = context->float_depth;

    if (data == NULL || palette == NULL || context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup_policy == NULL || context->lookup_map == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (channel_count <= 0 || channel_count > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }

    transparent_mask = context->transparent_mask;
    transparent_mask_size = context->transparent_mask_size;
    transparent_keycolor = context->transparent_keycolor;
    if (transparent_mask != NULL
            && transparent_keycolor >= 0
            && transparent_keycolor < SIXEL_PALETTE_MAX) {
        use_transparent_fence = 1;
    }

    if (palette_float != NULL && float_depth >= depth) {
        have_palette_float = 1;
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);
    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        sixel_dither_scanline_params_varcoeff_float32(serpentine,
                                     absolute_y,
                                     context->width,
                                     &start,
                                     &end,
                                     &step,
                                     &direction);
        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            base = (size_t)pos * (size_t)depth;
            is_transparent = 0;
            if (use_transparent_fence && absolute_y >= 0) {
                absolute_index = (size_t)absolute_y * (size_t)context->width
                    + (size_t)x;
                if (absolute_index < transparent_mask_size
                        && transparent_mask[absolute_index] != 0U) {
                    is_transparent = 1;
                }
            }
            if (is_transparent) {
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)transparent_keycolor;
                }
                continue;
            }

            source_pixel = data + base;
            for (n = 0; n < channel_count; ++n) {
                if (n >= SIXEL_MAX_CHANNELS) {
                    break;
                }
                quantized_float = source_pixel[n];
                quantized_float = sixel_pixelformat_float_channel_clamp(
                    context->pixelformat,
                    n,
                    quantized_float);
                if (source_pixel == data + base) {
                    source_pixel[n] = quantized_float;
                }
                lookup_pixel_float[n] = quantized_float;
            }

            lookup_pixel = (unsigned char const *)(void const *)
                lookup_pixel_float;
            color_index = context->lookup_map(context->lookup_policy,
                                              lookup_pixel);

            output_index = color_index;
            if (absolute_y >= context->output_start) {
                context->result[pos] = (sixel_index_t)output_index;
            }

            for (n = 0; n < channel_count; ++n) {
                if (n >= SIXEL_MAX_CHANNELS) {
                    break;
                }
                if (n > 0 && (
                        context->pixelformat
                            == SIXEL_PIXELFORMAT_OKLABFLOAT32
                        || context->pixelformat
                            == SIXEL_PIXELFORMAT_CIELABFLOAT32
                        || context->pixelformat
                            == SIXEL_PIXELFORMAT_DIN99DFLOAT32
                   )) {
                    break;  /* L or Y only */
                }

                if (have_palette_float) {
                    palette_value_float =
                        palette_float[color_index * float_depth + n];
                } else {
                    palette_value = palette[color_index * depth + n];
                    palette_value_float = sixel_pixelformat_byte_to_float(
                        context->pixelformat,
                        n,
                        palette_value);
                }
                error = source_pixel[n] - palette_value_float;
                if (error < 0.0f) {
                    diff = (int)(-error * 255.0f + 0.5f);
                } else {
                    diff = (int)(error * 255.0f + 0.5f);
                }
                if (diff > 255) {
                    diff = 255;
                }
                table_index = diff;
                diffuse_lso2_float(data + n,
                                   context->width,
                                   context->height,
                                   x,
                                   y,
                                   depth,
                                   error,
                                   table_index,
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */

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

typedef struct sixel_dither_policy_lso2_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_lso2_object_t;

static sixel_dither_policy_lso2_object_t *
sixel_dither_policy_lso2_from_base(sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_lso2_object_t *)(void *)policy;
}

static sixel_dither_policy_lso2_object_t const *
sixel_dither_policy_lso2_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_lso2_object_t const *)(void const *)policy;
}

static void
sixel_dither_policy_lso2_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_lso2_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_lso2_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_lso2_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_lso2_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_lso2_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_dither_policy_lso2_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_lso2_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_lso2_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_lso2_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective)
{
    sixel_dither_policy_lso2_object_t const *object;

    object = NULL;
    if (policy == NULL || request == NULL || effective == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_lso2_from_base_const(policy);
    *effective = *request;
    effective->method_for_scan = object->method_for_scan;
    effective->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_lso2_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_lso2_context_t *context)
{
    sixel_dither_lookup_map_fn lookup_map;
    sixel_dither_t *dither;

    lookup_map = NULL;
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

    lookup_map = request->lookup_policy->vtbl->map_pixel;
    context->lookup_map = lookup_map;

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
sixel_dither_policy_lso2_apply_8bit(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_lso2_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_lso2_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_lso2_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_apply_lso2_8bit(
        effective.dither,
        &context);
}

static SIXELSTATUS
sixel_dither_policy_lso2_apply_float32(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_policy_lso2_context_t context;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));

    status = sixel_dither_policy_lso2_make_effective_request(policy,
                                                             request,
                                                             &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_lso2_build_context(&effective,
                                                    &context);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_dither_apply_lso2_float32(
        effective.dither,
        &context);
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_lso2_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_lso2_vtbl = {
    sixel_dither_policy_lso2_ref,
    sixel_dither_policy_lso2_unref,
    sixel_dither_policy_lso2_prepare,
    sixel_dither_policy_lso2_apply_8bit,
    sixel_dither_policy_lso2_supports_parallel_bands
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_create_lso2(
    sixel_dither_policy_interface_t **policy)
{
    sixel_dither_policy_lso2_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *policy = NULL;

    object = (sixel_dither_policy_lso2_object_t *)malloc(sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    object->base.vtbl = &g_sixel_dither_policy_lso2_vtbl;
    object->ref = 1U;
    object->method_for_scan = SIXEL_SCAN_AUTO;
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_lso2_8bit_vtbl = {
    sixel_dither_policy_lso2_ref,
    sixel_dither_policy_lso2_unref,
    sixel_dither_policy_lso2_prepare,
    sixel_dither_policy_lso2_apply_8bit,
    sixel_dither_policy_lso2_supports_parallel_bands
};

static sixel_dither_policy_vtbl_t const
    g_sixel_dither_policy_lso2_float32_vtbl = {
    sixel_dither_policy_lso2_ref,
    sixel_dither_policy_lso2_unref,
    sixel_dither_policy_lso2_prepare,
    sixel_dither_policy_lso2_apply_float32,
    sixel_dither_policy_lso2_supports_parallel_bands
};

SIXELSTATUS
sixel_dither_policy_create_lso2_8bit(
    sixel_dither_policy_interface_t **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_create_lso2(policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        (*policy)->vtbl = &g_sixel_dither_policy_lso2_8bit_vtbl;
    }

    return status;
}

SIXELSTATUS
sixel_dither_policy_create_lso2_float32(
    sixel_dither_policy_interface_t **policy)
{
    SIXELSTATUS status;

    status = sixel_dither_policy_create_lso2(policy);
    if (SIXEL_SUCCEEDED(status) && policy != NULL && *policy != NULL) {
        (*policy)->vtbl = &g_sixel_dither_policy_lso2_float32_vtbl;
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
