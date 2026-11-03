/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_RETAINED_PLANE_ROWS_H
#define LIBSIXEL_RETAINED_PLANE_ROWS_H

#include <6cells.h>

/* Private RetainedPlane execution view. Origins are already applied to the
 * destination pointers; source indexes and paint marks are frame-local.
 * A prepared view borrows immutable palette/index storage through commit.
 * Disjoint row ranges may execute concurrently with separate statistics. */
typedef struct sixel_retained_rows {
    sixel_wire_palette_view_t palette;
    sixel_index_t const *indexes;
    unsigned char const *skip;
    unsigned char *rgb;
    unsigned char *valid;
    unsigned char *paint;
    int width;
    size_t stride;
    int neon;
    unsigned char lut[3][256];
} sixel_retained_rows_t;

SIXEL_INTERNAL_API void
sixel_retained_rows_prepare(sixel_retained_rows_t *rows,
    sixel_wire_palette_view_t const *palette);

/* Preconditions are checked by the component before publishing output.
 * This kernel cannot fail, allocate, create workers or change sequence. */
SIXEL_INTERNAL_API void
sixel_retained_rows_run(sixel_retained_rows_t const *rows,
    int first, int last, sixel_retained_update_stats_t *stats);

#endif
