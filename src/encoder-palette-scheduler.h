/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_PALETTE_SCHEDULER_H
#define LIBSIXEL_ENCODER_PALETTE_SCHEDULER_H

#include "threading.h"
#include "planner.h"

/* Session-local CPU reservations, including each lane's caller. Load,
 * control and writer threads are not CPU work reservations. */
typedef enum sixel_palette_lane {
    SIXEL_PALETTE_LANE_BUILD,
    SIXEL_PALETTE_LANE_PREPARE,
    SIXEL_PALETTE_LANE_APPLY
} sixel_palette_lane_t;

typedef struct sixel_palette_scheduler {
    sixel_mutex_t mutex;
    sixel_threading_snapshot_t policy;
    int enabled;
    int prepare_limit;
    int used[3];
} sixel_palette_scheduler_t;

typedef struct sixel_palette_lease {
    sixel_palette_scheduler_t *scheduler;
    sixel_palette_lane_t lane;
    int threads;
} sixel_palette_lease_t;

/* Input is the resolved encoder policy. Feature eligibility is decided by
 * the session owner. Disabled schedulers grant nothing: use the frame path. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_scheduler_init(sixel_palette_scheduler_t *scheduler,
    sixel_threading_snapshot_t const *policy);
/* All leases and worker scopes must have ended before destruction. */
SIXEL_INTERNAL_API void
sixel_palette_scheduler_destroy(sixel_palette_scheduler_t *scheduler);
/* Nonblocking, partial admission. A zero grant defers work without holding
 * an operation gate or waiting for another lane. Start P, then optional Q,
 * then M; P/Q always leave at least one slot for M. A zero-initialized lease
 * has one owner and cannot grow while its work or child pools are running.
 * requested <= 0 asks for the lane's default maximum. */
SIXEL_INTERNAL_API int
sixel_palette_scheduler_acquire(sixel_palette_scheduler_t *scheduler,
    sixel_palette_lane_t lane, int requested, sixel_palette_lease_t *lease);
/* Return only after the lane and all its children have joined. Publication
 * alone does not make a palette reservation reusable. */
SIXEL_INTERNAL_API void
sixel_palette_lease_release(sixel_palette_lease_t *lease);
/* Enter on the executing thread. The frozen policy carries stage caps and
 * frame keys; the nested budget bounds child pools to the granted lease. */
SIXEL_INTERNAL_API void
sixel_palette_lease_enter(sixel_palette_lease_t const *lease,
    sixel_thread_budget_scope_t *policy, sixel_thread_budget_scope_t *budget);
/* Call before fixing application geometry, with a live M lease. */
SIXEL_INTERNAL_API void
sixel_palette_lease_allocate(sixel_palette_lease_t const *lease,
    sixel_dither_encode_request_t const *request,
    sixel_dither_encode_allocation_t *allocation);
SIXEL_INTERNAL_API void
sixel_palette_scheduler_usage(sixel_palette_scheduler_t *scheduler,
    int used[3]);

#endif /* LIBSIXEL_ENCODER_PALETTE_SCHEDULER_H */
