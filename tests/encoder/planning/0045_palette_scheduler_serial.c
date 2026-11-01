/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "../../_support/planner/pipeline/palette_store_test_common.h"

int
test_palette_scheduler_0045(int argc, char **argv)
{
    sixel_palette_scheduler_t scheduler;
    sixel_threading_snapshot_t policy;
    sixel_palette_lease_t lease = {0};
    int mode, used[3], failed, initialized;

    (void)argc;
    (void)argv;
    failed = 1;
    initialized = 0;
    memset(&policy, 0, sizeof(policy));
    for (mode = 0; mode < 3; ++mode) {
        policy.budget = mode == 0 ? 1 : 8;
        policy.options.palette = mode == 1 ? SIXEL_THREADING_PALETTE_FRAME
            : SIXEL_THREADING_PALETTE_LATEST;
#if SIXEL_ENABLE_THREADS && !defined(__PCC__) && !defined(__TINYC__)
        if (mode == 2) {
            continue;
        }
#endif
        PALETTE_STORE_CHECK(sixel_palette_scheduler_init(&scheduler, &policy)
            == SIXEL_OK);
        initialized = 1;
        /* No altered budget survives a decision to use the frame path. */
        PALETTE_STORE_CHECK(!scheduler.enabled &&
            sixel_palette_scheduler_acquire(&scheduler,
                SIXEL_PALETTE_LANE_BUILD, 0, &lease) == 0);
        sixel_palette_scheduler_usage(&scheduler, used);
        PALETTE_STORE_CHECK(used[0] + used[1] + used[2] == 0 &&
            scheduler.policy.budget == policy.budget);
        sixel_palette_scheduler_destroy(&scheduler);
        initialized = 0;
    }
    failed = 0;
end:
    sixel_palette_lease_release(&lease);
    if (initialized) {
        sixel_palette_scheduler_destroy(&scheduler);
    }
    return failed;
}
