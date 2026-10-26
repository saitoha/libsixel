/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "encoder-palette-scheduler.h"
#include "loader-common.h"

SIXELSTATUS
sixel_palette_scheduler_init(sixel_palette_scheduler_t *scheduler,
    sixel_threading_snapshot_t const *policy)
{
    memset(scheduler, 0, sizeof(*scheduler));
#if SIXEL_ENABLE_THREADS
    if (sixel_mutex_init(&scheduler->mutex) != 0) {
        return SIXEL_RUNTIME_ERROR;
    }
#endif
    if (sixel_cond_init(&scheduler->changed) != 0) {
        sixel_mutex_destroy(&scheduler->mutex);
        return SIXEL_RUNTIME_ERROR;
    }
    scheduler->policy = *policy;
    scheduler->prepare_limit = policy->budget;
    if (policy->options.prepare_threads_override &&
            policy->options.prepare_threads > 0 &&
            policy->options.prepare_threads < (unsigned int)policy->budget) {
        scheduler->prepare_limit = (int)policy->options.prepare_threads;
    }
#if SIXEL_ENABLE_THREADS && !defined(__PCC__) && !defined(__TINYC__)
    scheduler->enabled = policy->budget >= 2 &&
        (policy->options.palette == SIXEL_THREADING_PALETTE_LATEST ||
         policy->options.palette == SIXEL_THREADING_PALETTE_AUTO);
#endif
    return SIXEL_OK;
}

void
sixel_palette_scheduler_destroy(sixel_palette_scheduler_t *scheduler)
{
    sixel_cond_destroy(&scheduler->changed);
    sixel_mutex_destroy(&scheduler->mutex);
}

int
sixel_palette_scheduler_acquire(sixel_palette_scheduler_t *scheduler,
    sixel_palette_lane_t lane, int requested, sixel_palette_lease_t *lease)
{
    int available;
    int limit;

    if (!scheduler->enabled || lease->threads != 0) {
        return 0;
    }
    sixel_mutex_lock(&scheduler->mutex);
    if (lane == SIXEL_PALETTE_LANE_APPLY) {
        /* M owns the entire allocator budget regardless of running loans.
         * It never waits for a background callback or reduces its geometry. */
        available = scheduler->used[lane] == 0
            ? scheduler->policy.budget : 0;
    } else if (lane == SIXEL_PALETTE_LANE_PREPARE) {
        available = scheduler->prepare_limit - scheduler->used[lane];
    } else {
        available = scheduler->prepare_limit - scheduler->used[lane];
        limit = scheduler->policy.options.mode == SIXEL_THREADING_POLICY_LAZY
            ? 1 : scheduler->policy.budget / 2;
        if (available > limit) {
            available = limit;
        }
    }
    if (lane != SIXEL_PALETTE_LANE_APPLY &&
            requested > 0 && available > requested) {
        available = requested;
    }
    if (available > 0) {
        scheduler->used[lane] += available;
        lease->scheduler = scheduler;
        lease->lane = lane;
        lease->threads = available;
    }
    sixel_mutex_unlock(&scheduler->mutex);
    return available > 0 ? available : 0;
}

/* Foreground admission only updates demand. Running background callbacks
 * finish their current job, then must request another loan. No foreground
 * path waits for their acknowledgement, including when demand rises to N. */
void
sixel_palette_scheduler_foreground(void *context, int worker, int begin)
{
    sixel_palette_scheduler_t *scheduler;

    scheduler = context;
    sixel_mutex_lock(&scheduler->mutex);
    if (worker) {
        scheduler->foreground_jobs += begin ? 1 : -1;
    } else {
        scheduler->foreground_controller = begin;
    }
    sixel_trace_topic_message("palette_latest",
        "LSXLOAN1|event=foreground|worker=%d|begin=%d|controller=%d|"
        "main=%d|background=%d|budget=%d",
        worker, begin, scheduler->foreground_controller,
        scheduler->foreground_jobs, scheduler->background_jobs,
        scheduler->policy.budget);
    sixel_cond_broadcast(&scheduler->changed);
    sixel_mutex_unlock(&scheduler->mutex);
}

void
sixel_palette_scheduler_background(void *context, int worker, int begin)
{
    sixel_palette_scheduler_t *scheduler;
    int waiting;

    (void)worker;
    scheduler = context;
    waiting = 0;
    sixel_mutex_lock(&scheduler->mutex);
    if (begin) {
        while (scheduler->foreground_controller ||
                scheduler->foreground_jobs + scheduler->background_jobs >=
                    scheduler->policy.budget ||
                scheduler->background_jobs >= scheduler->prepare_limit) {
            /* Announce once. Re-announcing after each wake could make
             * two borrowers wake each other indefinitely with no loan. */
            if (!waiting) {
                waiting = 1;
                ++scheduler->waiting_jobs;
                sixel_cond_broadcast(&scheduler->changed);
            }
            sixel_cond_wait(&scheduler->changed, &scheduler->mutex);
        }
        scheduler->waiting_jobs -= waiting;
        ++scheduler->background_jobs;
    } else {
        --scheduler->background_jobs;
    }
    sixel_trace_topic_message("palette_latest",
        "LSXLOAN1|event=background|worker=%d|begin=%d|controller=%d|"
        "main=%d|background=%d|budget=%d",
        worker, begin, scheduler->foreground_controller,
        scheduler->foreground_jobs, scheduler->background_jobs,
        scheduler->policy.budget);
    sixel_cond_broadcast(&scheduler->changed);
    sixel_mutex_unlock(&scheduler->mutex);
}

void
sixel_palette_lease_release(sixel_palette_lease_t *lease)
{
    sixel_palette_scheduler_t *scheduler;

    scheduler = lease->scheduler;
    if (scheduler == NULL) {
        return;
    }
    sixel_mutex_lock(&scheduler->mutex);
    scheduler->used[lease->lane] -= lease->threads;
    sixel_mutex_unlock(&scheduler->mutex);
    memset(lease, 0, sizeof(*lease));
}

void
sixel_palette_lease_enter(sixel_palette_lease_t const *lease,
    sixel_thread_budget_scope_t *policy, sixel_thread_budget_scope_t *budget)
{
    sixel_threading_policy_enter(policy, &lease->scheduler->policy);
    sixel_thread_budget_enter(budget, lease->threads);
}

void
sixel_palette_lease_allocate(sixel_palette_lease_t const *lease,
    sixel_dither_encode_request_t const *request,
    sixel_dither_encode_allocation_t *allocation)
{
    sixel_dither_encode_request_t local;
    sixel_threading_policy_options_t const *options;

    local = *request;
    options = &lease->scheduler->policy.options;
    local.total_threads = lease->threads;
    local.lazy = options->mode == SIXEL_THREADING_POLICY_LAZY;
    local.threads_max = options->dither_threads;
    local.threads_max_set = options->dither_threads_override;
    local.encode_threads_max = options->encode_threads;
    local.encode_threads_max_set = options->encode_threads_override;
    sixel_dither_encode_allocate(&local, allocation);
}

void
sixel_palette_scheduler_usage(sixel_palette_scheduler_t *scheduler,
    int used[3])
{
    sixel_mutex_lock(&scheduler->mutex);
    memcpy(used, scheduler->used, sizeof(scheduler->used));
    sixel_mutex_unlock(&scheduler->mutex);
}
