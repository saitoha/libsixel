/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FROMTGA_H
#define LIBSIXEL_FROMTGA_H

#include <sixel.h>

/* Borrowed metadata only: raster decoding and CMS remain separate stages.
 * The caller keeps the input buffer alive through correction/normalization. */
typedef struct sixel_tga_color {
    unsigned char const *table;
    size_t raster_limit;
    unsigned int gamma_num;
    unsigned int gamma_den;
    unsigned int alpha_type;
} sixel_tga_color_t;

/* FALSE means no color definition. Malformed declared metadata is an error.
 * A selected table replaces the scalar gamma hint, rather than stacking it. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_tga_color_read(unsigned char const *data, size_t size,
                     sixel_tga_color_t *color);

/* ARGB table words are normalized directly, without an eight-bit boundary.
 * Channel 0 is alpha; channels 1..3 are RGB, indexed independently. */
SIXEL_INTERNAL_API float
sixel_tga_color_sample(sixel_tga_color_t const *color,
                       unsigned int channel, unsigned int sample);

#endif /* LIBSIXEL_FROMTGA_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
