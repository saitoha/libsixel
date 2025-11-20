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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */

#include "dither-fixed-8bit.h"
#include "dither-common-pipeline.h"
#include "lookup-common.h"

/*
 * Local serpentine traversal helper.  The function mirrors the behaviour used
 * by other dithering strategies without forcing additional shared headers.
 */
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

#define VARERR_SCALE_SHIFT 12
#define VARERR_SCALE       (1 << VARERR_SCALE_SHIFT)
#define VARERR_ROUND       (1 << (VARERR_SCALE_SHIFT - 1))
#define VARERR_MAX_VALUE   (255 * VARERR_SCALE)

static void
error_diffuse_normal(
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

static void
error_diffuse_fast(
    unsigned char /* in */    *data,      /* base address of pixel buffer */
    int           /* in */    pos,        /* address of the destination pixel */
    int           /* in */    depth,      /* color depth in bytes */
    int           /* in */    error,      /* error energy */
    int           /* in */    numerator,  /* numerator of diffusion coefficient */
    int           /* in */    denominator /* denominator of diffusion coefficient */)
{
    int c;

    data += pos * depth;

    c = *data + error * numerator / denominator;
    if (c < 0) {
        c = 0;
    }
    if (c >= 1 << 8) {
        c = (1 << 8) - 1;
    }
    *data = (unsigned char)c;
}

/* error diffusion with precise strategy */
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

typedef void (*diffuse_fixed_carry_mode)(int32_t *carry_curr,
                                         int32_t *carry_next,
                                         int32_t *carry_far,
                                         int width,
                                         int height,
                                         int depth,
                                         int x,
                                         int y,
                                         int32_t error,
                                         int direction,
                                         int channel);

static int32_t
diffuse_fixed_term(int32_t error, int numerator, int denominator)
{
    int64_t delta;

    delta = (int64_t)error * (int64_t)numerator;
    if (delta >= 0) {
        delta = (delta + denominator / 2) / denominator;
    } else {
        delta = (delta - denominator / 2) / denominator;
    }

    return (int32_t)delta;
}

static void diffuse_none(unsigned char *data,
                         int width,
                         int height,
                         int x,
                         int y,
                         int depth,
                         int error,
                         int direction);

static void diffuse_none_carry(int32_t *carry_curr,
                               int32_t *carry_next,
                               int32_t *carry_far,
                               int width,
                               int height,
                               int depth,
                               int x,
                               int y,
                               int32_t error,
                               int direction,
                               int channel);

static void diffuse_fs(unsigned char *data,
                       int width,
                       int height,
                       int x,
                       int y,
                       int depth,
                       int error,
                       int direction);

static void diffuse_fs_carry(int32_t *carry_curr,
                             int32_t *carry_next,
                             int32_t *carry_far,
                             int width,
                             int height,
                             int depth,
                             int x,
                             int y,
                             int32_t error,
                             int direction,
                             int channel);

static void diffuse_atkinson(unsigned char *data,
                             int width,
                             int height,
                             int x,
                             int y,
                             int depth,
                             int error,
                             int direction);

static void diffuse_atkinson_carry(int32_t *carry_curr,
                                   int32_t *carry_next,
                                   int32_t *carry_far,
                                   int width,
                                   int height,
                                   int depth,
                                   int x,
                                   int y,
                                   int32_t error,
                                   int direction,
                                   int channel);

static void diffuse_jajuni(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static void diffuse_jajuni_carry(int32_t *carry_curr,
                                 int32_t *carry_next,
                                 int32_t *carry_far,
                                 int width,
                                 int height,
                                 int depth,
                                 int x,
                                 int y,
                                 int32_t error,
                                 int direction,
                                 int channel);

static void diffuse_stucki(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static void diffuse_stucki_carry(int32_t *carry_curr,
                                 int32_t *carry_next,
                                 int32_t *carry_far,
                                 int width,
                                 int height,
                                 int depth,
                                 int x,
                                 int y,
                                 int32_t error,
                                 int direction,
                                 int channel);

static void diffuse_burkes(unsigned char *data,
                           int width,
                           int height,
                           int x,
                           int y,
                           int depth,
                           int error,
                           int direction);

static void diffuse_burkes_carry(int32_t *carry_curr,
                                 int32_t *carry_next,
                                 int32_t *carry_far,
                                 int width,
                                 int height,
                                 int depth,
                                 int x,
                                 int y,
                                 int32_t error,
                                 int direction,
                                 int channel);

static void diffuse_sierra1(unsigned char *data,
                            int width,
                            int height,
                            int x,
                            int y,
                            int depth,
                            int error,
                            int direction);

static void diffuse_sierra1_carry(int32_t *carry_curr,
                                  int32_t *carry_next,
                                  int32_t *carry_far,
                                  int width,
                                  int height,
                                  int depth,
                                  int x,
                                  int y,
                                  int32_t error,
                                  int direction,
                                  int channel);

static void diffuse_sierra2(unsigned char *data,
                            int width,
                            int height,
                            int x,
                            int y,
                            int depth,
                            int error,
                            int direction);

static void diffuse_sierra2_carry(int32_t *carry_curr,
                                  int32_t *carry_next,
                                  int32_t *carry_far,
                                  int width,
                                  int height,
                                  int depth,
                                  int x,
                                  int y,
                                  int32_t error,
                                  int direction,
                                  int channel);

static void diffuse_sierra3(unsigned char *data,
                            int width,
                            int height,
                            int x,
                            int y,
                            int depth,
                            int error,
                            int direction);

static void diffuse_sierra3_carry(int32_t *carry_curr,
                                  int32_t *carry_next,
                                  int32_t *carry_far,
                                  int width,
                                  int height,
                                  int depth,
                                  int x,
                                  int y,
                                  int32_t error,
                                  int direction,
                                  int channel);

static SIXELSTATUS
sixel_dither_apply_fixed_impl(
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
    int optimize_palette,
    int (*f_lookup)(const unsigned char *pixel,
                    int depth,
                    const unsigned char *palette,
                    int reqcolor,
                    unsigned short *cachetable,
                    int complexion),
    sixel_lut_t *fast_lut,
    int use_fast_lut,
    unsigned short *indextable,
    int complexion,
    unsigned char new_palette[],
    unsigned short migration_map[],
    int *ncolors,
    int method_for_diffuse,
    int method_for_carry,
    float *palette_float,
    float *new_palette_float,
    int float_depth,
    sixel_dither_t *dither)
{
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    SIXELSTATUS status = SIXEL_FALSE;
    int serpentine;
    int y;
    void (*f_diffuse)(unsigned char *data,
                      int width,
                      int height,
                      int x,
                      int y,
                      int depth,
                      int offset,
                      int direction);
    diffuse_fixed_carry_mode f_diffuse_carry;
    int use_carry;
    size_t carry_len;
    int32_t *carry_curr = NULL;
    int32_t *carry_next = NULL;
    int32_t *carry_far = NULL;
    unsigned char corrected[max_channels];
    int32_t accum_scaled[max_channels];
    int start;
    int end;
    int step;
    int direction;
    int x;
    int absolute_y;
    int pos;
    size_t base;
    size_t carry_base;
    const unsigned char *source_pixel;
    int color_index;
    int output_index;
    int n;
    int palette_value;
    int64_t accum;
    int64_t clamped;
    int32_t target_scaled;
    int32_t error_scaled;
    int offset;
    int float_index;
    int32_t *tmp;

    if (depth > max_channels) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    use_carry = (method_for_carry == SIXEL_CARRY_ENABLE);
    carry_len = 0;

    if (depth != 3) {
        f_diffuse = diffuse_none;
        f_diffuse_carry = diffuse_none_carry;
        use_carry = 0;
    } else {
        switch (method_for_diffuse) {
        case SIXEL_DIFFUSE_NONE:
            f_diffuse = diffuse_none;
            f_diffuse_carry = diffuse_none_carry;
            break;
        case SIXEL_DIFFUSE_ATKINSON:
            f_diffuse = diffuse_atkinson;
            f_diffuse_carry = diffuse_atkinson_carry;
            break;
        case SIXEL_DIFFUSE_FS:
            f_diffuse = diffuse_fs;
            f_diffuse_carry = diffuse_fs_carry;
            break;
        case SIXEL_DIFFUSE_JAJUNI:
            f_diffuse = diffuse_jajuni;
            f_diffuse_carry = diffuse_jajuni_carry;
            break;
        case SIXEL_DIFFUSE_STUCKI:
            f_diffuse = diffuse_stucki;
            f_diffuse_carry = diffuse_stucki_carry;
            break;
        case SIXEL_DIFFUSE_BURKES:
            f_diffuse = diffuse_burkes;
            f_diffuse_carry = diffuse_burkes_carry;
            break;
        case SIXEL_DIFFUSE_SIERRA1:
            f_diffuse = diffuse_sierra1;
            f_diffuse_carry = diffuse_sierra1_carry;
            break;
        case SIXEL_DIFFUSE_SIERRA2:
            f_diffuse = diffuse_sierra2;
            f_diffuse_carry = diffuse_sierra2_carry;
            break;
        case SIXEL_DIFFUSE_SIERRA3:
            f_diffuse = diffuse_sierra3;
            f_diffuse_carry = diffuse_sierra3_carry;
            break;
        default:
            f_diffuse = diffuse_none;
            f_diffuse_carry = diffuse_none_carry;
            break;
        }
    }

    if (use_carry) {
        carry_len = (size_t)width * (size_t)depth;
        if (carry_len > 0) {
            carry_curr = (int32_t *)calloc(carry_len, sizeof(int32_t));
            if (carry_curr == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            carry_next = (int32_t *)calloc(carry_len, sizeof(int32_t));
            if (carry_next == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            carry_far = (int32_t *)calloc(carry_len, sizeof(int32_t));
            if (carry_far == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        } else {
            use_carry = 0;
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
    } else {
        *ncolors = reqcolor;
    }

    for (y = 0; y < height; ++y) {
        absolute_y = band_origin + y;
        sixel_dither_scanline_params(serpentine, absolute_y, width,
                        &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * width + x;
            base = (size_t)pos * (size_t)depth;
            carry_base = (size_t)x * (size_t)depth;
            if (use_carry) {
                for (n = 0; n < depth; ++n) {
                    accum = ((int64_t)data[base + n]
                             << VARERR_SCALE_SHIFT)
                           + carry_curr[carry_base + (size_t)n];
                    if (accum < INT32_MIN) {
                        accum = INT32_MIN;
                    } else if (accum > INT32_MAX) {
                        accum = INT32_MAX;
                    }
                    clamped = accum;
                    if (clamped < 0) {
                        clamped = 0;
                    } else if (clamped > VARERR_MAX_VALUE) {
                        clamped = VARERR_MAX_VALUE;
                    }
                    accum_scaled[n] = (int32_t)clamped;
                    corrected[n]
                        = (unsigned char)((clamped + VARERR_ROUND)
                                          >> VARERR_SCALE_SHIFT);
                    data[base + n] = corrected[n];
                    carry_curr[carry_base + (size_t)n] = 0;
                }
                source_pixel = corrected;
            } else {
                source_pixel = data + base;
            }

            if (use_fast_lut && fast_lut != NULL) {
                color_index = sixel_lut_map_pixel(fast_lut, source_pixel);
            } else {
                color_index = f_lookup(source_pixel, depth, palette,
                                       reqcolor, indextable,
                                       complexion);
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
                if (absolute_y >= output_start) {
                    result[pos] = output_index;
                }
            } else {
                output_index = color_index;
                if (absolute_y >= output_start) {
                    result[pos] = output_index;
                }
            }

            for (n = 0; n < depth; ++n) {
                if (optimize_palette) {
                    palette_value = new_palette[output_index * depth + n];
                } else {
                    palette_value = palette[color_index * depth + n];
                }
                if (use_carry) {
                    target_scaled = (int32_t)palette_value
                                  << VARERR_SCALE_SHIFT;
                    error_scaled = accum_scaled[n] - target_scaled;
                    f_diffuse_carry(carry_curr, carry_next, carry_far,
                                    width, height, depth,
                                    x, y, error_scaled, direction, n);
                } else {
                    offset = (int)source_pixel[n] - palette_value;
                    f_diffuse(data + n, width, height, x, y,
                              depth, offset, direction);
                }
            }
        }
        if (use_carry) {
            tmp = carry_curr;
            carry_curr = carry_next;
            carry_next = carry_far;
            carry_far = tmp;
            if (carry_len > 0) {
                memset(carry_far, 0x00, carry_len * sizeof(int32_t));
            }
        }
        if (absolute_y >= output_start) {
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
                   (size_t)(*ncolors * float_depth) * sizeof(float));
        }
    }

    status = SIXEL_OK;

end:
    free(carry_far);
    free(carry_next);
    free(carry_curr);
    return status;
}

SIXELSTATUS
sixel_dither_apply_fixed_8bit(sixel_dither_t *dither,
                              sixel_dither_context_t *context)
{
    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->pixels == NULL || context->palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->result == NULL || context->new_palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->migration_map == NULL || context->ncolors == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->lookup == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_dither_apply_fixed_impl(context->result,
                                         context->pixels,
                                         context->width,
                                         context->height,
                                         context->band_origin,
                                         context->output_start,
                                         context->depth,
                                         context->palette,
                                         context->reqcolor,
                                         context->method_for_scan,
                                         context->optimize_palette,
                                         context->lookup,
                                         context->lut,
                                         context->lut != NULL,
                                         context->indextable,
                                         context->complexion,
                                         context->new_palette,
                                         context->migration_map,
                                         context->ncolors,
                                         context->method_for_diffuse,
                                         context->method_for_carry,
                                         context->palette_float,
                                         context->new_palette_float,
                                         context->float_depth,
                                         dither);
}

static void
diffuse_none(unsigned char *data, int width, int height,
             int x, int y, int depth, int error, int direction)
{
    /* unused */ (void) data;
    /* unused */ (void) width;
    /* unused */ (void) height;
    /* unused */ (void) x;
    /* unused */ (void) y;
    /* unused */ (void) depth;
    /* unused */ (void) error;
    /* unused */ (void) direction;
}


static void
diffuse_none_carry(int32_t *carry_curr, int32_t *carry_next,
                   int32_t *carry_far, int width, int height,
                   int depth, int x, int y, int32_t error,
                   int direction, int channel)
{
    /* unused */ (void) carry_curr;
    /* unused */ (void) carry_next;
    /* unused */ (void) carry_far;
    /* unused */ (void) width;
    /* unused */ (void) height;
    /* unused */ (void) depth;
    /* unused */ (void) x;
    /* unused */ (void) y;
    /* unused */ (void) error;
    /* unused */ (void) direction;
    /* unused */ (void) channel;
}

static void
diffuse_fs(unsigned char *data, int width, int height,
           int x, int y, int depth, int error, int direction)
{
    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/16    1/16
     */
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    if (forward) {
        if (x < width - 1) {
            error_diffuse_normal(data, pos + 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x > 0) {
                error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 3, 16);
            }
            error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x < width - 1) {
                error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 1, 16);
            }
        }
    } else {
        if (x > 0) {
            error_diffuse_normal(data, pos - 1, depth, error, 7, 16);
        }
        if (y < height - 1) {
            if (x < width - 1) {
                error_diffuse_normal(data,
                                     pos + width + 1,
                                     depth, error, 3, 16);
            }
            error_diffuse_normal(data,
                                 pos + width,
                                 depth, error, 5, 16);
            if (x > 0) {
                error_diffuse_normal(data,
                                     pos + width - 1,
                                     depth, error, 1, 16);
            }
        }
    }
}


static void
diffuse_fs_carry(int32_t *carry_curr, int32_t *carry_next,
                 int32_t *carry_far, int width, int height,
                 int depth, int x, int y, int32_t error,
                 int direction, int channel)
{
    /* Floyd Steinberg Method
     *          curr    7/16
     *  3/16    5/48    1/16
     */
    int forward;

    /* unused */ (void) carry_far;
    if (error == 0) {
        return;
    }

    forward = direction >= 0;
    if (forward) {
        if (x + 1 < width) {
            size_t base;
            int32_t term;

            base = ((size_t)(x + 1) * (size_t)depth)
                 + (size_t)channel;
            term = diffuse_fixed_term(error, 7, 16);
            carry_curr[base] += term;
        }
        if (y + 1 < height) {
            if (x > 0) {
                size_t base;
                int32_t term;

                base = ((size_t)(x - 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 3, 16);
                carry_next[base] += term;
            }
            {
                size_t base;
                int32_t term;

                base = ((size_t)x * (size_t)depth) + (size_t)channel;
                term = diffuse_fixed_term(error, 5, 16);
                carry_next[base] += term;
            }
            if (x + 1 < width) {
                size_t base;
                int32_t term;

                base = ((size_t)(x + 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 1, 16);
                carry_next[base] += term;
            }
        }
    } else {
        if (x - 1 >= 0) {
            size_t base;
            int32_t term;

            base = ((size_t)(x - 1) * (size_t)depth)
                 + (size_t)channel;
            term = diffuse_fixed_term(error, 7, 16);
            carry_curr[base] += term;
        }
        if (y + 1 < height) {
            if (x + 1 < width) {
                size_t base;
                int32_t term;

                base = ((size_t)(x + 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 3, 16);
                carry_next[base] += term;
            }
            {
                size_t base;
                int32_t term;

                base = ((size_t)x * (size_t)depth) + (size_t)channel;
                term = diffuse_fixed_term(error, 5, 16);
                carry_next[base] += term;
            }
            if (x - 1 >= 0) {
                size_t base;
                int32_t term;

                base = ((size_t)(x - 1) * (size_t)depth)
                     + (size_t)channel;
                term = diffuse_fixed_term(error, 1, 16);
                carry_next[base] += term;
            }
        }
    }
}


static void
diffuse_atkinson(unsigned char *data, int width, int height,
                 int x, int y, int depth, int error, int direction)
{
    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    int pos;
    int sign;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    if (x + sign >= 0 && x + sign < width) {
        error_diffuse_fast(data, pos + sign, depth, error, 1, 8);
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        error_diffuse_fast(data, pos + sign * 2, depth, error, 1, 8);
    }
    if (y < height - 1) {
        int row;

        row = pos + width;
        if (x - sign >= 0 && x - sign < width) {
            error_diffuse_fast(data,
                               row + (-sign),
                               depth, error, 1, 8);
        }
        error_diffuse_fast(data, row, depth, error, 1, 8);
        if (x + sign >= 0 && x + sign < width) {
            error_diffuse_fast(data,
                               row + sign,
                               depth, error, 1, 8);
        }
    }
    if (y < height - 2) {
        error_diffuse_fast(data, pos + width * 2, depth, error, 1, 8);
    }
}


static void
diffuse_atkinson_carry(int32_t *carry_curr, int32_t *carry_next,
                       int32_t *carry_far, int width, int height,
                       int depth, int x, int y, int32_t error,
                       int direction, int channel)
{
    /* Atkinson's Method
     *          curr    1/8    1/8
     *   1/8     1/8    1/8
     *           1/8
     */
    int sign;
    int32_t term;

    if (error == 0) {
        return;
    }

    term = diffuse_fixed_term(error, 1, 8);
    sign = direction >= 0 ? 1 : -1;
    if (x + sign >= 0 && x + sign < width) {
        size_t base;

        base = ((size_t)(x + sign) * (size_t)depth)
             + (size_t)channel;
        carry_curr[base] += term;
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        size_t base;

        base = ((size_t)(x + sign * 2) * (size_t)depth)
             + (size_t)channel;
        carry_curr[base] += term;
    }
    if (y + 1 < height) {
        if (x - sign >= 0 && x - sign < width) {
            size_t base;

            base = ((size_t)(x - sign) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term;
        }
        {
            size_t base;

            base = ((size_t)x * (size_t)depth) + (size_t)channel;
            carry_next[base] += term;
        }
        if (x + sign >= 0 && x + sign < width) {
            size_t base;

            base = ((size_t)(x + sign) * (size_t)depth)
                 + (size_t)channel;
            carry_next[base] += term;
        }
    }
    if (y + 2 < height) {
        size_t base;

        base = ((size_t)x * (size_t)depth) + (size_t)channel;
        carry_far[base] += term;
    }
}


static void
diffuse_jajuni(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_weights[] = { 7, 5 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_weights[] = { 3, 5, 7, 5, 3 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_weights[] = { 1, 3, 5, 3, 1 };
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
                              row0_weights[i], 48);
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
                                  row1_weights[i], 48);
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
                                  row2_weights[i], 48);
        }
    }
}


static void
diffuse_jajuni_carry(int32_t *carry_curr, int32_t *carry_next,
                     int32_t *carry_far, int width, int height,
                     int depth, int x, int y, int32_t error,
                     int direction, int channel)
{
    /* Jarvis, Judice & Ninke Method
     *                  curr    7/48    5/48
     *  3/48    5/48    7/48    5/48    3/48
     *  1/48    3/48    5/48    3/48    1/48
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_weights[] = { 7, 5 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_weights[] = { 3, 5, 7, 5, 3 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_weights[] = { 1, 3, 5, 3, 1 };
    int sign;
    int i;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        int neighbor;
        int32_t term;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_weights[i], 48);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_weights[i], 48);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_weights[i], 48);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
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
diffuse_stucki_carry(int32_t *carry_curr, int32_t *carry_next,
                     int32_t *carry_far, int width, int height,
                     int depth, int x, int y, int32_t error,
                     int direction, int channel)
{
    /* Stucki's Method
     *                  curr    8/48    4/48
     *  2/48    4/48    8/48    4/48    2/48
     *  1/48    2/48    4/48    2/48    1/48
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 6, 12 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 24, 12, 6, 12, 24 };
    static const int row2_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row2_num[] = { 1, 1, 1, 1, 1 };
    static const int row2_den[] = { 48, 24, 12, 24, 48 };
    int sign;
    int i;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        int neighbor;
        int32_t term;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_num[i], row2_den[i]);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}


static void
diffuse_burkes(unsigned char *data, int width, int height,
               int x, int y, int depth, int error, int direction)
{
    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    int pos;
    int sign;
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
    int i;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
        int neighbor;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_normal(data,
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
            error_diffuse_normal(data,
                                 row + (neighbor - x),
                                 depth, error,
                                 row1_num[i], row1_den[i]);
        }
    }
}

static void
diffuse_burkes_carry(int32_t *carry_curr, int32_t *carry_next,
                     int32_t *carry_far, int width, int height,
                     int depth, int x, int y, int32_t error,
                     int direction, int channel)
{
    /* Burkes' Method
     *                  curr    4/16    2/16
     *  1/16    2/16    4/16    2/16    1/16
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 1, 1 };
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
    int sign;
    int i;

    /* unused */ (void) carry_far;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        int neighbor;
        int32_t term;

        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            int neighbor;
            int32_t term;

            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
}

static void
diffuse_sierra1(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 1; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        error_diffuse_normal(data,
                             pos + (neighbor - x),
                             depth, error,
                             row0_num[i], row0_den[i]);
    }
    if (y < height - 1) {
        row = pos + width;
        for (i = 0; i < 2; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            error_diffuse_normal(data,
                                 row + (neighbor - x),
                                 depth, error,
                                 row1_num[i], row1_den[i]);
        }
    }
}


static void
diffuse_sierra1_carry(int32_t *carry_curr, int32_t *carry_next,
                      int32_t *carry_far, int width, int height,
                      int depth, int x, int y, int32_t error,
                      int direction, int channel)
{
    /* Sierra Lite Method
     *          curr    2/4
     *  1/4     1/4
     */
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
    int sign;
    int i;
    int neighbor;
    int32_t term;

    /* unused */ (void) carry_far;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 1; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 2; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
}


static void
diffuse_sierra2(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra Two-row Method
     *                  curr    4/32    3/32
     *  1/32    2/32    3/32    2/32    1/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
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
        row = pos + width;
        for (i = 0; i < 5; ++i) {
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
        row = pos + width * 2;
        for (i = 0; i < 3; ++i) {
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
diffuse_sierra2_carry(int32_t *carry_curr, int32_t *carry_next,
                      int32_t *carry_far, int width, int height,
                      int depth, int x, int y, int32_t error,
                      int direction, int channel)
{
    /* Sierra Two-row Method
     *                  curr    4/32    3/32
     *  1/32    2/32    3/32    2/32    1/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int sign;
    int i;
    int neighbor;
    int32_t term;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_num[i], row2_den[i]);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}


static void
diffuse_sierra3(unsigned char *data, int width, int height,
                int x, int y, int depth, int error, int direction)
{
    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int pos;
    int sign;
    int i;
    int neighbor;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    for (i = 0; i < 2; ++i) {
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
        row = pos + width;
        for (i = 0; i < 5; ++i) {
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
        row = pos + width * 2;
        for (i = 0; i < 3; ++i) {
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
diffuse_sierra3_carry(int32_t *carry_curr, int32_t *carry_next,
                      int32_t *carry_far, int width, int height,
                      int depth, int x, int y, int32_t error,
                      int direction, int channel)
{
    /* Sierra-3 Method
     *                  curr    5/32    3/32
     *  2/32    4/32    5/32    4/32    2/32
     *                  2/32    3/32    2/32
     */
    static const int row0_offsets[] = { 1, 2 };
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
    int sign;
    int i;
    int neighbor;
    int32_t term;

    if (error == 0) {
        return;
    }

    sign = direction >= 0 ? 1 : -1;
    for (i = 0; i < 2; ++i) {
        neighbor = x + sign * row0_offsets[i];
        if (neighbor < 0 || neighbor >= width) {
            continue;
        }
        term = diffuse_fixed_term(error, row0_num[i], row0_den[i]);
        carry_curr[((size_t)neighbor * (size_t)depth)
                   + (size_t)channel] += term;
    }
    if (y + 1 < height) {
        for (i = 0; i < 5; ++i) {
            neighbor = x + sign * row1_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row1_num[i], row1_den[i]);
            carry_next[((size_t)neighbor * (size_t)depth)
                       + (size_t)channel] += term;
        }
    }
    if (y + 2 < height) {
        for (i = 0; i < 3; ++i) {
            neighbor = x + sign * row2_offsets[i];
            if (neighbor < 0 || neighbor >= width) {
                continue;
            }
            term = diffuse_fixed_term(error, row2_num[i], row2_den[i]);
            carry_far[((size_t)neighbor * (size_t)depth)
                      + (size_t)channel] += term;
        }
    }
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
