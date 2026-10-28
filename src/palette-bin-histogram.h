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
#include "sample-statistics.h"
#include "sample-stream.h"
#include "weighted-point-set.h"

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

/* A frame-local refinement of the nominal hard grid. Each occupied interval
 * keeps at least one address; only empty intervals donate addresses. Thus a
 * sparse highlight can never be merged into a dark interval by adaptation.
 * Statistics use addressing coordinates, not the solver's distance metric. */
typedef struct sixel_palette_bin_range_cell {
    double count;
    double sum;
    double square_sum;
    double minimum;
    double maximum;
    double scale;
    unsigned int first;
    unsigned int divisions;
} sixel_palette_bin_range_cell_t;

typedef struct sixel_palette_bin_range {
    sixel_palette_bin_range_cell_t cells[3][256];
    unsigned int levels;
    int active[3];
    /* Borrowed producer evidence, valid until aggregation completes. */
    sixel_sample_statistics_t const *statistics;
    double scale[3];
    double offset[3];
    sixel_palette_binning_grid_map_t grid_map;
    int raw_coordinates;
    unsigned int axis_weight[3];
} sixel_palette_bin_range_t;

/* Model-dependent recovery budgets are bounded by producer evidence. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_range_prepare(sixel_palette_bin_range_t *range,
    sixel_sample_statistics_t const *statistics,
    sixel_sample_stream_t const *stream,
    sixel_weighted_point_set_t const *points,
    unsigned int requested_colors, double const scale[3],
    double const offset[3], sixel_palette_binning_grid_map_t grid_map,
    int raw_coordinates, unsigned int const *byte_lut,
    unsigned int strength, int model);

/* Refinement addresses use the producer's safe-tone projection; the caller
 * still accumulates its original sample and weight. Visible indices exclude
 * hidden samples in the same order as the producer's observations. */
SIXEL_INTERNAL_API unsigned int
sixel_palette_bin_range_sample_index(sixel_palette_bin_range_t const *range,
    unsigned int channel, unsigned int bin, double value,
    size_t visible_index);

/* Encoder workers install a frame-local override around palette building. */
SIXEL_INTERNAL_API void
sixel_palette_bin_range_set_strength(int enabled, unsigned int strength);

SIXEL_INTERNAL_API unsigned int
sixel_palette_bin_range_get_strength(void);

SIXEL_INTERNAL_API void
sixel_palette_bin_range_init(sixel_palette_bin_range_t *range,
                             unsigned int levels);

SIXEL_INTERNAL_API void
sixel_palette_bin_range_observe(sixel_palette_bin_range_t *range,
                                unsigned int channel,
                                unsigned int bin,
                                double value);

/* Reduce independent axis populations into this consumer's addressing grid.
 * A byte LUT preserves the Heckbert rounding convention. Raw coordinates
 * preserve the medoids distance domain; normalized coordinates serve the
 * common binning filter. No source pixels are accessed here. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_range_from_statistics(sixel_palette_bin_range_t *range,
    sixel_sample_statistics_t const *statistics, double const scale[3],
    double const offset[3], sixel_palette_binning_grid_map_t grid_map,
    int raw_coordinates, unsigned int const *byte_lut);

/* Probe only retained samples. Stop as soon as the nominal occupied count
 * reaches either the requested palette size or available safe-tone count.
 * No spatial measurement or colorspace conversion belongs to this gate. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_range_gate(sixel_sample_statistics_t const *statistics,
    sixel_sample_stream_t const *stream,
    sixel_weighted_point_set_t const *points,
    unsigned int levels, unsigned int requested_colors,
    double const scale[3], double const offset[3],
    sixel_palette_binning_grid_map_t grid_map, int raw_coordinates,
    unsigned int const *byte_lut, int *enabled);

SIXEL_INTERNAL_API void
sixel_palette_bin_range_finish(sixel_palette_bin_range_t *range,
                               int byte_coordinates, unsigned int strength);

SIXEL_INTERNAL_API unsigned int
sixel_palette_bin_range_index(sixel_palette_bin_range_t const *range,
                              unsigned int channel,
                              unsigned int bin,
                              double value);

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

/* Expand bin addresses; source coordinates and solver scales stay intact. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_bin_expand_bounds(void const *samples,
                               unsigned int sample_count,
                               int input_is_float32,
                               double scale[3], double offset[3]);

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
