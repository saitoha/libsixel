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

#ifndef LIBSIXEL_DECODER_H
#define LIBSIXEL_DECODER_H

#include <sixel.h>

#include "sixel_atomic.h"

/* encode settings object */
struct sixel_decoder {
    sixel_atomic_u32_t ref;
    char *input;
    char *output;
    int dequantize_method;
    int dequantize_similarity_bias;
    int dequantize_edge_strength;
    int thumbnail_size;
    int direct_color;
    sixel_allocator_t *allocator;
    int clipboard_input_active;
    int clipboard_output_active;
    char clipboard_input_format[32];
    char clipboard_output_format[32];
};

/*
 * Shared k_undither dequantizer used by decoder-side -d handling and tools
 * that need the same decoded target pixels without a PNG round trip.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dequantize_k_undither(unsigned char *indexed_pixels,
                            int width,
                            int height,
                            unsigned char *palette,
                            int ncolors,
                            int similarity_bias,
                            int edge_strength,
                            sixel_allocator_t *allocator,
                            unsigned char **output);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dequantize_k_undither_rgba(unsigned char *indexed_pixels,
                                 unsigned char const *paint_mask,
                                 int width,
                                 int height,
                                 unsigned char *palette,
                                 int ncolors,
                                 int similarity_bias,
                                 int edge_strength,
                                 sixel_allocator_t *allocator,
                                 unsigned char **output);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dequantize_k_undither_fast4(unsigned char *indexed_pixels,
                                  int width,
                                  int height,
                                  unsigned char *palette,
                                  int ncolors,
                                  int similarity_bias,
                                  sixel_allocator_t *allocator,
                                  unsigned char **output);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dequantize_k_undither_fast4_rgba(unsigned char *indexed_pixels,
                                       unsigned char const *paint_mask,
                                       int width,
                                       int height,
                                       unsigned char *palette,
                                       int ncolors,
                                       int similarity_bias,
                                       sixel_allocator_t *allocator,
                                       unsigned char **output);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dequantize_k_undither_fast4_rows(
    unsigned char const *indexed_pixels,
    int width,
    int height,
    unsigned char const *palette,
    int ncolors,
    int similarity_bias,
    int y_start,
    int y_end,
    sixel_allocator_t *allocator,
    unsigned char *rgb);


#endif /* LIBSIXEL_DECODER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
