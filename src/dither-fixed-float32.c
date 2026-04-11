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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#include <stdint.h>
#include <string.h>

#include "compat_stub.h"
#include "dither-fixed-float32.h"
#include "dither-interframe-method.h"
#include "dither-common-pipeline.h"
#include "pixelformat.h"
#include "lookup-common.h"

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

typedef SIXELSTATUS (*sixel_interframe_prepare_frame_float32_fn)(
    sixel_dither_t *dither,
    int width,
    int height,
    int depth,
    int can_update,
    int *enabled,
    int32_t **frame);

typedef void (*sixel_interframe_load_pixel_float32_fn)(
    sixel_dither_t *dither,
    float const *source_pixel,
    size_t base,
    int x,
    int y,
    int depth,
    int pixelformat,
    int32_t const *frame,
    float working_float[SIXEL_MAX_CHANNELS],
    unsigned char corrected[SIXEL_MAX_CHANNELS]);

typedef void (*sixel_interframe_clear_pixel_float32_fn)(
    int32_t *frame,
    size_t base,
    int depth,
    int can_update);

typedef void (*sixel_interframe_store_error_float32_fn)(
    int32_t *frame,
    size_t base,
    int channel,
    int offset,
    int can_update);

typedef struct sixel_interframe_method_float32_ops {
    int method_id;
    sixel_interframe_prepare_frame_float32_fn prepare_frame;
    sixel_interframe_load_pixel_float32_fn load_pixel;
    sixel_interframe_clear_pixel_float32_fn clear_pixel;
    sixel_interframe_store_error_float32_fn store_error;
} sixel_interframe_method_float32_ops_t;

typedef sixel_interframe_stbn_state_common_t
    sixel_interframe_stbn_state_float32_t;

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

static void
sixel_interframe_clear_pixel_float32(int32_t *frame,
                                   size_t base,
                                   int depth,
                                   int can_update)
{
    int n;

    n = 0;
    if (frame == NULL || can_update == 0) {
        return;
    }
    for (n = 0; n < depth; ++n) {
        frame[base + (size_t)n] = 0;
    }
}

static void
sixel_interframe_store_error_float32(int32_t *frame,
                                   size_t base,
                                   int channel,
                                   int offset,
                                   int can_update)
{
    int32_t scaled;

    scaled = 0;
    if (frame == NULL || can_update == 0) {
        return;
    }

    /*
     * Multiplication keeps signed offsets defined on every compiler.
     */
    scaled = (int32_t)(offset * SIXEL_INTERFRAME_VARERR_SCALE);
    frame[base + (size_t)channel] = scaled;
}

static SIXELSTATUS
sixel_interframe_diffusion_prepare_frame_float32(sixel_dither_t *dither,
                                               int width,
                                               int height,
                                               int depth,
                                               int can_update,
                                               int *enabled,
                                               int32_t **frame)
{
    return sixel_interframe_prepare_shared_frame(
        dither,
        width,
        height,
        depth,
        can_update,
        SIXEL_INTERFRAME_METHOD_DIFFUSION,
        enabled,
        frame);
}

static void
sixel_interframe_diffusion_load_pixel_float32(
    sixel_dither_t *dither,
    float const *source_pixel,
    size_t base,
    int x,
    int y,
    int depth,
    int pixelformat,
    int32_t const *frame,
    float working_float[SIXEL_MAX_CHANNELS],
    unsigned char corrected[SIXEL_MAX_CHANNELS])
{
    int n;
    float interframe_delta;
    float interframe_corrected;

    (void)dither;
    (void)x;
    (void)y;

    n = 0;
    interframe_delta = 0.0f;
    interframe_corrected = 0.0f;

    for (n = 0; n < depth; ++n) {
        if (frame != NULL) {
            interframe_delta = (float)frame[base + (size_t)n]
                / (float)SIXEL_INTERFRAME_VARERR_SCALE
                / 255.0f;
        } else {
            interframe_delta = 0.0f;
        }
        interframe_corrected = source_pixel[n] + interframe_delta;
        interframe_corrected = sixel_pixelformat_float_channel_clamp(
            pixelformat,
            n,
            interframe_corrected);
        working_float[n] = interframe_corrected;
        corrected[n] = (unsigned char)sixel_pixelformat_float_channel_to_byte(
            pixelformat,
            n,
            interframe_corrected);
    }
}

static int
sixel_interframe_stbn_bias_u8_sampled_float32(
    sixel_interframe_stbn_sample_u16_fn sample_fn,
    uint32_t sequence_index,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    /*
     * Float32 keeps STBN active for both hash and mask backends in v1.
     */
    sample_value = sample_fn(sequence_index, x, y, channel, depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);

    return (int)bias_u8;
}

static int
sixel_interframe_stbn_bias_u8_sampled_row_cached_float32(
    sixel_interframe_stbn_state_float32_t *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sixel_interframe_stbn_source_pmj_sample_u16_row_cached_common(
        stbn_state,
        x,
        y,
        channel,
        depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);

    return (int)bias_u8;
}

static int
sixel_interframe_stbn_bias_u8_sampled_tiled_float32(
    sixel_interframe_stbn_state_float32_t const *stbn_state,
    int x,
    int y,
    int channel,
    int depth,
    int strength_u8)
{
    uint16_t sample_value;
    int32_t bias_u8;

    sample_value = 0U;
    bias_u8 = 0;

    sample_value = sixel_interframe_stbn_source_pmj_sample_u16_tiled_common(
        stbn_state,
        x,
        y,
        channel,
        depth);
    bias_u8 = sixel_interframe_stbn_bias_u8_from_sample_u16_inline_common(
        sample_value,
        strength_u8);

    return (int)bias_u8;
}

static int
sixel_interframe_stbn_prepare_pmj_float_lut_float32(
    sixel_interframe_stbn_state_float32_t *stbn_state,
    int pixelformat,
    int depth)
{
    int channel;
    int value;

    channel = 0;
    value = 0;

    if (stbn_state == NULL || depth <= 0 || depth > SIXEL_MAX_CHANNELS) {
        return 0;
    }
    if (stbn_state->pmj_float_lut_valid != 0
            && stbn_state->pmj_float_lut_pixelformat == pixelformat
            && stbn_state->pmj_float_lut_depth == depth) {
        return 1;
    }

    for (channel = 0; channel < depth; ++channel) {
        for (value = 0; value < SIXEL_INTERFRAME_FLOAT_LUT_SIZE; ++value) {
            stbn_state->pmj_float_u8_to_float[channel][value]
                = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    channel,
                    (unsigned char)value);
        }
    }

    stbn_state->pmj_float_lut_pixelformat = pixelformat;
    stbn_state->pmj_float_lut_depth = depth;
    stbn_state->pmj_float_lut_valid = 1;
    return 1;
}

static SIXELSTATUS
sixel_interframe_stbn_prepare_frame_float32(sixel_dither_t *dither,
                                          int width,
                                          int height,
                                          int depth,
                                          int can_update,
                                          int *enabled,
                                          int32_t **frame)
{
    SIXELSTATUS status;
    sixel_interframe_stbn_state_float32_t *stbn_state;
    int strategy_token;

    status = SIXEL_OK;
    stbn_state = NULL;
    strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;

    status = sixel_interframe_prepare_shared_frame(
        dither,
        width,
        height,
        depth,
        can_update,
        SIXEL_INTERFRAME_METHOD_STBN,
        enabled,
        frame);
    if (status != SIXEL_OK || *enabled == 0) {
        return status;
    }

    strategy_token = sixel_interframe_strategy_token_from_dither_or_env_common(
        dither);
    status = sixel_interframe_prepare_stbn_state_common(
        dither,
        can_update,
        strategy_token,
        sizeof(sixel_interframe_stbn_state_float32_t),
        (void **)&stbn_state);
    if (status != SIXEL_OK || stbn_state == NULL || can_update == 0) {
        return status;
    }

    return status;
}

static void
sixel_interframe_stbn_load_pixel_float32(
    sixel_dither_t *dither,
    float const *source_pixel,
    size_t base,
    int x,
    int y,
    int depth,
    int pixelformat,
    int32_t const *frame,
    float working_float[SIXEL_MAX_CHANNELS],
    unsigned char corrected[SIXEL_MAX_CHANNELS])
{
    sixel_interframe_stbn_state_float32_t *stbn_state;
    int n;
    int bias_u8;
    int adjusted_u8;
    sixel_interframe_stbn_sample_u16_fn sample_u16;
    uint32_t sequence_index;
    int use_pmj_row_cached;
    int use_pmj_tiled;
    int use_pmj_float_lut;
    int stbn_strength_u8;

    stbn_state = NULL;
    n = 0;
    bias_u8 = 0;
    adjusted_u8 = 0;
    sample_u16 = sixel_interframe_stbn_sample_hash_u16_common;
    sequence_index = 0U;
    use_pmj_row_cached = 0;
    use_pmj_tiled = 0;
    use_pmj_float_lut = 0;
    stbn_strength_u8 = SIXEL_INTERFRAME_STBN_V1_STRENGTH_U8;

    stbn_state = (sixel_interframe_stbn_state_float32_t *)
        sixel_interframe_get_method_private(
            dither,
            SIXEL_INTERFRAME_METHOD_STBN,
            sizeof(sixel_interframe_stbn_state_float32_t));

    sixel_interframe_diffusion_load_pixel_float32(
        dither,
        source_pixel,
        base,
        x,
        y,
        depth,
        pixelformat,
        frame,
        working_float,
        corrected);

    /*
     * Cache resolved STBN sample source per pixel so per-channel bias
     * uses only the backend sample function call.
     */
    if (stbn_state != NULL) {
        sequence_index = stbn_state->sequence_index;
        stbn_strength_u8 = stbn_state->stbn_strength_u8;
        if (stbn_state->sample_u16 != NULL) {
            sample_u16 = stbn_state->sample_u16;
        }
        if (stbn_state->sample_source_id == SIXEL_INTERFRAME_STBN_SOURCE_PMJ) {
            use_pmj_row_cached = 1;
            if (stbn_state->pmj_tile_enabled != 0) {
                use_pmj_tiled = 1;
            }
            use_pmj_float_lut
                = sixel_interframe_stbn_prepare_pmj_float_lut_float32(
                      stbn_state,
                      pixelformat,
                      depth);
        }
    }

    if (stbn_strength_u8 <= 0) {
        return;
    }

    if (use_pmj_tiled != 0) {
        for (n = 0; n < depth; ++n) {
            bias_u8 = sixel_interframe_stbn_bias_u8_sampled_tiled_float32(
                stbn_state,
                x,
                y,
                n,
                depth,
                stbn_strength_u8);
            if (bias_u8 == 0) {
                continue;
            }

            adjusted_u8 = (int)corrected[n] + bias_u8;
            if (adjusted_u8 < 0) {
                adjusted_u8 = 0;
            } else if (adjusted_u8 > 255) {
                adjusted_u8 = 255;
            }
            corrected[n] = (unsigned char)adjusted_u8;
            if (use_pmj_float_lut != 0) {
                working_float[n] = stbn_state->pmj_float_u8_to_float[
                    n][corrected[n]];
            } else {
                working_float[n] = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    n,
                    corrected[n]);
            }
        }
        return;
    }

    if (use_pmj_row_cached != 0) {
        for (n = 0; n < depth; ++n) {
            bias_u8 = sixel_interframe_stbn_bias_u8_sampled_row_cached_float32(
                stbn_state,
                x,
                y,
                n,
                depth,
                stbn_strength_u8);
            if (bias_u8 == 0) {
                continue;
            }

            adjusted_u8 = (int)corrected[n] + bias_u8;
            if (adjusted_u8 < 0) {
                adjusted_u8 = 0;
            } else if (adjusted_u8 > 255) {
                adjusted_u8 = 255;
            }
            corrected[n] = (unsigned char)adjusted_u8;
            if (use_pmj_float_lut != 0) {
                working_float[n] = stbn_state->pmj_float_u8_to_float[
                    n][corrected[n]];
            } else {
                working_float[n] = sixel_pixelformat_byte_to_float(
                    pixelformat,
                    n,
                    corrected[n]);
            }
        }
        return;
    }

    for (n = 0; n < depth; ++n) {
        bias_u8 = sixel_interframe_stbn_bias_u8_sampled_float32(
            sample_u16,
            sequence_index,
            x,
            y,
            n,
            depth,
            stbn_strength_u8);
        if (bias_u8 == 0) {
            continue;
        }

        adjusted_u8 = (int)corrected[n] + bias_u8;
        if (adjusted_u8 < 0) {
            adjusted_u8 = 0;
        } else if (adjusted_u8 > 255) {
            adjusted_u8 = 255;
        }
        corrected[n] = (unsigned char)adjusted_u8;
        working_float[n] = sixel_pixelformat_byte_to_float(
            pixelformat,
            n,
            corrected[n]);
    }
}

static sixel_interframe_method_float32_ops_t const
sixel_interframe_diffusion_ops_float32 = {
    SIXEL_INTERFRAME_METHOD_DIFFUSION,
    sixel_interframe_diffusion_prepare_frame_float32,
    sixel_interframe_diffusion_load_pixel_float32,
    sixel_interframe_clear_pixel_float32,
    sixel_interframe_store_error_float32
};

static sixel_interframe_method_float32_ops_t const
sixel_interframe_stbn_ops_float32 = {
    SIXEL_INTERFRAME_METHOD_STBN,
    sixel_interframe_stbn_prepare_frame_float32,
    sixel_interframe_stbn_load_pixel_float32,
    sixel_interframe_clear_pixel_float32,
    sixel_interframe_store_error_float32
};

static sixel_interframe_method_float32_ops_t const *
sixel_interframe_method_ops_float32_for_id(int method_id)
{
    switch (method_id) {
    case SIXEL_INTERFRAME_METHOD_DIFFUSION:
        return &sixel_interframe_diffusion_ops_float32;
    case SIXEL_INTERFRAME_METHOD_STBN:
        return &sixel_interframe_stbn_ops_float32;
    default:
        break;
    }

    return NULL;
}

SIXELSTATUS
sixel_dither_apply_fixed_float32(sixel_dither_t *dither,
                                 sixel_dither_context_t *context)
{
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
    unsigned char quantized[SIXEL_MAX_CHANNELS];
    unsigned char corrected[SIXEL_MAX_CHANNELS];
    float working_float[SIXEL_MAX_CHANNELS];
    float lookup_pixel_float[SIXEL_MAX_CHANNELS];
    int color_index;
    int output_index;
    unsigned char palette_value_u8;
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
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int use_transparent_fence;
    int is_transparent;
    size_t absolute_index;
    int interframe_method;
    int interframe_enabled;
    int interframe_can_update;
    int32_t *interframe_error;
    sixel_interframe_method_float32_ops_t const *interframe_ops;
    int strategy_token;
    int interframe_spatial_diffuse;

    palette_float = NULL;
    new_palette_float = NULL;
    float_depth = 0;
    interframe_method = SIXEL_INTERFRAME_METHOD_NONE;
    interframe_enabled = 0;
    interframe_can_update = 0;
    interframe_error = NULL;
    interframe_ops = NULL;
    strategy_token = SIXEL_INTERFRAME_STRATEGY_TOKEN_NONE;
    interframe_spatial_diffuse = SIXEL_DIFFUSE_FS;

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
    if (context->depth > SIXEL_MAX_CHANNELS || context->depth != 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (context->reqcolor < 1) {
        return SIXEL_BAD_ARGUMENT;
    }

    fast_lut = context->lut;
    use_fast_lut = (fast_lut != NULL);
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
    if (method_for_diffuse == SIXEL_DIFFUSE_INTERFRAME) {
        /*
         * Keep the interframe strategy token and spatial kernel independent
         * so callers can choose kernels with
         * -d interframe:diffusion=... or -d stbn:diffusion=....
         */
        strategy_token =
            sixel_interframe_strategy_token_from_dither_or_env_common(dither);
        interframe_method = sixel_interframe_method_from_diffuse_and_token(
            method_for_diffuse,
            strategy_token);
        interframe_ops = sixel_interframe_method_ops_float32_for_id(
            interframe_method);
        if (interframe_ops != NULL) {
            interframe_enabled = 1;
        }
        interframe_spatial_diffuse =
            sixel_interframe_spatial_diffuse_from_dither_or_env_common(
                dither);
        method_for_diffuse = interframe_spatial_diffuse;
    }
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
    case SIXEL_DIFFUSE_INTERFRAME:
    default:
        f_diffuse = diffuse_fs_float;
        break;
    }

    if (interframe_enabled) {
        if (interframe_ops == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        interframe_can_update = dither->interframe_state.last_apply_consumed;
        status = interframe_ops->prepare_frame(
            dither,
            context->width,
            context->height,
            context->depth,
            interframe_can_update,
            &interframe_enabled,
            &interframe_error);
        if (SIXEL_FAILED(status)) {
            return status;
        }
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
        sixel_dither_scanline_params_fixed_float32(serpentine, absolute_y,
                                     context->width,
                                     &start, &end, &step, &direction);
        for (x = start; x != end; x += step) {
            pos = y * context->width + x;
            base = (size_t)pos * (size_t)context->depth;
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
                if (interframe_enabled && interframe_ops != NULL) {
                    interframe_ops->clear_pixel(interframe_error,
                                              base,
                                              context->depth,
                                              interframe_can_update);
                }
                continue;
            }
            source_pixel = data + base;

            /*
             * Keep the dereference guarded even after the early interframe
             * setup checks so GCC -fanalyzer can prove this path is safe.
             */
            if (interframe_enabled && interframe_ops != NULL) {
                interframe_ops->load_pixel(dither,
                                         source_pixel,
                                         base,
                                         x,
                                         absolute_y,
                                         context->depth,
                                         context->pixelformat,
                                         interframe_error,
                                         working_float,
                                         corrected);
                for (n = 0; n < context->depth; ++n) {
                    quantized[n] = corrected[n];
                    if (need_float_pixel) {
                        lookup_pixel_float[n] = working_float[n];
                    }
                }
            } else {
                for (n = 0; n < context->depth; ++n) {
                    working_float[n] = source_pixel[n];
                    if (!lookup_wants_float
                            && !use_palette_float_lookup) {
                        quantized[n]
                            = sixel_pixelformat_float_channel_to_byte(
                                context->pixelformat,
                                n,
                                source_pixel[n]);
                    }
                    if (need_float_pixel) {
                        lookup_pixel_float[n] = working_float[n];
                    }
                }
            }

            if (lookup_wants_float) {
                lookup_pixel = (unsigned char const *)(void const *)
                    working_float;
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
                            context->new_palette[
                                output_index * context->depth + n]
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
                    /*
                     * The palette count never exceeds SIXEL_PALETTE_MAX (256),
                     * so storing it in an unsigned short is safe.
                     */
                    context->migration_map[color_index]
                        = (unsigned short)(*context->ncolors);
                } else {
                    output_index = context->migration_map[color_index] - 1;
                }
                if (absolute_y >= context->output_start) {
                    /*
                     * Palette indices are bounded by SIXEL_PALETTE_MAX, which
                     * fits in sixel_index_t (unsigned char).
                     */
                    context->result[pos] = (sixel_index_t)output_index;
                }
            } else {
                output_index = color_index;
                if (absolute_y >= context->output_start) {
                    context->result[pos] = (sixel_index_t)output_index;
                }
            }

            for (n = 0; n < context->depth; ++n) {
                if (context->optimize_palette) {
                    palette_value_u8 = context->new_palette[
                        output_index * context->depth + n];
                    if (have_new_palette_float) {
                        palette_value_float =
                            new_palette_float[output_index * float_depth
                                              + n];
                    } else {
                        palette_value_float
                            = sixel_pixelformat_byte_to_float(
                                  context->pixelformat,
                                  n,
                                  palette_value_u8);
                    }
                } else {
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
                }
                error = working_float[n] - palette_value_float;
                if (interframe_enabled && interframe_ops != NULL) {
                    interframe_ops->store_error(
                        interframe_error,
                        base,
                        n,
                        (int)corrected[n] - (int)palette_value_u8,
                        interframe_can_update);
                }
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
