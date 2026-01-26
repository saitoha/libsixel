/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

/*
 * Adaptive diffusion backend operating on RGBFLOAT32 buffers.  The worker
 * mirrors the 8bit implementation but keeps intermediate values in float so
 * rounding happens only at palette lookups.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#include "dither-varcoeff-float32.h"
#include "dither-common-pipeline.h"
#include "lookup-common.h"
#include "pixelformat.h"

static float
sixel_oklab_lightness_toe_float(float lightness)
{
    const float k1 = 0.206f;
    const float k2 = 0.03f;
    const float k3 = (1.0f + k1) / (1.0f + k2);

    return 0.5f * (((k3 * lightness) / (lightness + k1))
                   + (lightness / ((k2 * lightness) + 1.0f)));
}

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

typedef void (*diffuse_varerr_mode_float)(float *data,
                                          int width,
                                          int height,
                                          int x,
                                          int y,
                                          int depth,
                                          float error,
                                          int index,
                                          int direction,
                                          int pixelformat,
                                          int channel);

typedef void (*diffuse_varerr_carry_mode_float)(float *carry_curr,
                                                 float *carry_next,
                                                 float *carry_far,
                                                 int width,
                                                 int height,
                                                 int depth,
                                                 int x,
                                                 int y,
                                                 float error,
                                                 int index,
                                                 int direction,
                                                 int channel);

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

static void
diffuse_lso2_carry_float(float *carry_curr,
                          float *carry_next,
                          float *carry_far,
                          int width,
                          int height,
                          int depth,
                          int x,
                          int y,
                          float error,
                          int index,
                          int direction,
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
    size_t base_curr;
    size_t base_next;
    size_t base_far;
    size_t stride;
    int has_left;
    int has_two_left;
    int has_right;
    int has_two_right;
    int has_next;
    int has_far;

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
     * Share a single division across the error terms so the carry path is
     * competitive with the direct-write variant even on large frames.
     */
    factor = error / (float)denom;
    term_r = factor * (float)entry[0];
    term_r2 = factor * (float)entry[1];
    term_dl = factor * (float)entry[2];
    term_d = factor * (float)entry[3];
    term_dr = factor * (float)entry[4];
    term_d2 = error - term_r - term_r2 - term_dl - term_d - term_dr;

    stride = (size_t)depth;
    base_curr = ((size_t)x * stride) + (size_t)channel;
    base_next = base_curr;
    base_far = base_curr;
    /*
     * Compute stride- and row-aligned offsets once so the carry buffers do
     * not pay for repeated multiplications.  Keeping the neighbor flags in
     * registers trims branch conditions to quick boolean checks during the
     * innermost diffusion loop.
     */
    
    has_left = (x - 1) >= 0;
    has_two_left = (x - 2) >= 0;
    has_right = (x + 1) < width;
    has_two_right = (x + 2) < width;
    has_next = (y + 1) < height;
    has_far = (y + 2) < height;

    if (direction >= 0) {
        if (has_right) {
            carry_curr[base_curr + stride] += term_r;
        }
        if (has_two_right) {
            carry_curr[base_curr + (stride * 2)] += term_r2;
        }
        if (has_next && has_left) {
            carry_next[base_next - stride] += term_dl;
        }
        if (has_next) {
            carry_next[base_next] += term_d;
        }
        if (has_next && has_right) {
            carry_next[base_next + stride] += term_dr;
        }
        if (has_far) {
            carry_far[base_far] += term_d2;
        }
    } else {
        if (has_left) {
            carry_curr[base_curr - stride] += term_r;
        }
        if (has_two_left) {
            carry_curr[base_curr - (stride * 2)] += term_r2;
        }
        if (has_next && has_right) {
            carry_next[base_next + stride] += term_dl;
        }
        if (has_next) {
            carry_next[base_next] += term_d;
        }
        if (has_next && has_left) {
            carry_next[base_next - stride] += term_dr;
        }
        if (has_far) {
            carry_far[base_far] += term_d2;
        }
    }
}

#define max_channels 4

SIXELSTATUS
sixel_dither_apply_varcoeff_float32(sixel_dither_t *dither,
                                    sixel_dither_context_t *context)
{
    SIXELSTATUS status;
    float *data;
    unsigned char *palette;
    unsigned char *new_palette;
    float *source_pixel;
    float corrected[max_channels];
    float quantized_float;
    unsigned char quantized[max_channels];
    float *carry_curr = NULL;
    float *carry_next = NULL;
    float *carry_far = NULL;
    float palette_value_float;
    unsigned char palette_value;
    float error;
    int serpentine;
    int use_carry;
    size_t carry_len;
    int method_for_diffuse;
    int method_for_carry;
    int method_for_scan;
    sixel_lut_t *fast_lut;
    int use_fast_lut;
    diffuse_varerr_mode_float varerr_diffuse;
    diffuse_varerr_carry_mode_float varerr_diffuse_carry;
    int optimize_palette;
    int y;
    int absolute_y;
    int start;
    int end;
    int step;
    int direction;
    int x;
    int pos;
    size_t base;
    size_t carry_base;
    int depth;
    int reqcolor;
    int n;
    int palette_index;
    int color_index;
    int output_index;
    int diff;
    int table_index;
    unsigned short *indextable;
    unsigned short *migration_map;
    int *ncolors;
    int (*lookup)(const unsigned char *,
                  int,
                  const unsigned char *,
                  int,
                  unsigned short *,
                  int);
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int float_index;
    float *palette_float_buffer;
    float lookup_pixel_float[max_channels];
    unsigned char const *lookup_pixel;
    int lookup_wants_float;
    int use_palette_float_lookup;
    int need_float_pixel;
    int have_palette_float;
    int have_new_palette_float;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_BAD_ARGUMENT;
    data = context->pixels_float;
    palette = context->palette;
    new_palette = context->new_palette;
    indextable = context->indextable;
    migration_map = context->migration_map;
    ncolors = context->ncolors;
    depth = context->depth;
    reqcolor = context->reqcolor;
    lookup = context->lookup;
    fast_lut = context->lut;
    use_fast_lut = (fast_lut != NULL);
    optimize_palette = context->optimize_palette;
    method_for_diffuse = context->method_for_diffuse;
    method_for_carry = context->method_for_carry;
    method_for_scan = context->method_for_scan;
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    if (data == NULL || palette == NULL || context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (lookup == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    palette_float_buffer = NULL;
    /*
     * Track float palette availability separately for the original palette
     * and the palette-optimization buffer so later loops can skip
     * byte-to-float round-trips when computing the quantization error.
     */
    if (palette_float != NULL && float_depth >= depth) {
        have_palette_float = 1;
    } else {
        have_palette_float = 0;
    }
    if (context->use_l_r_distance != 0 && have_palette_float == 0) {
        palette_float_buffer = (float *)malloc((size_t)reqcolor
                                               * (size_t)depth
                                               * sizeof(float));
        if (palette_float_buffer == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (palette_index = 0; palette_index < reqcolor;
                ++palette_index) {
            for (n = 0; n < depth; ++n) {
                palette_float_buffer[(size_t)palette_index
                                      * (size_t)depth
                                      + (size_t)n]
                    = sixel_pixelformat_byte_to_float(
                          context->pixelformat,
                          n,
                          palette[(size_t)palette_index
                                  * (size_t)depth
                                  + (size_t)n]);
            }
        }
        palette_float = palette_float_buffer;
        float_depth = depth;
        have_palette_float = 1;
    }
    if (new_palette_float != NULL && float_depth >= depth) {
        have_new_palette_float = 1;
    } else {
        have_new_palette_float = 0;
    }

    if (optimize_palette) {
        if (new_palette == NULL || migration_map == NULL || ncolors == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
    } else if (ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (depth <= 0 || depth > max_channels) {
        return SIXEL_BAD_ARGUMENT;
    }

    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_LSO2:
    default:
        varerr_diffuse = diffuse_lso2_float;
        varerr_diffuse_carry = diffuse_lso2_carry_float;
        break;
    }

    use_carry = (method_for_carry == SIXEL_CARRY_ENABLE);
    carry_curr = NULL;
    carry_next = NULL;
    carry_far = NULL;
    carry_len = 0;

    if (use_carry) {
        carry_len = (size_t)context->width * (size_t)depth;
        carry_curr = (float *)calloc(carry_len, sizeof(float));
        carry_next = (float *)calloc(carry_len, sizeof(float));
        carry_far = (float *)calloc(carry_len, sizeof(float));
        if (carry_curr == NULL || carry_next == NULL || carry_far == NULL) {
            goto end;
        }
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);
    lookup_wants_float = (context->lookup_source_is_float != 0);
    use_palette_float_lookup = 0;
    if (context->use_l_r_distance != 0 && have_palette_float != 0) {
        use_palette_float_lookup = 1;
    } else if (context->prefer_palette_float_lookup != 0
            && palette_float != NULL
            && float_depth >= depth) {
        use_palette_float_lookup = 1;
    }
    need_float_pixel = lookup_wants_float || use_palette_float_lookup;
    if (optimize_palette) {
        *ncolors = 0;
        memset(new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)depth);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float, 0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth * sizeof(float));
        }
        memset(migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    }

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
            carry_base = (size_t)x * (size_t)depth;
            if (use_carry) {
                for (n = 0; n < depth; ++n) {
                    float accum;

                    accum = data[base + (size_t)n]
                           + carry_curr[carry_base + (size_t)n];
                    carry_curr[carry_base + (size_t)n] = 0.0f;
                    accum = sixel_pixelformat_float_channel_clamp(
                        context->pixelformat,
                        n,
                        accum);
                    corrected[n] = accum;
                }
                source_pixel = corrected;
            } else {
                source_pixel = data + base;
            }

            for (n = 0; n < depth; ++n) {
                quantized_float = source_pixel[n];
                quantized_float = sixel_pixelformat_float_channel_clamp(
                    context->pixelformat,
                    n,
                    quantized_float);
                if (source_pixel == data + base) {
                    source_pixel[n] = quantized_float;
                }
                if (need_float_pixel) {
                    lookup_pixel_float[n] = quantized_float;
                }
                if (!lookup_wants_float && !use_palette_float_lookup) {
                    quantized[n] = sixel_pixelformat_float_channel_to_byte(
                        context->pixelformat,
                        n,
                        quantized_float);
                }
            }
            if (context->use_l_r_distance != 0 && use_palette_float_lookup) {
                lookup_pixel_float[0] = sixel_oklab_lightness_toe_float(
                    lookup_pixel_float[0]);
            }
            if (lookup_wants_float) {
                lookup_pixel = (unsigned char const *)(void const *)
                    lookup_pixel_float;
                if (use_fast_lut) {
                    color_index = sixel_lut_map_pixel(fast_lut,
                                                     lookup_pixel);
                } else {
                    color_index = lookup(lookup_pixel,
                                         depth,
                                         palette,
                                         reqcolor,
                                         indextable,
                                         context->complexion);
                }
            } else if (use_palette_float_lookup) {
                color_index = sixel_dither_lookup_palette_float32(
                    lookup_pixel_float,
                    depth,
                    palette_float,
                    reqcolor,
                    context->complexion,
                    context->use_l_r_distance);
            } else {
                lookup_pixel = quantized;
                if (use_fast_lut) {
                    color_index = sixel_lut_map_pixel(fast_lut,
                                                     lookup_pixel);
                } else {
                    color_index = lookup(lookup_pixel,
                                         depth,
                                         palette,
                                         reqcolor,
                                         indextable,
                                         context->complexion);
                }
            }

                if (optimize_palette) {
                    if (migration_map[color_index] == 0) {
                        output_index = *ncolors;
                        for (n = 0; n < depth; ++n) {
                            new_palette[output_index * depth + n]
                                = palette[color_index * depth + n];
                    }
                    if (palette_float != NULL
                            && new_palette_float != NULL
                            && float_depth > 0) {
                        for (float_index = 0;
                                float_index < float_depth;
                                ++float_index) {
                            new_palette_float[output_index * float_depth
                                              + float_index]
                                = palette_float[color_index * float_depth
                                                + float_index];
                        }
                    }
                    ++*ncolors;
                    /*
                     * Palette entries are capped at SIXEL_PALETTE_MAX (256),
                     * so storing them in unsigned short is safe.
                     */
                    migration_map[color_index] = (unsigned short)(*ncolors);
                } else {
                    output_index = migration_map[color_index] - 1;
                }
                if (absolute_y >= context->output_start) {
                    /*
                     * Palette indices are bounded by SIXEL_PALETTE_MAX and fit
                     * in sixel_index_t (unsigned char).
                     */
                    context->result[pos] = (sixel_index_t)output_index;
                }
            } else {
                output_index = color_index;
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)output_index;
                }
            }
            for (n = 0; n < depth; ++n) {
                if (n > 0 && (
                        context->pixelformat == SIXEL_PIXELFORMAT_OKLABFLOAT32 ||
                        context->pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32 ||
                        context->pixelformat == SIXEL_PIXELFORMAT_DIN99DFLOAT32
                   )) {
                    break;  /* L or Y only */
                }
                if (optimize_palette) {
                    if (have_new_palette_float) {
                        palette_value_float =
                            new_palette_float[output_index * float_depth
                                              + n];
                    } else {
                        palette_value =
                            new_palette[output_index * depth + n];
                        palette_value_float
                            = sixel_pixelformat_byte_to_float(
                                  context->pixelformat,
                                  n,
                                  palette_value);
                    }
                } else {
                    if (have_palette_float) {
                        palette_value_float =
                            palette_float[color_index * float_depth + n];
                    } else {
                        palette_value =
                            palette[color_index * depth + n];
                        palette_value_float
                            = sixel_pixelformat_byte_to_float(
                                  context->pixelformat,
                                  n,
                                  palette_value);
                    }
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
                if (use_carry) {
                    varerr_diffuse_carry(carry_curr,
                                         carry_next,
                                         carry_far,
                                         context->width,
                                         context->height,
                                         depth,
                                         x,
                                         y,
                                         error,
                                         table_index,
                                         direction,
                                         n);
                } else {
                    varerr_diffuse(data + n,
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
        }

        if (use_carry) {
            float *tmp;

            tmp = carry_curr;
            carry_curr = carry_next;
            carry_next = carry_far;
            carry_far = tmp;
            if (carry_len > 0) {
                memset(carry_far, 0x00, carry_len * sizeof(float));
            }
        }
        if (absolute_y >= context->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }

    if (optimize_palette) {
        memcpy(palette, new_palette, (size_t)(*ncolors * depth));
        if (palette_float != NULL
                && new_palette_float != NULL
                && float_depth > 0) {
            memcpy(palette_float,
                   new_palette_float,
                   (size_t)(*ncolors * float_depth)
                       * sizeof(float));
        }
    } else {
        *ncolors = reqcolor;
    }

    status = SIXEL_OK;

end:
    free(carry_curr);
    free(carry_next);
    free(carry_far);
    free(palette_float_buffer);
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
