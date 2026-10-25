/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_PALETTE_STORE_H
#define LIBSIXEL_ENCODER_PALETTE_STORE_H

#include "encoder-palette.h"
#include "encoder-palette-scheduler.h"

typedef struct sixel_palette_store sixel_palette_store_t;
typedef struct sixel_palette_store_job sixel_palette_store_job_t;

/* A retained record is also the selection event. Publication and selection
 * serials come from the same mutex, not from wall-clock timestamps. Release
 * snapshot after all application workers finish; store replacement is safe. */
typedef struct sixel_palette_selection {
    sixel_palette_snapshot_t *snapshot;
    uint64_t epoch;
    uint64_t sequence;
    uint64_t attempt;
    uint64_t published;
    uint64_t selected;
} sixel_palette_selection_t;

/* Optional in-process test barriers/faults. Called without the store lock.
 * Production callers pass NULL. The context must live through job join. */
typedef struct sixel_palette_job_test_hooks {
    void (*before_publish)(void *context);
    void (*after_publish)(void *context);
    void (*before_join)(void *context);
    void *context;
    SIXELSTATUS completion_status;
    int completion_retryable;
    int fail_thread_create;
} sixel_palette_job_test_hooks_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_store_new(sixel_palette_store_t **out,
    sixel_allocator_t *allocator);
/* Reset invalidates both published records and running attempts. It does
 * not recycle worker slots or return their budgets before join. */
SIXEL_INTERNAL_API void
sixel_palette_store_reset(sixel_palette_store_t *store, uint64_t frontier);
SIXEL_INTERNAL_API void
sixel_palette_store_stop(sixel_palette_store_t *store);
/* Quiesce admission/selection callers first. Free stops publication, joins
 * every admitted worker, and only then destroys references and the mutex. */
SIXEL_INTERNAL_API void
sixel_palette_store_free(sixel_palette_store_t *store);

/* At most two jobs and two future records, plus one eligible record. Move
 * request and private sample only on success. A full store returns FALSE;
 * the scheduler must finish current work before retrying admission. Each
 * job's start/join/release calls have one owner, independent of other jobs. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_store_admit(sixel_palette_store_t *store, uint64_t sequence,
    sixel_palette_build_request_t **request, sixel_frame_t **sample,
    sixel_palette_job_test_hooks_t const *hooks,
    sixel_palette_store_job_t **out);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_store_job_start(sixel_palette_store_job_t *job,
    sixel_palette_scheduler_t *scheduler);
/* Start returns FALSE on unavailable admission, RUNTIME_ERROR on thread
 * creation failure.
 * No lease remains and the same request/sample can be run synchronously in
 * a live Q/M lease or after the other lanes have joined (frame fallback).
 * No grant can also defer admission; it never authorizes unbudgeted work.
 * Never select an older palette after serial fallback. Sampling retry
 * remains the frame owner's responsibility. */
SIXEL_INTERNAL_API void
sixel_palette_store_job_run(sixel_palette_store_job_t *job);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_store_job_status(sixel_palette_store_job_t *job, int *finished);
/* Preserve an earlier frame error; otherwise report the own build error,
 * including one discovered after emission. Restores worker diagnostics. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_store_job_join(sixel_palette_store_job_t *job,
    SIXELSTATUS frame_status);
/* Construction failures may retry AUTO sampling; lookup failures may not.
 * Query only after joining the own job. */
SIXEL_INTERNAL_API int
sixel_palette_store_job_retryable(sixel_palette_store_job_t *job);
SIXEL_INTERNAL_API void
sixel_palette_store_job_cancel(sixel_palette_store_job_t *job);
/* Release joins before freeing inputs. Cancel a discarded future job first;
 * a successful current job leaves its published palette in the store. */
SIXEL_INTERNAL_API void
sixel_palette_store_job_release(sixel_palette_store_job_t *job);
/* Obtain the own result after join, even when another palette was selected.
 * The caller owns the returned snapshot reference. */
SIXEL_INTERNAL_API void
sixel_palette_store_job_result(sixel_palette_store_job_t *job,
    sixel_palette_selection_t *result);
/* Advance in frame order. Select the greatest published s <= sequence once
 * per application transaction. A known own error wins over an older palette.
 * An empty successful result means the caller must wait for its own job. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_store_select(sixel_palette_store_t *store, uint64_t sequence,
    sixel_palette_selection_t *result);

#endif /* LIBSIXEL_ENCODER_PALETTE_STORE_H */
