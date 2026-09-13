/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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

#ifndef LIBSIXEL_COLORSPACE_H
#define LIBSIXEL_COLORSPACE_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

int
sixel_colorspace_supports_pixelformat(int pixelformat);

/* Analytic, sign-preserving TRCs for intermediate extended RGB values. */
SIXEL_INTERNAL_API double
sixel_color_decode_srgb(double value);

SIXEL_INTERNAL_API double
sixel_color_encode_srgb(double value);

/*
 * Convert one extended triplet in the existing working-space conventions.
 * Unlike the public storage converter, this internal path neither encodes
 * bytes nor restricts the RGB gamut. It accepts gamma, linear, Oklab,
 * CIELAB and DIN99d. Nonfinite/domain errors return 0 without changing out.
 * Exact aliasing is supported. The caller owns eventual float32 rounding.
 */
SIXEL_INTERNAL_API int
sixel_color_convert(double out[3], double const in[3],
                    int source, int target);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_COLORSPACE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
