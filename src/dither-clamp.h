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

#ifndef LIBSIXEL_DITHER_CLAMP_H
#define LIBSIXEL_DITHER_CLAMP_H

#include <sixel.h>
#include "pixelformat.h"

/* Keep a small correction margin at nominal working-space boundaries. */
#define SIXEL_DITHER_CLAMP_THRESHOLD 1.01

/* Hard/soft float requests own an immutable source snapshot; NONE does not
 * allocate one. Work bands never share writable limiter state, including
 * during overlap warm-up. */
typedef struct sixel_dither_clamp {
    float *source;
    size_t samples;
    int depth;
    int policy;
    double threshold;
    double minimum[SIXEL_MAX_CHANNELS];
    double maximum[SIXEL_MAX_CHANNELS];
    /* Nominal bounds less the policy margin.  They lie inside every
     * source-containing bound, so a corrected sample within them is left
     * unchanged; valid only when every snapshot sample is finite. */
    double inner_low[SIXEL_MAX_CHANNELS];
    double inner_high[SIXEL_MAX_CHANNELS];
    int source_finite;
    sixel_allocator_t *allocator;
} sixel_dither_clamp_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_clamp_init(sixel_dither_clamp_t *clamp,
                        float const *pixels, int width, int height,
                        int depth, int pixelformat, int policy,
                        double threshold, sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_dither_clamp_finish(sixel_dither_clamp_t *clamp);

/* Bound corrected coordinates. An unchanged source is an exact fixed
 * point, even for negative or over-range color coordinates. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_clamp_pixel(sixel_dither_clamp_t const *clamp,
                         size_t pixel, float *corrected);

#endif /* LIBSIXEL_DITHER_CLAMP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
