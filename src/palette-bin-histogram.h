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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_PALETTE_BIN_HISTOGRAM_H
#define LIBSIXEL_PALETTE_BIN_HISTOGRAM_H

#include <stdint.h>

#include <sixel.h>

#include "palette-plan.h"

typedef struct sixel_palette_bin_entry {
    uint32_t key;
    double weight;
    double sum[3];
} sixel_palette_bin_entry_t;

typedef struct sixel_palette_bin_histogram {
    sixel_palette_bin_entry_t *entries;
    unsigned int capacity;
    unsigned int mask;
    unsigned int size;
    unsigned int bits_per_axis;
    unsigned int bin_count;
} sixel_palette_bin_histogram_t;

SIXEL_INTERNAL_API void
sixel_palette_bin_histogram_clear(
    sixel_palette_bin_histogram_t *histogram);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_histogram_init(
    sixel_palette_bin_histogram_t *histogram,
    size_t expected_entries,
    unsigned int bits_per_axis,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_palette_bin_histogram_dispose(
    sixel_palette_bin_histogram_t *histogram,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API double
sixel_palette_bin_map_sample_to_unit(
    double sample,
    int input_is_float32,
    unsigned int channel,
    double const *scale,
    double const *offset,
    sixel_palette_binning_grid_map_t grid_map);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_histogram_add_hard(
    sixel_palette_bin_histogram_t *histogram,
    double const sample[3],
    double const mapped[3],
    double weight,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_histogram_add_soft_trilinear(
    sixel_palette_bin_histogram_t *histogram,
    double const sample[3],
    double const mapped[3],
    double weight,
    sixel_allocator_t *allocator);

#endif /* LIBSIXEL_PALETTE_BIN_HISTOGRAM_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
