/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include "retained-plane-workers.h"
#include "threading.h"

#if SIXEL_ENABLE_THREADS
typedef struct sixel_retained_worker {
    sixel_retained_workers_t *owner;
    sixel_thread_t thread;
    sixel_retained_update_stats_t stats;
    int first;
    int last;
    int ready;
    int started;
} sixel_retained_worker_t;

struct sixel_retained_workers {
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    sixel_cond_t ready;
    sixel_cond_t done;
    int gate_ready;
    int ready_ready;
    int done_ready;
    int capacity;
    int remaining;
    int stopping;
    sixel_retained_worker_t *slots;
    sixel_retained_rows_t const *rows;
    sixel_threading_snapshot_t policy;
};

static int
sixel_retained_worker_main(void *context)
{
    sixel_retained_worker_t *worker;
    sixel_retained_workers_t *owner;
    sixel_thread_budget_scope_t scope;
    sixel_retained_update_stats_t stats;

    worker = context;
    owner = worker->owner;
    sixel_mutex_lock(&owner->gate);
    for (;;) {
        while (!worker->ready && !owner->stopping) {
            sixel_cond_wait(&owner->ready, &owner->gate);
        }
        if (owner->stopping) break;
        worker->ready = 0;
        sixel_mutex_unlock(&owner->gate);

        /* Capture belongs to this batch, not the pool's creation frame.
         * Enter/leave on every job also scopes latest-palette CPU loans. */
        sixel_threading_policy_enter(&scope, &owner->policy);
        if (owner->policy.work != NULL) {
            owner->policy.work(owner->policy.work_context, 1, 1);
        }
        memset(&stats, 0, sizeof(stats));
        sixel_retained_rows_run(owner->rows, worker->first,
            worker->last, &stats);
        if (owner->policy.work != NULL) {
            owner->policy.work(owner->policy.work_context, 1, 0);
        }
        sixel_thread_budget_leave(&scope);

        /* Per-pixel reductions stay on the worker's stack. Publish once,
         * avoiding cache-line contention between neighboring result slots. */
        sixel_mutex_lock(&owner->gate);
        worker->stats = stats;
        --owner->remaining;
        if (owner->remaining == 0) sixel_cond_signal(&owner->done);
    }
    sixel_mutex_unlock(&owner->gate);
    return SIXEL_OK;
}
#endif

SIXEL_INTERNAL_API void
sixel_retained_workers_free(sixel_retained_workers_t *workers)
{
#if SIXEL_ENABLE_THREADS
    sixel_allocator_t *allocator;
    int i;

    if (workers == NULL) return;
    allocator = workers->allocator;
    if (workers->gate_ready && workers->ready_ready) {
        sixel_mutex_lock(&workers->gate);
        workers->stopping = 1;
        sixel_cond_broadcast(&workers->ready);
        sixel_mutex_unlock(&workers->gate);
    }
    if (workers->slots != NULL) {
        for (i = 0; i < workers->capacity - 1; ++i) {
            if (workers->slots[i].started) {
                sixel_thread_join(&workers->slots[i].thread);
            }
        }
    }
    sixel_allocator_free(allocator, workers->slots);
    if (workers->done_ready) sixel_cond_destroy(&workers->done);
    if (workers->ready_ready) sixel_cond_destroy(&workers->ready);
    if (workers->gate_ready) sixel_mutex_destroy(&workers->gate);
    sixel_allocator_free(allocator, workers);
    sixel_allocator_unref(allocator);
#else
    (void)workers;
#endif
}

SIXEL_INTERNAL_API sixel_retained_workers_t *
sixel_retained_workers_new(sixel_allocator_t *allocator, int participants)
{
#if SIXEL_ENABLE_THREADS
    sixel_retained_workers_t *workers;
    int i;

    if (allocator == NULL || participants < 2 ||
            (size_t)(participants - 1) >
                SIZE_MAX / sizeof(sixel_retained_worker_t)) {
        return NULL;
    }
    workers = sixel_allocator_calloc(allocator, 1, sizeof(*workers));
    if (workers == NULL) return NULL;
    workers->allocator = allocator;
    workers->capacity = participants;
    sixel_allocator_ref(allocator);
    if (sixel_mutex_init(&workers->gate) != 0) goto failed;
    workers->gate_ready = 1;
    if (sixel_cond_init(&workers->ready) != 0) goto failed;
    workers->ready_ready = 1;
    if (sixel_cond_init(&workers->done) != 0) goto failed;
    workers->done_ready = 1;
    workers->slots = sixel_allocator_calloc(allocator,
        (size_t)(participants - 1), sizeof(*workers->slots));
    if (workers->slots == NULL) goto failed;
    for (i = 0; i < participants - 1; ++i) {
        workers->slots[i].owner = workers;
        if (sixel_thread_create(&workers->slots[i].thread,
                sixel_retained_worker_main, &workers->slots[i]) != 0) {
            goto failed;
        }
        workers->slots[i].started = 1;
    }
    return workers;
failed:
    sixel_retained_workers_free(workers);
    return NULL;
#else
    (void)allocator;
    (void)participants;
    return NULL;
#endif
}

SIXEL_INTERNAL_API int
sixel_retained_workers_capacity(sixel_retained_workers_t const *workers)
{
#if SIXEL_ENABLE_THREADS
    return workers != NULL ? workers->capacity : 1;
#else
    (void)workers;
    return 1;
#endif
}

SIXEL_INTERNAL_API void
sixel_retained_workers_run(sixel_retained_workers_t *workers,
    sixel_retained_rows_t const *rows, int height, int participants,
    sixel_retained_update_stats_t *stats)
{
#if SIXEL_ENABLE_THREADS
    int i;
    int first;
    int last;
    int remainder;

    if (workers != NULL && participants > 1) {
        /* The owner admitted this count before output. Compute boundaries
         * without multiplying height by a participant index: that product
         * can overflow even when the image itself fits in address space. */
        (void)sixel_threading_policy_capture(&workers->policy, 0);
        sixel_mutex_lock(&workers->gate);
        workers->rows = rows;
        workers->remaining = participants - 1;
        remainder = height % participants;
        first = 0;
        for (i = 0; i < participants - 1; ++i) {
            last = first + height / participants + (i < remainder);
            workers->slots[i].first = first;
            workers->slots[i].last = last;
            workers->slots[i].ready = 1;
            first = last;
        }
        sixel_cond_broadcast(&workers->ready);
        sixel_mutex_unlock(&workers->gate);
        sixel_retained_rows_run(rows, first, height, stats);

        /* An idle controller must not hold a CPU loan needed by a worker
         * completing this batch. Admission callbacks stay outside gates. */
        sixel_thread_work_end();
        sixel_mutex_lock(&workers->gate);
        while (workers->remaining != 0) {
            sixel_cond_wait(&workers->done, &workers->gate);
        }
        for (i = 0; i < participants - 1; ++i) {
            stats->painted_pixels += workers->slots[i].stats.painted_pixels;
            stats->refinement_needed |=
                workers->slots[i].stats.refinement_needed;
        }
        workers->rows = NULL;
        memset(&workers->policy, 0, sizeof(workers->policy));
        sixel_mutex_unlock(&workers->gate);
        sixel_thread_work_begin();
        stats->worker_threads = participants;
        return;
    }
#else
    (void)workers;
    (void)participants;
#endif
    sixel_retained_rows_run(rows, 0, height, stats);
    stats->worker_threads = 1;
}
