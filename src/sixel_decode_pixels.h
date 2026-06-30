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

/*
 * Shared SIXEL memory decoding helper for packed pixel output.
 */
#ifndef SIXEL_DECODE_PIXELS_H
#define SIXEL_DECODE_PIXELS_H

#include <stddef.h>

#include <sixel.h>

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_raw_with_options(unsigned char *p,
                              int len,
                              unsigned int decode_flags,
                              unsigned char **pixels,
                              int *pwidth,
                              int *pheight,
                              unsigned char **palette,
                              int *ncolors,
                              unsigned int *result_flags,
                              sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_raw_with_options_mask(unsigned char *p,
                                   int len,
                                   unsigned int decode_flags,
                                   unsigned char **pixels,
                                   unsigned char **paint_mask,
                                   int *pwidth,
                                   int *pheight,
                                   unsigned char **palette,
                                   int *ncolors,
                                   unsigned int *result_flags,
                                   sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_direct_with_options(unsigned char *p,
                                 int len,
                                 unsigned int decode_flags,
                                 unsigned char **pixels,
                                 int *pwidth,
                                 int *pheight,
                                 unsigned int *result_flags,
                                 sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_kundither_fast4_with_options(unsigned char *p,
                                          int len,
                                          int direct_output,
                                          int similarity_bias,
                                          unsigned int decode_flags,
                                          unsigned int *result_flags,
                                          unsigned char **pixels,
                                          int *pwidth,
                                          int *pheight,
                                          sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_pixels_finish_rgba(unsigned char **decoded,
                                int width,
                                int height,
                                int pixelformat,
                                unsigned char const *bg,
                                unsigned int result_flags,
                                sixel_decode_result_t *result,
                                sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decode_pixels(unsigned char const *data,
                    size_t size,
                    sixel_decode_options_t const *options,
                    sixel_decode_result_t *result,
                    sixel_allocator_t *allocator);

#endif /* SIXEL_DECODE_PIXELS_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
