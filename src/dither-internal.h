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

#include <limits.h>
#include <stddef.h>

#include "dither.h"
#include "lookup-common.h"

typedef enum sixel_dither_lookup_mode {
    SIXEL_DITHER_LOOKUP_MODE_NORMAL = 0,
    SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG = 1,
    SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG = 2
} sixel_dither_lookup_mode_t;

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
    sixel_dither_lookup_mode_t lookup_mode;
    struct sixel_lut *lut;
    unsigned short *indextable;
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

/*
 * Keep lookup dispatch in this header so every dithering backend can inline
 * the policy branch inside its hot pixel loops.
 */
static inline int
sixel_dither_lookup_normal_index(unsigned char const *pixel,
                                 int depth,
                                 unsigned char const *palette,
                                 int reqcolor,
                                 int complexion)
{
    int result;
    int diff;
    int r;
    int i;
    int n;
    int distant;

    result = (-1);
    diff = INT_MAX;
    for (i = 0; i < reqcolor; ++i) {
        distant = 0;
        r = pixel[0] - palette[i * depth + 0];
        distant += r * r * complexion;
        for (n = 1; n < depth; ++n) {
            r = pixel[n] - palette[i * depth + n];
            distant += r * r;
        }
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }
    return result;
}

static inline int
sixel_dither_lookup_mono_darkbg_index(unsigned char const *pixel,
                                      int depth,
                                      int reqcolor)
{
    int n;
    int distant;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant >= 128 * reqcolor ? 1 : 0;
}

static inline int
sixel_dither_lookup_mono_lightbg_index(unsigned char const *pixel,
                                       int depth,
                                       int reqcolor)
{
    int n;
    int distant;

    distant = 0;
    for (n = 0; n < depth; ++n) {
        distant += pixel[n];
    }
    return distant < 128 * reqcolor ? 1 : 0;
}

static inline int
sixel_dither_lookup_index(sixel_dither_context_t const *context,
                          unsigned char const *pixel)
{
    if (context->lut != NULL) {
        return sixel_lut_map_pixel(context->lut, pixel);
    }

    if (context->lookup_source_is_float != 0) {
        /*
         * Float-source lookup is valid only with LUT acceleration.
         * Keep the legacy fallback when LUT setup unexpectedly fails.
         */
        return 0;
    }

    switch (context->lookup_mode) {
    case SIXEL_DITHER_LOOKUP_MODE_MONO_DARKBG:
        return sixel_dither_lookup_mono_darkbg_index(pixel,
                                                     context->depth,
                                                     context->reqcolor);
    case SIXEL_DITHER_LOOKUP_MODE_MONO_LIGHTBG:
        return sixel_dither_lookup_mono_lightbg_index(pixel,
                                                      context->depth,
                                                      context->reqcolor);
    case SIXEL_DITHER_LOOKUP_MODE_NORMAL:
    default:
        return sixel_dither_lookup_normal_index(pixel,
                                                context->depth,
                                                context->palette,
                                                context->reqcolor,
                                                context->complexion);
    }
}

#endif /* LIBSIXEL_DITHER_INTERNAL_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
