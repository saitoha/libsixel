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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdint.h>

#include <sixel.h>

#include "weighted-point-set.h"

static int
sixel_weighted_point_colorspace_is_valid(int colorspace)
{
    return colorspace >= SIXEL_COLORSPACE_GAMMA &&
        colorspace <= SIXEL_COLORSPACE_DIN99D;
}

void
sixel_weighted_point_set_init(sixel_weighted_point_set_t *set)
{
    if (set == NULL) {
        return;
    }

    set->coordinates = NULL;
    set->weights = NULL;
    set->allocator = NULL;
    set->ownership = SIXEL_WEIGHTED_POINT_EMPTY;
    set->policy = SIXEL_PALETTE_BINNING_AUTO;
    set->bits_per_axis = 0u;
    set->grid_map = SIXEL_PALETTE_BINNING_GRID_NONE;
    set->kernel = SIXEL_PALETTE_BINNING_KERNEL_NONE;
    set->backend = SIXEL_PALETTE_BINNING_BACKEND_UNRESOLVED;
    set->source_point_count = 0u;
    set->point_count = 0u;
    set->entry_capacity_bound = 0u;
    set->total_weight = 0.0;
    set->colorspace = SIXEL_COLORSPACE_GAMMA;
}

static SIXELSTATUS
sixel_weighted_point_set_bind(
    sixel_weighted_point_set_t *set,
    double *coordinates,
    double *weights,
    size_t point_count,
    double total_weight,
    int colorspace,
    sixel_palette_binning_state_t const *binning,
    sixel_weighted_point_ownership_t ownership,
    sixel_allocator_t *allocator)
{
    sixel_palette_binning_policy_t policy;

    policy = SIXEL_PALETTE_BINNING_AUTO;
    if (set == NULL || coordinates == NULL || binning == NULL ||
            point_count == 0u || point_count > SIZE_MAX / 3u ||
            !isfinite(total_weight) || total_weight <= 0.0 ||
            !sixel_weighted_point_colorspace_is_valid(colorspace) ||
            (ownership != SIXEL_WEIGHTED_POINT_BORROWED &&
             ownership != SIXEL_WEIGHTED_POINT_OWNED) ||
            (ownership == SIXEL_WEIGHTED_POINT_OWNED && allocator == NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (set->ownership != SIXEL_WEIGHTED_POINT_EMPTY ||
            set->coordinates != NULL || set->weights != NULL ||
            set->allocator != NULL) {
        return SIXEL_LOGIC_ERROR;
    }
    if (binning->policy.phase != SIXEL_PALETTE_POLICY_RESOLVED &&
            binning->policy.phase != SIXEL_PALETTE_POLICY_EXECUTED) {
        return SIXEL_LOGIC_ERROR;
    }
    policy = (sixel_palette_binning_policy_t)binning->policy.effective;
    if (policy < SIXEL_PALETTE_BINNING_NONE ||
            policy > SIXEL_PALETTE_BINNING_SOFT ||
            binning->source_point_count == 0u ||
            binning->entry_capacity_bound == 0u ||
            point_count > binning->entry_capacity_bound ||
            (policy == SIXEL_PALETTE_BINNING_NONE &&
             point_count != binning->source_point_count) ||
            (policy != SIXEL_PALETTE_BINNING_NONE && weights == NULL) ||
            (weights == NULL && total_weight != (double)point_count) ||
            (weights != NULL && coordinates == weights)) {
        return SIXEL_BAD_ARGUMENT;
    }

    set->coordinates = coordinates;
    set->weights = weights;
    set->ownership = ownership;
    set->policy = policy;
    set->bits_per_axis = binning->bits_per_axis;
    set->grid_map = binning->grid_map;
    set->kernel = binning->kernel;
    set->backend = binning->backend;
    set->source_point_count = binning->source_point_count;
    set->point_count = point_count;
    set->entry_capacity_bound = binning->entry_capacity_bound;
    set->total_weight = total_weight;
    set->colorspace = colorspace;
    if (ownership == SIXEL_WEIGHTED_POINT_OWNED) {
        sixel_allocator_ref(allocator);
        set->allocator = allocator;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_weighted_point_set_bind_borrowed(
    sixel_weighted_point_set_t *set,
    double *coordinates,
    double *weights,
    size_t point_count,
    double total_weight,
    int colorspace,
    sixel_palette_binning_state_t const *binning)
{
    return sixel_weighted_point_set_bind(set,
                                         coordinates,
                                         weights,
                                         point_count,
                                         total_weight,
                                         colorspace,
                                         binning,
                                         SIXEL_WEIGHTED_POINT_BORROWED,
                                         NULL);
}

SIXELSTATUS
sixel_weighted_point_set_take_owned(
    sixel_weighted_point_set_t *set,
    double **coordinates_slot,
    double **weights_slot,
    size_t point_count,
    double total_weight,
    int colorspace,
    sixel_palette_binning_state_t const *binning,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    double *weights;

    status = SIXEL_FALSE;
    weights = NULL;
    if (coordinates_slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (weights_slot != NULL) {
        weights = *weights_slot;
    }
    status = sixel_weighted_point_set_bind(
        set,
        *coordinates_slot,
        weights,
        point_count,
        total_weight,
        colorspace,
        binning,
        SIXEL_WEIGHTED_POINT_OWNED,
        allocator);
    if (SIXEL_SUCCEEDED(status)) {
        *coordinates_slot = NULL;
        if (weights_slot != NULL) {
            *weights_slot = NULL;
        }
    }
    return status;
}

void
sixel_weighted_point_set_dispose(sixel_weighted_point_set_t *set)
{
    sixel_allocator_t *allocator;

    if (set == NULL) {
        return;
    }
    allocator = set->allocator;
    if (set->ownership == SIXEL_WEIGHTED_POINT_OWNED && allocator != NULL) {
        if (set->coordinates != NULL) {
            sixel_allocator_free(allocator, set->coordinates);
        }
        if (set->weights != NULL) {
            sixel_allocator_free(allocator, set->weights);
        }
    }
    sixel_weighted_point_set_init(set);
    sixel_allocator_unref(allocator);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
