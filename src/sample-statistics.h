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

#ifndef LIBSIXEL_SAMPLE_STATISTICS_H
#define LIBSIXEL_SAMPLE_STATISTICS_H

#include <sixel.h>
#include <stdint.h>

/* Recovery evidence may justify several candidates per palette entry. */
#define SIXEL_SAMPLE_SAFE_LIMIT (SIXEL_PALETTE_MAX * 4u)

/* Exact marginal populations in the coordinates published to binning.
 * No strength, grid depth, palette size or solver decision belongs here.
 * Byte values use fixed counters; floating values retain exact finite
 * coordinates in sparse tables. The object is immutable after publication. */
typedef struct sixel_sample_axis_entry {
    double value;
    size_t count;
} sixel_sample_axis_entry_t;

typedef struct sixel_sample_statistics {
    sixel_allocator_t *allocator;
    /* References are managed by the owning orchestration thread. Readers
     * may share the immutable observations, but do not mutate ownership. */
    unsigned int references;
    int colorspace;
    int byte_coordinates;
    size_t point_count;
    size_t byte_counts[3][256];
    sixel_sample_axis_entry_t *axes[3];
    size_t capacities[3];
    size_t sizes[3];
    double minimum[3];
    double maximum[3];
    /* Distinct gamma-RGB safe tones, capped at the candidate evidence limit.
     * This is an observation, not a requested palette or binning budget.
     * Float projections retain the output-resolvable addressing coordinate
     * for each visible sample. Solver coordinates remain untouched. */
    unsigned int safe_color_count;
    int safe_colors_valid;
    uint32_t safe_color_keys[SIXEL_SAMPLE_SAFE_LIMIT * 2];
    float safe_color_values[SIXEL_SAMPLE_SAFE_LIMIT * 2][3];
    float *safe_coordinates;
    uint32_t *safe_keys;
    size_t safe_capacity;
} sixel_sample_statistics_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_sample_statistics_new(sixel_sample_statistics_t **out,
    int colorspace, int byte_coordinates, sixel_allocator_t *allocator);

/* Observation is private to producers, before the artifact is published. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_sample_statistics_observe(sixel_sample_statistics_t *statistics,
    double const coordinates[3]);

/* A colorspace producer may already own the matching gamma source sample.
 * Reuse it instead of reversing the transform solely for tone evidence. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_sample_statistics_observe_gamma(sixel_sample_statistics_t *statistics,
    double const coordinates[3], double const gamma_source[3]);

SIXEL_INTERNAL_API void
sixel_sample_statistics_ref(sixel_sample_statistics_t *statistics);
SIXEL_INTERNAL_API void
sixel_sample_statistics_unref(sixel_sample_statistics_t *statistics);

#endif
