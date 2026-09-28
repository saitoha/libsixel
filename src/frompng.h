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

#ifndef LIBSIXEL_FROMPNG_H
#define LIBSIXEL_FROMPNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"

/* Inspect a unique, CRC-valid cICP before PLTE/IDAT. The returned static
 * name is a supported shared CMS definition, or NULL for ignored metadata. */
char const *
sixel_frompng_cicp_name(unsigned char const *buffer, size_t size);

int
sixel_frompng_has_color_work(unsigned char const *buffer, size_t size);

void
sixel_frompng_apply_colorspace_fallback(unsigned char *pixels,
                                        int width,
                                        int height,
                                        unsigned char const *buffer,
                                        size_t size,
                                        sixel_allocator_t *allocator);

int
sixel_frompng_apply_colorspace_fallback_pixelformat(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *buffer,
    size_t size,
    sixel_allocator_t *allocator);

SIXELSTATUS
sixel_frompng_load_nonindexed(sixel_chunk_t const *pchunk,
                              sixel_allocator_t *allocator,
                              sixel_frame_t *frame,
                              int enable_cms,
                              unsigned char const *bgcolor,
                              int bgcolor_source);

/* Animation rectangles retain straight coverage for later canvas blending.
 * Background interpretation is returned separately, never baked into RGB. */
typedef struct sixel_frompng_composition {
    float background[3];
    int has_background;
    int managed;
} sixel_frompng_composition_t;

SIXELSTATUS
sixel_frompng_load_rect(sixel_chunk_t const *pchunk,
                        sixel_allocator_t *allocator, sixel_frame_t *frame,
                        int enable_cms, unsigned char const *bgcolor,
                        int bgcolor_source,
                        sixel_frompng_composition_t *composition);

#ifdef __cplusplus
}
#endif

#endif  /* LIBSIXEL_FROMPNG_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
