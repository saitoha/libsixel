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

#ifndef LIBSIXEL_LOOKUP_BLOCKCULL_FLOAT32_H
#define LIBSIXEL_LOOKUP_BLOCKCULL_FLOAT32_H

#include "allocator.h"

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_lookup_blockcull_float32
    sixel_lookup_blockcull_float32_t;

/* Copies the palette and weights; palette entries are three floats. */
SIXELSTATUS
sixel_lookup_blockcull_float32_create(
    sixel_allocator_t *allocator,
    float const *palette,
    int ncolors,
    float const *weights,
    sixel_lookup_blockcull_float32_t **lookup_out);

void
sixel_lookup_blockcull_float32_destroy(
    sixel_lookup_blockcull_float32_t *lookup);

int
sixel_lookup_blockcull_float32_map(
    sixel_lookup_blockcull_float32_t const *lookup,
    float const *pixel);

void
sixel_lookup_blockcull_float32_map_pixels(
    sixel_lookup_blockcull_float32_t const *lookup,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_BLOCKCULL_FLOAT32_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
