/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "../../_support/planner/pipeline/palette_store_test_common.h"

typedef struct palette_admission_probe {
    sixel_palette_scheduler_t *scheduler;
    sixel_palette_lane_t lane;
    int failed;
} palette_admission_probe_t;

static int
palette_admission_worker(void *context)
{
    palette_admission_probe_t *probe;
    sixel_palette_lease_t lease = {0};
    int iteration, used[3];

    probe = context;
    for (iteration = 0; iteration < 256; ++iteration) {
        (void)sixel_palette_scheduler_acquire(probe->scheduler,
            probe->lane, 0, &lease);
        sixel_palette_scheduler_usage(probe->scheduler, used);
        probe->failed |= used[0] > 3 || used[1] > 3 || used[2] != 4;
        sixel_palette_lease_release(&lease);
    }
    return 0;
}

int
test_palette_scheduler_0046(int argc, char **argv)
{
    palette_store_fixture_t f;
    palette_admission_probe_t probes[2];
    sixel_thread_t threads[2];
    sixel_palette_scheduler_t other;
    sixel_palette_lease_t m = {0}, independent = {0};
    int used[3];
    int i, started, initialized, failed;

    (void)argc;
    (void)argv;
    started = initialized = 0;
    failed = 1;
    memset(probes, 0, sizeof(probes));
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    f.scheduler.prepare_limit = 3;
    PALETTE_STORE_CHECK(sixel_palette_scheduler_init(&other,
        &f.scheduler.policy) == SIXEL_OK);
    initialized = 1;
    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&other,
        SIXEL_PALETTE_LANE_APPLY, 0, &independent) == 4);
    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&f.scheduler,
        SIXEL_PALETTE_LANE_APPLY, 1, &m) == 4);
    for (i = 0; i < 2; ++i) {
        probes[i].scheduler = &f.scheduler;
        probes[i].lane = (sixel_palette_lane_t)i;
        PALETTE_STORE_CHECK(sixel_thread_create(&threads[i],
            palette_admission_worker, &probes[i]) == SIXEL_OK);
        ++started;
    }
    for (i = 0; i < started; ++i) {
        sixel_thread_join(&threads[i]);
    }
    started = 0;
    PALETTE_STORE_CHECK(!probes[0].failed && !probes[1].failed);
    sixel_palette_scheduler_usage(&other, used);
    PALETTE_STORE_CHECK(used[0] == 0 && used[1] == 0 && used[2] == 4);
    sixel_palette_lease_release(&m);
    sixel_palette_scheduler_usage(&f.scheduler, used);
    PALETTE_STORE_CHECK(used[0] + used[1] + used[2] == 0);
    failed = 0;
end:
    for (i = 0; i < started; ++i) {
        sixel_thread_join(&threads[i]);
    }
    sixel_palette_lease_release(&independent);
    sixel_palette_lease_release(&m);
    if (initialized) {
        sixel_palette_scheduler_destroy(&other);
    }
    return palette_store_fixture_free(&f) || failed;
}
