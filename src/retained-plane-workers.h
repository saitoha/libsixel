/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_RETAINED_PLANE_WORKERS_H
#define LIBSIXEL_RETAINED_PLANE_WORKERS_H

#include "retained-plane-rows.h"

/* A private RetainedPlane executor, not a shared scheduling service. It has
 * one synchronous, infallible row batch and no queue, cancellation protocol
 * or frame ownership. Admission is before output; run never creates workers.
 * The owner serializes all calls and keeps row storage alive through run. */
typedef struct sixel_retained_workers sixel_retained_workers_t;

SIXEL_INTERNAL_API sixel_retained_workers_t *
sixel_retained_workers_new(sixel_allocator_t *allocator, int participants);

SIXEL_INTERNAL_API void
sixel_retained_workers_free(sixel_retained_workers_t *workers);

SIXEL_INTERNAL_API int
sixel_retained_workers_capacity(sixel_retained_workers_t const *workers);

SIXEL_INTERNAL_API void
sixel_retained_workers_run(sixel_retained_workers_t *workers,
    sixel_retained_rows_t const *rows, int height, int participants,
    sixel_retained_update_stats_t *stats);

#endif
