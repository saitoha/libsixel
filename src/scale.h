/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_SCALE_H
#define LIBSIXEL_SCALE_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

SIXEL_INTERNAL_API int
sixel_helper_scale_image_float32(
    float             /* out */ *dst,
    float const       /* in */  *src,
    int               /* in */  srcw,
    int               /* in */  srch,
    int               /* in */  pixelformat,
    int               /* in */  dstw,
    int               /* in */  dsth,
    int               /* in */  method_for_resampling,
    sixel_allocator_t /* in */  *allocator);

/* Extended color only; callers publish the destination after success. */
SIXEL_INTERNAL_API int
sixel_scale_image_rgbf32(
    float *dst,
    float const *src,
    int srcw,
    int srch,
    int pixelformat,
    int dstw,
    int dsth,
    int method_for_resampling,
    sixel_allocator_t *allocator);

/* The reader supplies exactly one unpacked RGB row per call. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_scale_bilinear_rgbf32_rows(float *dst, int srcw, int srch,
    int dstw, int dsth, void (*read_row)(void *, int, float *),
    void *source, sixel_allocator_t *allocator);

/* Interpolate at positions (1 << shift)*k + phase/2 in luma coordinates.
 * The integer weights represent a distance in half-luma-sample units. */
static inline void
sixel_scale_ycbcr_axis(int pixel, int phase, int shift, int count, int nearest,
                       int *first, int *second, float *weight)
{
    int pos, fraction, period;

    pos = 2 * pixel - phase;
    if (pos < 0) {
        pos = 0;
    }
    period = 2 << shift;
    fraction = pos & (period - 1);
    *first = pos >> (shift + 1);
    *second = *first + 1 < count ? *first + 1 : *first;
    if (nearest) {
        if (fraction > period / 2) {
            *first = *second;
        }
        *second = *first;
        *weight = 0.0f;
    } else {
        *weight = fraction * (shift == 1 ? 0.25f : 0.125f);
    }
}

/* Store the resized Y, Cb and Cr in triplets without clipping or rounding
 * to integer codes. The destination can then be converted to RGB in place. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_scale_bilinear_ycbcr420(float *dst, sixel_ycbcr_frame_view_t const *view,
    int dstw, int dsth, int xphase, int yphase, int nearest,
    sixel_allocator_t *allocator);

typedef struct sixel_scale_host_filter sixel_scale_host_filter_t;

sixel_scale_host_filter_t *
sixel_scale_host_filter_new(int sw, int sh, int dw, int dh, int channels,
    int xphase, int yphase, int nearest, sixel_allocator_t *allocator);

void
sixel_scale_host_filter_free(sixel_scale_host_filter_t *filter,
                            sixel_allocator_t *allocator);

/* Read one output row. At y == 0 start a fresh frame. The input is either
 * an 8-bit plane or a callback producing packed float RGB rows. */
float *
sixel_scale_host_filter_row(sixel_scale_host_filter_t *filter, int y,
    unsigned char const *row0, ptrdiff_t stride, int spacing,
    void (*read_row)(void *, int, float *), void *context);

SIXEL_INTERNAL_API size_t
sixel_scale_parallel_min_bytes(void);
SIXEL_INTERNAL_API int
sixel_scale_parallel_band_span(int rows, int threads);


#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_SCALE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
