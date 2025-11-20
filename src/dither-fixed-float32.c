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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
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

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#include <string.h>

#include "dither-fixed-float32.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "lut.h"

typedef void (*diffuse_fixed_float_fn)(float *data,
                                       int width,
                                       int height,
                                       int x,
                                       int y,
                                       int depth,
                                       float error,
                                       int direction,
                                       int pixelformat,
                                       int channel_index);

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

static void
diffuse_none_float(float *data,
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
    (void)data;
    (void)width;
    (void)height;
    (void)x;
    (void)y;
    (void)depth;
    (void)error;
    (void)direction;
    (void)pixelformat;
    (void)channel_index;
}

static void
diffuse_fs_float(float *data,
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
    int pos;
    int forward;

    pos = y * width + x;
    forward = direction >= 0;

    if (forward) {
        if (x < width - 1) {
            error_diffuse_float(data,
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
                error_diffuse_float(data,
                                    pos + width - 1,
                                    depth,
                                    error,
                                    3,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
            error_diffuse_float(data,
                                pos + width,
                                depth,
                                error,
                                5,
                                16,
                                pixelformat,
                                channel_index);
            if (x < width - 1) {
                error_diffuse_float(data,
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
            error_diffuse_float(data,
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
                error_diffuse_float(data,
                                    pos + width + 1,
                                    depth,
                                    error,
                                    3,
                                    16,
                                    pixelformat,
                                    channel_index);
            }
            error_diffuse_float(data,
                                pos + width,
                                depth,
                                error,
                                5,
                                16,
                                pixelformat,
                                channel_index);
            if (x > 0) {
                error_diffuse_float(data,
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

/*
 * Atkinson's kernel spreads the error within a 3x3 neighborhood using
 * symmetric 1/8 weights.  The float variant mirrors the integer version
 * but keeps the higher precision samples intact.
 */
static void
diffuse_atkinson_float(float *data,
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
    int pos;
    int sign;
    int row;

    pos = y * width + x;
    sign = direction >= 0 ? 1 : -1;

    if (x + sign >= 0 && x + sign < width) {
        error_diffuse_float(data,
                            pos + sign,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
    }
    if (x + sign * 2 >= 0 && x + sign * 2 < width) {
        error_diffuse_float(data,
                            pos + sign * 2,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
    }
    if (y < height - 1) {
        row = pos + width;
        if (x - sign >= 0 && x - sign < width) {
            error_diffuse_float(data,
                                row - sign,
                                depth,
                                error,
                                1,
                                8,
                                pixelformat,
                                channel_index);
        }
        error_diffuse_float(data,
                            row,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
        if (x + sign >= 0 && x + sign < width) {
            error_diffuse_float(data,
                                row + sign,
                                depth,
                                error,
                                1,
                                8,
                                pixelformat,
                                channel_index);
        }
    }
    if (y < height - 2) {
        error_diffuse_float(data,
                            pos + width * 2,
                            depth,
                            error,
                            1,
                            8,
                            pixelformat,
                            channel_index);
    }
}

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
static void
diffuse_jajuni_float(float *data,
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
static void
diffuse_burkes_float(float *data,
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
    static const int row0_den[] = { 4, 8 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 1, 1, 1, 1 };
    static const int row1_den[] = { 16, 8, 4, 8, 16 };
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
}

/*
 * Sierra Lite (Sierra1) uses a compact 2x2 mask to reduce ringing while
 * keeping serpentine traversal stable.
 */
static void
diffuse_sierra1_float(float *data,
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
    static const int row0_offsets[] = { 1 };
    static const int row0_num[] = { 1 };
    static const int row0_den[] = { 2 };
    static const int row1_offsets[] = { -1, 0 };
    static const int row1_num[] = { 1, 1 };
    static const int row1_den[] = { 4, 4 };
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
                         1);
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
                             2);
    }
}

/*
 * Sierra Two-row keeps the full 5x3 footprint but halves the lower row weights
 * relative to Sierra-3, matching the 32-denominator formulation.
 */
static void
diffuse_sierra2_float(float *data,
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
    static const int row0_num[] = { 4, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 1, 2, 3, 2, 1 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
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
                             3);
    }
}

/*
 * Sierra-3 restores the heavier middle-row contributions (5/32) that
 * characterize the original kernel.
 */
static void
diffuse_sierra3_float(float *data,
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
    static const int row0_num[] = { 5, 3 };
    static const int row0_den[] = { 32, 32 };
    static const int row1_offsets[] = { -2, -1, 0, 1, 2 };
    static const int row1_num[] = { 2, 4, 5, 4, 2 };
    static const int row1_den[] = { 32, 32, 32, 32, 32 };
    static const int row2_offsets[] = { -1, 0, 1 };
    static const int row2_num[] = { 2, 3, 2 };
    static const int row2_den[] = { 32, 32, 32 };
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
                             3);
    }
}

#if HAVE_TESTS
static int g_sixel_dither_float32_diffusion_hits = 0;

void
sixel_dither_diffusion_tests_reset_float32_hits(void)
{
    g_sixel_dither_float32_diffusion_hits = 0;
}

int
sixel_dither_diffusion_tests_float32_hits(void)
{
    return g_sixel_dither_float32_diffusion_hits;
}

#define SIXEL_DITHER_FLOAT32_HIT()                                      \
    do {                                                                \
        ++g_sixel_dither_float32_diffusion_hits;                        \
    } while (0)
#else
#define SIXEL_DITHER_FLOAT32_HIT()                                      \
    do {                                                                \
    } while (0)
#endif

SIXELSTATUS
sixel_dither_apply_fixed_float32(sixel_dither_t *dither,
                                 sixel_dither_context_t *context)
{
#if _MSC_VER
    enum { max_channels = 4 };
#else
    const int max_channels = 4;
#endif
    SIXELSTATUS status;
    float *palette_float;
    float *new_palette_float;
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
    unsigned char quantized[max_channels];
    float snapshot[max_channels];
    float lookup_pixel_float[max_channels];
    int color_index;
    int output_index;
    int palette_value;
    float palette_value_float;
    float error;
    int n;
    float *data;
    unsigned char *palette;
    int float_index;
    int lookup_wants_float;
    int use_palette_float_lookup;
    int need_float_pixel;
    unsigned char const *lookup_pixel;
    sixel_lut_t *fast_lut;
    int use_fast_lut;
    int have_palette_float;
    int have_new_palette_float;
    diffuse_fixed_float_fn f_diffuse;
    int method_for_diffuse;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;

    if (dither == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    data = context->pixels_float;
    if (data == NULL || context->palette == NULL) {
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

    palette = context->palette;
    palette_float = context->palette_float;
    new_palette_float = context->new_palette_float;
    float_depth = context->float_depth;
    if (context->depth > max_channels || context->depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->reqcolor < 1) {
        return SIXEL_BAD_ARGUMENT;
    }

    fast_lut = context->lut;
    use_fast_lut = (fast_lut != NULL);

    serpentine = (context->method_for_scan == SIXEL_SCAN_SERPENTINE);
    lookup_wants_float = (context->lookup_source_is_float != 0);
    use_palette_float_lookup = 0;
    if (context->prefer_palette_float_lookup != 0
            && palette_float != NULL
            && float_depth >= context->depth) {
        use_palette_float_lookup = 1;
    }
    need_float_pixel = lookup_wants_float || use_palette_float_lookup;

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
    if (new_palette_float != NULL && float_depth >= context->depth) {
        have_new_palette_float = 1;
    } else {
        have_new_palette_float = 0;
    }

    method_for_diffuse = context->method_for_diffuse;
    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_NONE:
        f_diffuse = diffuse_none_float;
        break;
    case SIXEL_DIFFUSE_ATKINSON:
        f_diffuse = diffuse_atkinson_float;
        break;
    case SIXEL_DIFFUSE_JAJUNI:
        f_diffuse = diffuse_jajuni_float;
        break;
    case SIXEL_DIFFUSE_STUCKI:
        f_diffuse = diffuse_stucki_float;
        break;
    case SIXEL_DIFFUSE_BURKES:
        f_diffuse = diffuse_burkes_float;
        break;
    case SIXEL_DIFFUSE_SIERRA1:
        f_diffuse = diffuse_sierra1_float;
        break;
    case SIXEL_DIFFUSE_SIERRA2:
        f_diffuse = diffuse_sierra2_float;
        break;
    case SIXEL_DIFFUSE_SIERRA3:
        f_diffuse = diffuse_sierra3_float;
        break;
    case SIXEL_DIFFUSE_FS:
    default:
        f_diffuse = diffuse_fs_float;
        break;
    }

    if (context->optimize_palette) {
        *context->ncolors = 0;
        memset(context->new_palette, 0x00,
               (size_t)SIXEL_PALETTE_MAX * (size_t)context->depth);
        if (new_palette_float != NULL && float_depth > 0) {
            memset(new_palette_float, 0x00,
                   (size_t)SIXEL_PALETTE_MAX
                       * (size_t)float_depth * sizeof(float));
        }
        memset(context->migration_map, 0x00,
               sizeof(unsigned short) * (size_t)SIXEL_PALETTE_MAX);
    } else {
        *context->ncolors = context->reqcolor;
    }

    for (y = 0; y < context->height; ++y) {
        absolute_y = context->band_origin + y;
        sixel_dither_scanline_params(serpentine, absolute_y,
                                     context->width,
                                     &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            base = (size_t)pos * (size_t)context->depth;
            source_pixel = data + base;

            for (n = 0; n < context->depth; ++n) {
                snapshot[n] = source_pixel[n];
                if (need_float_pixel) {
                    lookup_pixel_float[n] = source_pixel[n];
                }
                if (!lookup_wants_float && !use_palette_float_lookup) {
                    quantized[n]
                        = sixel_pixelformat_float_channel_to_byte(
                              context->pixelformat,
                              n,
                              source_pixel[n]);
                }
            }

            if (lookup_wants_float) {
                lookup_pixel = (unsigned char const *)(void const *)source_pixel;
                if (use_fast_lut) {
                    color_index = sixel_lut_map_pixel(fast_lut,
                                                     lookup_pixel);
                } else {
                    color_index = context->lookup(lookup_pixel,
                                                  context->depth,
                                                  palette,
                                                  context->reqcolor,
                                                  context->indextable,
                                                  context->complexion);
                }
            } else if (use_palette_float_lookup) {
                color_index = sixel_dither_lookup_palette_float32(
                    lookup_pixel_float,
                    context->depth,
                    palette_float,
                    context->reqcolor,
                    context->complexion);
            } else {
                lookup_pixel = quantized;
                if (use_fast_lut) {
                    color_index = sixel_lut_map_pixel(fast_lut,
                                                     lookup_pixel);
                } else {
                    color_index = context->lookup(lookup_pixel,
                                                  context->depth,
                                                  palette,
                                                  context->reqcolor,
                                                  context->indextable,
                                                  context->complexion);
                }
            }

            if (context->optimize_palette) {
                if (context->migration_map[color_index] == 0) {
                    output_index = *context->ncolors;
                    for (n = 0; n < context->depth; ++n) {
                        context->new_palette[output_index * context->depth + n]
                            = palette[color_index * context->depth + n];
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
                    ++*context->ncolors;
                    context->migration_map[color_index] = *context->ncolors;
                } else {
                    output_index = context->migration_map[color_index] - 1;
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

            for (n = 0; n < context->depth; ++n) {
                if (context->optimize_palette) {
                    if (have_new_palette_float) {
                        palette_value_float =
                            new_palette_float[output_index * float_depth
                                              + n];
                    } else {
                        palette_value =
                            context->new_palette[output_index
                                                 * context->depth + n];
                        palette_value_float
                            = sixel_pixelformat_byte_to_float(
                                  context->pixelformat,
                                  n,
                                  (unsigned char)palette_value);
                    }
                } else {
                    if (have_palette_float) {
                        palette_value_float =
                            palette_float[color_index * float_depth + n];
                    } else {
                        palette_value =
                            palette[color_index * context->depth + n];
                        palette_value_float
                            = sixel_pixelformat_byte_to_float(
                                  context->pixelformat,
                                  n,
                                  (unsigned char)palette_value);
                    }
                }
                error = snapshot[n] - palette_value_float;
                source_pixel[n] = palette_value_float;
                f_diffuse(data + (size_t)n,
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

    if (context->optimize_palette) {
        memcpy(context->palette,
               context->new_palette,
               (size_t)(*context->ncolors * context->depth));
        if (palette_float != NULL
                && new_palette_float != NULL
                && float_depth > 0) {
            memcpy(palette_float,
                   new_palette_float,
                   (size_t)(*context->ncolors * float_depth)
                       * sizeof(float));
        }
    }

    status = SIXEL_OK;
    SIXEL_DITHER_FLOAT32_HIT();
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
