/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "../../_support/planner/pipeline/palette_store_test_common.h"

typedef struct palette_scope_probe {
    sixel_palette_lease_t lease;
    int failed;
} palette_scope_probe_t;

static int
palette_scope_worker(void *context)
{
    palette_scope_probe_t *probe;
    sixel_thread_budget_scope_t policy, budget, child;
    unsigned int cap;

    probe = context;
    sixel_palette_lease_enter(&probe->lease, &policy, &budget);
    probe->failed = sixel_threads_resolve() != probe->lease.threads ||
        sixel_threading_policy_mode() != SIXEL_THREADING_POLICY_GREEDY ||
        sixel_threading_policy_palette_mode() != SIXEL_THREADING_PALETTE_LATEST;
    probe->failed |= !sixel_threading_policy_stage_threads(
        SIXEL_THREADING_STAGE_DITHER, &cap) || cap != 7;
    /* A nested pool must not escape the lane by asking for the full N. */
    sixel_thread_budget_enter(&child, 100);
    probe->failed |= sixel_threads_resolve() != probe->lease.threads;
    sixel_thread_budget_leave(&child);
    sixel_thread_budget_leave(&budget);
    sixel_thread_budget_leave(&policy);
    return 0;
}

int
test_palette_scheduler_0041(int argc, char **argv)
{
    sixel_palette_scheduler_t scheduler;
    sixel_threading_snapshot_t policy;
    sixel_thread_budget_scope_t outer;
    palette_scope_probe_t probe = {{0}, 0};
    sixel_thread_t thread;
    int failed, initialized, entered;

    (void)argc;
    (void)argv;
    failed = 1;
    initialized = entered = 0;
    memset(&policy, 0, sizeof(policy));
    policy.budget = 8;
    policy.options.mode = SIXEL_THREADING_POLICY_GREEDY;
    policy.options.palette = SIXEL_THREADING_PALETTE_LATEST;
    policy.options.dither_threads_override = 1;
    policy.options.dither_threads = 7;
    PALETTE_STORE_CHECK(sixel_palette_scheduler_init(&scheduler, &policy)
        == SIXEL_OK);
    initialized = 1;
    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&scheduler,
        SIXEL_PALETTE_LANE_PREPARE, 3, &probe.lease) == 3);
    policy.budget = 1;
    policy.options.mode = SIXEL_THREADING_POLICY_LAZY;
    sixel_threading_policy_enter(&outer, &policy);
    entered = 1;
    PALETTE_STORE_CHECK(sixel_thread_create(&thread,
        palette_scope_worker, &probe) == SIXEL_OK);
    sixel_thread_join(&thread);
    PALETTE_STORE_CHECK(!probe.failed && sixel_threads_resolve() == 1);
    failed = 0;
end:
    if (entered) {
        sixel_thread_budget_leave(&outer);
    }
    sixel_palette_lease_release(&probe.lease);
    if (initialized) {
        sixel_palette_scheduler_destroy(&scheduler);
    }
    return failed;
}
