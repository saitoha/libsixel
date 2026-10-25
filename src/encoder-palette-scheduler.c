/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "encoder-palette-scheduler.h"

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
    scheduler->policy = *policy;
    scheduler->prepare_limit = policy->budget;
    if (policy->options.prepare_threads_override &&
            policy->options.prepare_threads > 0 &&
            policy->options.prepare_threads < (unsigned int)policy->budget) {
        scheduler->prepare_limit = (int)policy->options.prepare_threads;
    }
#if SIXEL_ENABLE_THREADS && !defined(__PCC__) && !defined(__TINYC__)
    scheduler->enabled = policy->budget >= 2 &&
        policy->options.palette == SIXEL_THREADING_PALETTE_LATEST;
#endif
    return SIXEL_OK;
}

void
sixel_palette_scheduler_destroy(sixel_palette_scheduler_t *scheduler)
{
    sixel_mutex_destroy(&scheduler->mutex);
}

int
sixel_palette_scheduler_acquire(sixel_palette_scheduler_t *scheduler,
    sixel_palette_lane_t lane, int requested, sixel_palette_lease_t *lease)
{
    int available;
    int prepare;
    int application;
    int limit;
    int lazy;

    if (!scheduler->enabled || lease->threads != 0) {
        return 0;
    }
    lazy = scheduler->policy.options.mode == SIXEL_THREADING_POLICY_LAZY;
    sixel_mutex_lock(&scheduler->mutex);
    prepare = scheduler->used[SIXEL_PALETTE_LANE_BUILD] +
        scheduler->used[SIXEL_PALETTE_LANE_PREPARE];
    application = scheduler->used[SIXEL_PALETTE_LANE_APPLY];
    if (lane != SIXEL_PALETTE_LANE_APPLY && application == 0) {
        application = 1;
    }
    available = scheduler->policy.budget - prepare - application;
    if (lane != SIXEL_PALETTE_LANE_APPLY) {
        limit = scheduler->prepare_limit - prepare;
        if (available > limit) {
            available = limit;
        }
        /* P receives half of N initially; Q uses only the remaining cap.
         * A second palette takes a fresh lease, never Q's running slots. */
        limit = lane == SIXEL_PALETTE_LANE_BUILD
            ? scheduler->policy.budget / 2 : scheduler->policy.budget;
        if (lazy) {
            limit = 1;
        }
        if (available > limit) {
            available = limit;
        }
    }
    if (requested > 0 && available > requested) {
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
