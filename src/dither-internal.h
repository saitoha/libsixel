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

/*
 * Internal dithering helpers shared between the 8bit and float32
 * implementations.  The header exposes the context descriptor passed to the
 * per-algorithm workers so each backend can access the relevant buffers
 * without expanding their public signatures.
 */
#ifndef LIBSIXEL_DITHER_INTERNAL_H
#define LIBSIXEL_DITHER_INTERNAL_H

#include <stddef.h>

#include "dither.h"

struct sixel_lookup_policy_interface;

typedef int (*sixel_dither_lookup_map_fn)(
    struct sixel_lookup_policy_interface const *policy,
    unsigned char const *pixel);

typedef struct sixel_dither_context {
    sixel_index_t *result;
    unsigned char *pixels;
    float *pixels_float;
    int width;
    int height;
    int band_origin;
    int output_start;
    int depth;
    unsigned char *palette;
    float *palette_float;
    int reqcolor;
    int method_for_diffuse;
    int method_for_scan;
    int optimize_palette;
    int complexion;
    struct sixel_lookup_policy_interface *lookup_policy;
    sixel_dither_lookup_map_fn lookup_map;
    unsigned char *scratch;
    unsigned char *new_palette;
    float *new_palette_float;
    unsigned short *migration_map;
    int *ncolors;
    int pixelformat;
    int float_depth;
    int lookup_source_is_float;
    int prefer_palette_float_lookup;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    unsigned char const *bluenoise_gradient_map;
    size_t bluenoise_gradient_map_size;
    int bluenoise_gradient_width;
    int bluenoise_gradient_height;
} sixel_dither_context_t;

#endif /* LIBSIXEL_DITHER_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
