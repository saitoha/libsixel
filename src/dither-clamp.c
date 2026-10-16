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
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
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
#endif
#include "compat_stub.h"
#include <stdint.h>
#include <string.h>
#include "dither-clamp.h"

SIXELSTATUS
sixel_dither_clamp_init(sixel_dither_clamp_t *clamp,
                        float const *pixels, int width, int height,
                        int depth, int pixelformat, int policy,
                        double threshold, sixel_allocator_t *allocator)
{
    size_t count;
    size_t index;
    double margin;
    int n;

    if (clamp == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(clamp, 0, sizeof(*clamp));
    if (pixels == NULL || width <= 0 || height <= 0 || depth <= 0 ||
            depth > SIXEL_MAX_CHANNELS || !isfinite(threshold) ||
            threshold < 1.0 || threshold > 16.0 ||
            policy < SIXEL_DITHER_CLAMP_AUTO ||
            policy > SIXEL_DITHER_CLAMP_NONE) {
        return SIXEL_BAD_ARGUMENT;
    }
    count = (size_t)width;
    if (count > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_ALLOCATION;
    }
    count *= (size_t)height;
    if (count > SIZE_MAX / (sizeof(float) * (size_t)depth)) {
        return SIXEL_BAD_ALLOCATION;
    }
    count *= (size_t)depth;
    clamp->samples = count;
    clamp->depth = depth;
    clamp->policy = policy == SIXEL_DITHER_CLAMP_AUTO
        ? SIXEL_DITHER_CLAMP_SOFT : policy;
    clamp->threshold = threshold;
    clamp->allocator = allocator;
    for (n = 0; n < depth; ++n) {
        clamp->minimum[n] = sixel_pixelformat_float_channel_min(
            pixelformat, n);
        clamp->maximum[n] = sixel_pixelformat_float_channel_max(
            pixelformat, n);
    }
    if (clamp->policy == SIXEL_DITHER_CLAMP_NONE) {
        return SIXEL_OK;
    }
    clamp->source = sixel_allocator_malloc(allocator, count * sizeof(float));
    if (clamp->source == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(clamp->source, pixels, count * sizeof(float));
    for (n = 0; n < depth; ++n) {
        margin = clamp->policy == SIXEL_DITHER_CLAMP_SOFT
            ? (clamp->threshold - 1.0)
                * (clamp->maximum[n] - clamp->minimum[n]) : 0.0;
        clamp->inner_low[n] = clamp->minimum[n] - margin;
        clamp->inner_high[n] = clamp->maximum[n] + margin;
    }
    clamp->source_finite = 1;
    for (index = 0; index < count; ++index) {
        if (!isfinite(clamp->source[index])) {
            clamp->source_finite = 0;
            break;
        }
    }
    return SIXEL_OK;
}

void
sixel_dither_clamp_finish(sixel_dither_clamp_t *clamp)
{
    if (clamp->source != NULL) {
        sixel_allocator_free(clamp->allocator, clamp->source);
        clamp->source = NULL;
    }
}

SIXELSTATUS
sixel_dither_clamp_pixel(sixel_dither_clamp_t const *clamp,
                         size_t pixel, float *corrected)
{
    double source[SIXEL_MAX_CHANNELS];
    double delta[SIXEL_MAX_CHANNELS];
    double lower;
    double upper;
    double margin;
    double ratio;
    double factor;
    size_t base;
    int n;

    if (clamp == NULL || corrected == NULL || clamp->depth <= 0 ||
            clamp->depth > SIXEL_MAX_CHANNELS ||
            pixel >= clamp->samples / (size_t)clamp->depth) {
        return SIXEL_BAD_ARGUMENT;
    }
    base = pixel * (size_t)clamp->depth;
    if (clamp->policy != SIXEL_DITHER_CLAMP_NONE && clamp->source_finite) {
        /*
         * Most corrected samples stay inside the nominal bounds less the
         * margin.  Those bounds lie inside lower and upper below for every
         * finite source, so neither policy would change such a sample; a
         * non-finite sample fails the comparison and takes the full path,
         * which reports it.
         */
        for (n = 0; n < clamp->depth; ++n) {
            if (!((double)corrected[n] >= clamp->inner_low[n]
                    && (double)corrected[n] <= clamp->inner_high[n])) {
                break;
            }
        }
        if (n == clamp->depth) {
            return SIXEL_OK;
        }
    }
    ratio = 1.0;
    for (n = 0; n < clamp->depth; ++n) {
        if (!isfinite(corrected[n])) {
            return SIXEL_BAD_INPUT;
        }
        if (clamp->policy == SIXEL_DITHER_CLAMP_NONE) {
            continue;
        }
        source[n] = clamp->source[base + (size_t)n];
        if (!isfinite(source[n])) {
            return SIXEL_BAD_INPUT;
        }
        /* Do the subtraction in double: two finite float32 coordinates
         * may have a difference outside the finite float32 range. */
        delta[n] = (double)corrected[n] - source[n];
        margin = clamp->policy == SIXEL_DITHER_CLAMP_SOFT
            ? (clamp->threshold - 1.0)
                * (clamp->maximum[n] - clamp->minimum[n]) : 0.0;
        lower = fmin(clamp->minimum[n], source[n]) - margin;
        upper = fmax(clamp->maximum[n], source[n]) + margin;
        if (clamp->policy == SIXEL_DITHER_CLAMP_HARD) {
            corrected[n] = (float)fmax(lower,
                                      fmin(upper, (double)corrected[n]));
        } else if (delta[n] > 0.0 && corrected[n] > upper) {
            factor = (upper - source[n]) / delta[n];
            ratio = fmin(ratio, factor);
        } else if (delta[n] < 0.0 && corrected[n] < lower) {
            factor = (lower - source[n]) / delta[n];
            ratio = fmin(ratio, factor);
        }
    }
    if (clamp->policy == SIXEL_DITHER_CLAMP_SOFT && ratio < 1.0) {
        /* One factor preserves the correction direction. Expanding
         * each boundary to include its original source keeps true extended
         * colors intact; this is not an input gamut mapping operation. */
        for (n = 0; n < clamp->depth; ++n) {
            corrected[n] = (float)(source[n] + ratio * delta[n]);
        }
    }
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
