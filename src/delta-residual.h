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

#ifndef LIBSIXEL_DELTA_RESIDUAL_H
#define LIBSIXEL_DELTA_RESIDUAL_H

#include <sixel.h>
#include <stddef.h>
#include <stdint.h>

enum sixel_delta_metric {
    SIXEL_DELTA_METRIC_AUTO = 0,
    SIXEL_DELTA_METRIC_MAX = 1,
    SIXEL_DELTA_METRIC_RGB241 = 2
};

/* A fixed-size screen grid bounds analysis and CPU-to-GPU transfer cost.
 * Cell edges are floor(i * extent / grid). The inverse mapping must match
 * these edges, including when the extent is not divisible by the grid. */
typedef struct sixel_delta_mask {
    uint32_t cells[128];
    unsigned int columns;
    unsigned int rows;
    int width;
    int height;
    unsigned int threshold;
} sixel_delta_mask_t;

typedef struct sixel_delta_residual {
    unsigned char age[4096];
    unsigned char next_age[4096];
    sixel_delta_mask_t mask;
    int width;
    int height;
    int pending;
    size_t selected_pixels;
} sixel_delta_residual_t;

/* Prepare from immutable target/display colors. This never commits age;
 * callers publish next_age only after the corresponding output succeeds. */
SIXEL_INTERNAL_API void
sixel_delta_residual_prepare(sixel_delta_residual_t *state,
    unsigned char const *target, unsigned char const *acc,
    unsigned char const *valid, int width, int height,
    double threshold, unsigned int frames, unsigned int keep_threshold);

SIXEL_INTERNAL_API unsigned int
sixel_delta_mask_threshold(sixel_delta_mask_t const *mask,
    int x, int y, unsigned int threshold);

SIXEL_INTERNAL_API int
sixel_delta_rgb241_within(unsigned char const *a, unsigned char const *b,
    unsigned int threshold);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
