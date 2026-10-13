/* SPDX-License-Identifier: MIT */
/* Private destination lattice. A grid belongs to one palette build. */
#ifndef LIBSIXEL_PALETTE_SNAP_GRID_H
#define LIBSIXEL_PALETTE_SNAP_GRID_H

#include <sixel.h>
#include "cms.h"

typedef struct sixel_snap_grid sixel_snap_grid_t;

enum sixel_snap_grid_policy {
    SIXEL_SNAP_GRID_AUTO = 0,
    SIXEL_SNAP_GRID_DIRECT,
    SIXEL_SNAP_GRID_TABLE,
    SIXEL_SNAP_GRID_CACHE,
    SIXEL_SNAP_GRID_FULL
};

/* The grid owns the analytic destination model after successful creation.
 * Caller definitions may be freed immediately; ICC LUT models are rejected. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_snap_grid_new(sixel_snap_grid_t **grid, int policy, int colorspace,
                     sixel_color_def_t const *destination,
                     sixel_allocator_t *allocator);

SIXEL_INTERNAL_API void
sixel_snap_grid_destroy(sixel_snap_grid_t *grid);

/* Input and result use the grid's typed coordinates. Nearest compares the
 * eight corners of the containing destination cell, not a global search. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_snap_grid_project(sixel_snap_grid_t *grid, double const source[3],
                         int nearest, double lightness_weight,
                         double result[3]);

/* Expose one decoded RGB101 target for numeric tests and final preparation. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_snap_grid_target(sixel_snap_grid_t *grid, unsigned int code,
                        double result[3]);

#endif
