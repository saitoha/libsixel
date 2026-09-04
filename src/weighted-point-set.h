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

#ifndef LIBSIXEL_WEIGHTED_POINT_SET_H
#define LIBSIXEL_WEIGHTED_POINT_SET_H

#include <sixel.h>

#include "palette-plan.h"

typedef enum sixel_weighted_point_ownership {
    SIXEL_WEIGHTED_POINT_EMPTY = 0,
    SIXEL_WEIGHTED_POINT_BORROWED,
    SIXEL_WEIGHTED_POINT_OWNED
} sixel_weighted_point_ownership_t;

/*
 * Typed output of the logical binning stage.
 *
 * Coordinates contain three interleaved doubles in the palette colorspace.
 * A NULL weight array represents unit mass for every point and is permitted
 * only for the unaggregated `none` policy.  Aggregated policies must publish
 * explicit weights so quantizers cannot accidentally discard sample mass.
 */
typedef struct sixel_weighted_point_set {
    double *coordinates;
    double *weights;
    sixel_allocator_t *allocator;
    sixel_weighted_point_ownership_t ownership;
    sixel_palette_binning_policy_t policy;
    unsigned int bits_per_axis;
    sixel_palette_binning_grid_map_t grid_map;
    sixel_palette_binning_kernel_t kernel;
    sixel_palette_binning_backend_t backend;
    size_t source_point_count;
    size_t point_count;
    size_t entry_capacity_bound;
    double total_weight;
    int colorspace;
} sixel_weighted_point_set_t;

SIXEL_INTERNAL_API void
sixel_weighted_point_set_init(sixel_weighted_point_set_t *set);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_weighted_point_set_bind_borrowed(
    sixel_weighted_point_set_t *set,
    double *coordinates,
    double *weights,
    size_t point_count,
    double total_weight,
    int colorspace,
    sixel_palette_binning_state_t const *binning);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_weighted_point_set_take_owned(
    sixel_weighted_point_set_t *set,
    double **coordinates_slot,
    double **weights_slot,
    size_t point_count,
    double total_weight,
    int colorspace,
    sixel_palette_binning_state_t const *binning,
    sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_weighted_point_set_dispose(sixel_weighted_point_set_t *set);

#endif /* LIBSIXEL_WEIGHTED_POINT_SET_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
