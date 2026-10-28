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

#ifndef LIBSIXEL_GAMUT_H
#define LIBSIXEL_GAMUT_H

#include "cms.h"

enum {
    SIXEL_GAMUT_AUTO = 0,
    SIXEL_GAMUT_CLIP,
    SIXEL_GAMUT_LOCAL_MINDE,
    SIXEL_GAMUT_EDGE_SEEKER,
    SIXEL_GAMUT_RAY_TRACE
};

typedef struct sixel_gamut sixel_gamut_t;

/* An immutable, encoder-owned policy. Workers borrow it read-only; apply
 * never rounds to the wire grid or limits diffusion compensation. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gamut_new(sixel_gamut_t **out, sixel_color_def_t const *destination,
                 int legacy_colorspace, int policy,
                 sixel_allocator_t *allocator);

/* An owned copy for a palette request that can outlive its encoder. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gamut_clone(sixel_gamut_t const *source, sixel_gamut_t **out);

SIXEL_INTERNAL_API int
sixel_gamut_apply(sixel_gamut_t const *mapper, double const input[3],
                   double output[3], int colorspace);

/* Map an owned packed float plane in place. Hidden pixels are untouched.
 * Linear sources skip scalar dispatch and stores for in-gamut pixels.
 * A failing pixel is unchanged; earlier successful pixels may be mapped. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gamut_apply_float32(sixel_gamut_t const *mapper, float *pixels,
    size_t count, int colorspace, unsigned char const *hidden,
    size_t hidden_count);

SIXEL_INTERNAL_API void
sixel_gamut_free(sixel_gamut_t *mapper);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
