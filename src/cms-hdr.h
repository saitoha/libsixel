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

#ifndef LIBSIXEL_CMS_HDR_H
#define LIBSIXEL_CMS_HDR_H
#include <stddef.h>
#include <sixel.h>

/* H.273 transfer codes, independent of ICC rendering intent. */
enum {
    SIXEL_CMS_HDR_PQ = 16,
    SIXEL_CMS_HDR_HLG = 18
};

typedef struct sixel_cms_hdr {
    int transfer;
    /* 0: sRGB/BT.709, 1: Display P3, 2: BT.2020. All use D65. */
    int primaries;
    double reference_white;
    double peak_luminance;
} sixel_cms_hdr_t;

/* Decode normalized RGB to linear sRGB, with one unit equal to the
 * reference white in cd/m2. HLG uses the reference-display OOTF with
 * peak luminance in [400,10000] cd/m2 and zero display black. No tone
 * mapping, gamut clipping, alpha operation or ICC adaptation occurs here.
 * Invalid arguments leave output unchanged, including exact aliases. */
SIXEL_INTERNAL_API int
sixel_cms_hdr_to_linear(double output[3], double const input[3],
                         sixel_cms_hdr_t const *hdr);

/* Stage the complete float buffer before publication. */
SIXEL_INTERNAL_API int
sixel_cms_hdr_convert(float *pixels, size_t count,
                       sixel_cms_hdr_t const *hdr);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
