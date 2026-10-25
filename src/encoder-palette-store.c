/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "encoder-palette-store.h"
#include "threading.h"
#include "status.h"

#define SIXEL_PALETTE_STORE_SLOTS 2

struct sixel_palette_store_job {
    sixel_palette_store_t *store;
    sixel_palette_build_request_t *request;
    sixel_frame_t *sample;
    sixel_palette_selection_t result;
    sixel_palette_selection_t future;
    sixel_thread_t thread;
    sixel_threading_snapshot_t policy;
    sixel_palette_lease_t lease;
    sixel_status_message_t message;
    sixel_palette_job_test_hooks_t hooks;
    SIXELSTATUS status;
    int threads;
    int admitted;
    int started;
    int joined;
    int finished;
    int cancelled;
};

struct sixel_palette_store {
    sixel_allocator_t *allocator;
    sixel_mutex_t mutex;
    sixel_palette_selection_t active;
    sixel_palette_store_job_t jobs[SIXEL_PALETTE_STORE_SLOTS];
    uint64_t epoch;
    uint64_t attempt;
    uint64_t event;
    uint64_t frontier;
    uint64_t selected_sequence;
    int stopped;
};

SIXELSTATUS
sixel_palette_store_new(sixel_palette_store_t **out,
    sixel_allocator_t *allocator)
{
    sixel_palette_store_t *store;
    int i;

    *out = NULL;
    store = sixel_allocator_calloc(allocator, 1, sizeof(*store));
    if (store == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (sixel_mutex_init(&store->mutex) != 0) {
        sixel_allocator_free(allocator, store);
        return SIXEL_RUNTIME_ERROR;
    }
    sixel_allocator_ref(allocator);
    store->allocator = allocator;
    store->epoch = 1;
    for (i = 0; i < SIXEL_PALETTE_STORE_SLOTS; ++i) {
        store->jobs[i].store = store;
    }
    *out = store;
    return SIXEL_OK;
}

/* Detach under lock, destroy outside it: destructors can invoke an allocator
 * supplied by the application and must never run while the store is locked. */
void
sixel_palette_store_reset(sixel_palette_store_t *store, uint64_t frontier)
{
    sixel_palette_snapshot_t *retired[SIXEL_PALETTE_STORE_SLOTS + 1];
    int i;

    sixel_mutex_lock(&store->mutex);
    ++store->epoch;
    store->frontier = frontier;
    store->selected_sequence = frontier;
    retired[0] = store->active.snapshot;
    memset(&store->active, 0, sizeof(store->active));
    for (i = 0; i < SIXEL_PALETTE_STORE_SLOTS; ++i) {
        store->jobs[i].cancelled = 1;
        retired[i + 1] = store->jobs[i].future.snapshot;
        memset(&store->jobs[i].future, 0, sizeof(store->jobs[i].future));
    }
    sixel_mutex_unlock(&store->mutex);
    for (i = 0; i <= SIXEL_PALETTE_STORE_SLOTS; ++i) {
        sixel_palette_snapshot_unref(retired[i]);
    }
}

void
sixel_palette_store_stop(sixel_palette_store_t *store)
{
    sixel_mutex_lock(&store->mutex);
    store->stopped = 1;
    sixel_mutex_unlock(&store->mutex);
}

SIXELSTATUS
sixel_palette_store_admit(sixel_palette_store_t *store, uint64_t sequence,
    sixel_palette_build_request_t **request, sixel_frame_t **sample,
    sixel_palette_job_test_hooks_t const *hooks,
    sixel_palette_store_job_t **out)
{
    sixel_palette_store_job_t *job;
    sixel_threading_snapshot_t policy;
    int i;
    int threads;

    *out = NULL;
    if (sequence == 0 || *request == NULL || *sample == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* Policy resolution may allocate; freeze it before taking the lock. */
    (void)sixel_threading_policy_capture(&policy, 0);
    threads = sixel_threads_resolve();
    sixel_mutex_lock(&store->mutex);
    if (store->stopped) {
        sixel_mutex_unlock(&store->mutex);
        return SIXEL_INTERRUPTED;
    }
    job = NULL;
    for (i = 0; i < SIXEL_PALETTE_STORE_SLOTS; ++i) {
        if (store->jobs[i].admitted && !store->jobs[i].cancelled &&
                store->jobs[i].result.sequence == sequence) {
            sixel_mutex_unlock(&store->mutex);
            return SIXEL_LOGIC_ERROR;
        }
        if (!store->jobs[i].admitted &&
                store->jobs[i].future.snapshot == NULL) {
            job = &store->jobs[i];
        }
    }
    if (job == NULL) {
        sixel_mutex_unlock(&store->mutex);
        return SIXEL_FALSE;
    }
    memset(job, 0, sizeof(*job));
    job->store = store;
    job->request = *request;
    job->sample = *sample;
    job->policy = policy;
    job->threads = threads;
    job->admitted = 1;
    job->result.epoch = store->epoch;
    job->result.sequence = sequence;
    job->result.attempt = ++store->attempt;
    if (hooks != NULL) {
        job->hooks = *hooks;
    }
    *request = NULL;
    *sample = NULL;
    *out = job;
    sixel_mutex_unlock(&store->mutex);
    return SIXEL_OK;
}

static void
sixel_palette_store_build(sixel_palette_store_job_t *job)
{
    sixel_palette_store_t *store;
    sixel_palette_frame_state_t state;
    sixel_palette_snapshot_t *snapshot;
    sixel_palette_snapshot_t *retired;
    sixel_palette_selection_t *destination;
    sixel_dither_t *built;
    sixel_thread_budget_scope_t policy_scope;
    sixel_thread_budget_scope_t budget_scope;
    SIXELSTATUS status;

    store = job->store;
    built = NULL;
    snapshot = retired = NULL;
    sixel_threading_policy_enter(&policy_scope, &job->policy);
    sixel_thread_budget_enter(&budget_scope, job->threads);
    sixel_helper_set_additional_message(NULL);
    status = sixel_palette_build_request_execute(job->request, job->sample,
        &built, &state, NULL);
    if (status == SIXEL_OK) {
        status = sixel_palette_snapshot_take(&snapshot, job->request,
            &built, &state);
    }
    sixel_dither_unref(built);
    if (job->hooks.before_publish != NULL) {
        job->hooks.before_publish(job->hooks.context);
    }
    if (status == SIXEL_OK && job->hooks.completion_status != SIXEL_OK) {
        status = job->hooks.completion_status;
    }
    if (status != SIXEL_OK) {
        sixel_status_take_message(&job->message);
        sixel_palette_snapshot_unref(snapshot);
        snapshot = NULL;
    }
    sixel_thread_budget_leave(&budget_scope);
    sixel_thread_budget_leave(&policy_scope);

    /* Publish only a complete successful attempt. The own result remains
     * separate from the store reference and from any application dither. */
    sixel_mutex_lock(&store->mutex);
    job->result.snapshot = snapshot;
    job->status = status;
    if (snapshot != NULL && !store->stopped && !job->cancelled &&
            job->result.epoch == store->epoch) {
        job->result.published = ++store->event;
        destination = &job->future;
        if (job->result.sequence <= store->frontier) {
            destination = &store->active;
        }
        if (job->result.sequence >= store->selected_sequence &&
                (destination->snapshot == NULL ||
                 destination->sequence < job->result.sequence)) {
            retired = destination->snapshot;
            *destination = job->result;
            sixel_palette_snapshot_ref(snapshot);
        }
    }
    job->finished = 1;
    sixel_mutex_unlock(&store->mutex);
    sixel_palette_snapshot_unref(retired);
    /* A completed publication is not a joined thread. Tests can hold this
     * boundary while readers replace the record or shutdown waits to join. */
    if (job->hooks.after_publish != NULL) {
        job->hooks.after_publish(job->hooks.context);
    }
}

void
sixel_palette_store_job_run(sixel_palette_store_job_t *job)
{
    /* Keep the frozen policy, but fit synchronous execution to the caller's
     * current lane. Its budget may be narrower than it was at admission. */
    job->threads = sixel_threads_resolve();
    sixel_palette_store_build(job);
}

static int
sixel_palette_store_worker(void *context)
{
    sixel_palette_store_build(context);
    return 0;
}

SIXELSTATUS
sixel_palette_store_job_start(sixel_palette_store_job_t *job,
    sixel_palette_scheduler_t *scheduler)
{
    int threads;

    threads = sixel_palette_scheduler_acquire(scheduler,
        SIXEL_PALETTE_LANE_BUILD, 0, &job->lease);
    if (threads == 0) {
        return SIXEL_FALSE;
    }
    job->policy = scheduler->policy;
    job->threads = threads;
    if (job->hooks.fail_thread_create ||
            sixel_thread_create(&job->thread,
                sixel_palette_store_worker, job)) {
        /* No worker exists, so return immediately. Keep the very same
         * sample and frozen policy for synchronous use in the caller lane. */
        sixel_palette_lease_release(&job->lease);
        job->threads = sixel_threads_resolve();
        return SIXEL_FALSE;
    }
    job->started = 1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_store_job_status(sixel_palette_store_job_t *job, int *finished)
{
    SIXELSTATUS status;

    sixel_mutex_lock(&job->store->mutex);
    *finished = job->finished;
    status = job->status;
    sixel_mutex_unlock(&job->store->mutex);
    return status;
}

SIXELSTATUS
sixel_palette_store_job_join(sixel_palette_store_job_t *job,
    SIXELSTATUS frame_status)
{
    if (!job->joined) {
        if (job->hooks.before_join != NULL) {
            job->hooks.before_join(job->hooks.context);
        }
        if (job->started) {
            sixel_thread_join(&job->thread);
        }
        sixel_palette_lease_release(&job->lease);
        job->joined = 1;
    }
    if (frame_status == SIXEL_OK && job->status != SIXEL_OK) {
        sixel_status_restore_message(&job->message);
        return job->status;
    }
    return frame_status;
}

void
sixel_palette_store_job_cancel(sixel_palette_store_job_t *job)
{
    sixel_palette_snapshot_t *active;
    sixel_palette_snapshot_t *future;
    sixel_palette_store_t *store;

    store = job->store;
    active = NULL;
    sixel_mutex_lock(&store->mutex);
    job->cancelled = 1;
    future = job->future.snapshot;
    memset(&job->future, 0, sizeof(job->future));
    if (store->active.attempt == job->result.attempt) {
        active = store->active.snapshot;
        memset(&store->active, 0, sizeof(store->active));
    }
    sixel_mutex_unlock(&store->mutex);
    sixel_palette_snapshot_unref(active);
    sixel_palette_snapshot_unref(future);
}

void
sixel_palette_store_job_result(sixel_palette_store_job_t *job,
    sixel_palette_selection_t *result)
{
    /* The owning lane calls this after join, so no worker can write result. */
    *result = job->result;
    sixel_palette_snapshot_ref(result->snapshot);
}

void
sixel_palette_store_job_release(sixel_palette_store_job_t *job)
{
    /* Cleanup must not replace diagnostics from an earlier write failure.
     * The frame owner collects the build status explicitly before release. */
    (void)sixel_palette_store_job_join(job, SIXEL_INTERRUPTED);
    sixel_palette_build_request_free(job->request);
    sixel_frame_unref(job->sample);
    sixel_palette_snapshot_unref(job->result.snapshot);
    sixel_status_dispose_message(&job->message);
    /* A future record can outlive its joined job but still occupies its
     * slot. Only frontier advancement or cancellation makes it reusable. */
    sixel_mutex_lock(&job->store->mutex);
    job->request = NULL;
    job->sample = NULL;
    job->result.snapshot = NULL;
    job->admitted = 0;
    sixel_mutex_unlock(&job->store->mutex);
}

SIXELSTATUS
sixel_palette_store_select(sixel_palette_store_t *store, uint64_t sequence,
    sixel_palette_selection_t *result)
{
    sixel_palette_selection_t *candidate;
    sixel_palette_snapshot_t *retired[SIXEL_PALETTE_STORE_SLOTS];
    SIXELSTATUS status;
    int count;
    int i;

    memset(result, 0, sizeof(*result));
    count = 0;
    status = SIXEL_OK;
    sixel_mutex_lock(&store->mutex);
    if (store->stopped || sequence < store->frontier) {
        status = store->stopped ? SIXEL_INTERRUPTED : SIXEL_LOGIC_ERROR;
        goto unlock;
    }
    store->frontier = sequence;
    for (i = 0; i < SIXEL_PALETTE_STORE_SLOTS; ++i) {
        if (store->jobs[i].admitted && !store->jobs[i].cancelled &&
                store->jobs[i].result.epoch == store->epoch &&
                store->jobs[i].result.sequence == sequence &&
                store->jobs[i].finished &&
                store->jobs[i].status != SIXEL_OK) {
            status = store->jobs[i].status;
            goto unlock;
        }
    }
    for (i = 0; i < SIXEL_PALETTE_STORE_SLOTS; ++i) {
        candidate = &store->jobs[i].future;
        if (candidate->snapshot == NULL || candidate->sequence > sequence) {
            continue;
        }
        if (candidate->sequence > store->active.sequence) {
            retired[count++] = store->active.snapshot;
            store->active = *candidate;
        } else {
            retired[count++] = candidate->snapshot;
        }
        memset(candidate, 0, sizeof(*candidate));
    }
    *result = store->active;
    result->selected = ++store->event;
    if (result->snapshot != NULL) {
        store->selected_sequence = result->sequence;
    }
    sixel_palette_snapshot_ref(result->snapshot);
unlock:
    sixel_mutex_unlock(&store->mutex);
    for (i = 0; i < count; ++i) {
        sixel_palette_snapshot_unref(retired[i]);
    }
    return status;
}

void
sixel_palette_store_free(sixel_palette_store_t *store)
{
    sixel_allocator_t *allocator;
    int i;

    if (store == NULL) {
        return;
    }
    sixel_palette_store_stop(store);
    for (i = 0; i < SIXEL_PALETTE_STORE_SLOTS; ++i) {
        if (store->jobs[i].admitted) {
            sixel_palette_store_job_release(&store->jobs[i]);
        }
    }
    sixel_palette_store_reset(store, 0);
    allocator = store->allocator;
    sixel_mutex_destroy(&store->mutex);
    sixel_allocator_free(allocator, store);
    sixel_allocator_unref(allocator);
}
