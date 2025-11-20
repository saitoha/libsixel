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
 * Adaptive (variable coefficient) diffusion backend operating on RGB888
 * buffers.  The implementation mirrors the historical `sixel_dither_apply_
 * variable` helper but now consumes the shared dithering context so both the
 * 8bit and RGBFLOAT32 workers expose identical signatures.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "dither-varcoeff-8bit.h"
#include "dither-common-pipeline.h"
#include "lut.h"

static void
sixel_dither_scanline_params(int serpentine,
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
lso2_table(void))[7]
{
#include "lso2.h"
    return var_coefs;
}

#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

typedef void (*diffuse_varerr_mode)(unsigned char *data,
                                    int width,
                                    int height,
                                    int x,
                                    int y,
                                    int depth,
                                    int32_t error,
                                    int index,
                                    int direction);

typedef void (*diffuse_varerr_carry_mode)(int32_t *carry_curr,
                                          int32_t *carry_next,
                                          int32_t *carry_far,
                                          int width,
                                          int height,
                                          int depth,
                                          int x,
                                          int y,
                                          int32_t error,
                                          int index,
                                          int direction,
                                          int channel);

static int32_t
sixel_varcoeff_safe_denom(int value)
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

    table = lso2_table();
    entry = table[index];
    denom = sixel_varcoeff_safe_denom(entry[6]);

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

static void
diffuse_lso2_carry(int32_t *carry_curr,
                   int32_t *carry_next,
                   int32_t *carry_far,
                   int width,
                   int height,
                   int depth,
                   int x,
                   int y,
                   int32_t error,
                   int index,
                   int direction,
                   int channel)
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
    size_t base;

    if (error == 0) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }

    table = lso2_table();
    entry = table[index];
    denom = sixel_varcoeff_safe_denom(entry[6]);

    term_r = diffuse_varerr_term(error, entry[0], denom);
    term_r2 = diffuse_varerr_term(error, entry[1], denom);
    term_dl = diffuse_varerr_term(error, entry[2], denom);
    term_d = diffuse_varerr_term(error, entry[3], denom);
    term_dr = diffuse_varerr_term(error, entry[4], denom);
    term_d2 = error - term_r - term_r2 - term_dl - term_d - term_dr;

    if (direction >= 0) {
        if (x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth)
                 + (size_t)channel;
            carry_curr[base] += term_r;
        }
        if (x + 2 < width) {
            base = ((size_t)(x + 2) * (size_t)depth)
                 + (size_t)channel;
            carry_curr[base] += term_r2;
        }
        if (y + 1 < height && x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term_dl;
        }
        if (y + 1 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_d;
        }
        if (y + 1 < height && x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term_dr;
        }
        if (y + 2 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_far[base] += term_d2;
        }
    } else {
        if (x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth)
                 + (size_t)channel;
            carry_curr[base] += term_r;
        }
        if (x - 2 >= 0) {
            base = ((size_t)(x - 2) * (size_t)depth)
                 + (size_t)channel;
            carry_curr[base] += term_r2;
        }
        if (y + 1 < height && x + 1 < width) {
            base = ((size_t)(x + 1) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term_dl;
        }
        if (y + 1 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term_d;
        }
        if (y + 1 < height && x - 1 >= 0) {
            base = ((size_t)(x - 1) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term_dr;
        }
        if (y + 2 < height) {
            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_far[base] += term_d2;
        }
    }
}

SIXELSTATUS
sixel_dither_apply_varcoeff_8bit(sixel_dither_t *dither,
                                 sixel_dither_context_t *context)
{
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    SIXELSTATUS status;
    unsigned char *data;
    unsigned char *palette;
    unsigned char *new_palette;
    unsigned char *source_pixel;
    unsigned char palette_value;
    unsigned char corrected[max_channels];
    int32_t sample_scaled[max_channels];
    int32_t accum_scaled[max_channels];
    int32_t target_scaled;
    int32_t error_scaled;
    int32_t *carry_curr;
    int32_t *carry_next;
    int32_t *carry_far;
    int serpentine;
    int use_carry;
    size_t carry_len;
    int method_for_diffuse;
    int method_for_carry;
    int method_for_scan;
    diffuse_varerr_mode varerr_diffuse;
    diffuse_varerr_carry_mode varerr_diffuse_carry;
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
    sixel_lut_t *fast_lut;
    int use_fast_lut;
    float *palette_float;
    float *new_palette_float;
    int float_depth;
    int float_index;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_BAD_ARGUMENT;
    data = context->pixels;
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
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    optimize_palette = context->optimize_palette;
    method_for_diffuse = context->method_for_diffuse;
    method_for_carry = context->method_for_carry;
    method_for_scan = context->method_for_scan;
    if (data == NULL || palette == NULL || context->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
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
        varerr_diffuse = diffuse_lso2;
        varerr_diffuse_carry = diffuse_lso2_carry;
        break;
    }

    use_carry = (method_for_carry == SIXEL_CARRY_ENABLE);
    carry_curr = NULL;
    carry_next = NULL;
    carry_far = NULL;
    carry_len = 0;

    if (use_carry) {
        carry_len = (size_t)context->width * (size_t)depth;
        carry_curr = (int32_t *)calloc(carry_len, sizeof(int32_t));
        carry_next = (int32_t *)calloc(carry_len, sizeof(int32_t));
        carry_far = (int32_t *)calloc(carry_len, sizeof(int32_t));
        if (carry_curr == NULL || carry_next == NULL || carry_far == NULL) {
            goto end;
        }
    }

    serpentine = (method_for_scan == SIXEL_SCAN_SERPENTINE);
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
        sixel_dither_scanline_params(serpentine,
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
                    int64_t accum;
                    int32_t clamped;

                    accum = ((int64_t)data[base + (size_t)n]
                             << VARERR_SCALE_SHIFT)
                          + carry_curr[carry_base + (size_t)n];
                    if (accum < INT32_MIN) {
                        accum = INT32_MIN;
                    } else if (accum > INT32_MAX) {
                        accum = INT32_MAX;
                    }
                    carry_curr[carry_base + (size_t)n] = 0;
                    clamped = (int32_t)accum;
                    if (clamped < 0) {
                        clamped = 0;
                    } else if (clamped > VARERR_MAX_VALUE) {
                        clamped = VARERR_MAX_VALUE;
                    }
                    accum_scaled[n] = clamped;
                    corrected[n]
                        = (unsigned char)((clamped + VARERR_ROUND)
                                          >> VARERR_SCALE_SHIFT);
                }
                source_pixel = corrected;
            } else {
                for (n = 0; n < depth; ++n) {
                    sample_scaled[n]
                        = (int32_t)data[base + (size_t)n]
                        << VARERR_SCALE_SHIFT;
                }
                source_pixel = data + base;
            }

            if (use_fast_lut) {
                color_index = sixel_lut_map_pixel(fast_lut, source_pixel);
            } else {
                color_index = lookup(source_pixel,
                                      depth,
                                      palette,
                                      reqcolor,
                                      indextable,
                                      context->complexion);
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
                    migration_map[color_index] = *ncolors;
                } else {
                    output_index = migration_map[color_index] - 1;
                }
                if (absolute_y >= context->output_start) {
                    context->result[pos] = output_index;
                }
            } else {
                output_index = color_index;
                if (absolute_y >= context->output_start) {
                    context->result[pos] = output_index;
                }
            }

            for (n = 0; n < depth; ++n) {
                if (optimize_palette) {
                    palette_value = new_palette[output_index * depth + n];
                } else {
                    palette_value = palette[color_index * depth + n];
                }
                diff = (int)source_pixel[n] - (int)palette_value;
                if (diff < 0) {
                    diff = -diff;
                }
                if (diff > 255) {
                    diff = 255;
                }
                table_index = diff;
                if (use_carry) {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = accum_scaled[n] - target_scaled;
                    varerr_diffuse_carry(carry_curr,
                                         carry_next,
                                         carry_far,
                                         context->width,
                                         context->height,
                                         depth,
                                         x,
                                         y,
                                         error_scaled,
                                         table_index,
                                         direction,
                                         n);
                } else {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = sample_scaled[n] - target_scaled;
                    varerr_diffuse(data + n,
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
        }

        if (use_carry) {
            int32_t *tmp;

            tmp = carry_curr;
            carry_curr = carry_next;
            carry_next = carry_far;
            carry_far = tmp;
            if (carry_len > 0) {
                memset(carry_far, 0x00, carry_len * sizeof(int32_t));
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
