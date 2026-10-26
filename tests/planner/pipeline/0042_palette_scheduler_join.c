/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_store_test_common.h"

int
test_palette_scheduler_0042(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *job;
    sixel_palette_lease_t p = {0}, m = {0};
    int used[3];
    int finished, failed;

    (void)argc;
    (void)argv;
    failed = 1;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    /* W=1 serializes build jobs without reducing the emission budget. */
    f.scheduler.prepare_limit = 1;
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, &f.gates[0],
        SIXEL_OK, &job) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(job, &f.scheduler)
        == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&f.scheduler,
        SIXEL_PALETTE_LANE_APPLY, 0, &m) == 4);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].entered);
    PALETTE_STORE_CHECK(f.gates[0].threads == 1 &&
        f.gates[0].palette_mode == SIXEL_THREADING_PALETTE_LATEST);
    palette_store_gate_open(&f.gates[0], 0);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].published);
    PALETTE_STORE_CHECK(sixel_palette_store_job_status(job, &finished)
        == SIXEL_OK && finished);
    sixel_palette_scheduler_usage(&f.scheduler, used);
    PALETTE_STORE_CHECK(used[0] == 1 && used[1] == 0 && used[2] == 4);
    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&f.scheduler,
        SIXEL_PALETTE_LANE_BUILD, 1, &p) == 0);
    palette_store_gate_open(&f.gates[0], 1);
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(job, SIXEL_OK)
        == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_scheduler_acquire(&f.scheduler,
        SIXEL_PALETTE_LANE_BUILD, 1, &p) == 1);
    failed = 0;
end:
    sixel_palette_lease_release(&p);
    sixel_palette_lease_release(&m);
    return palette_store_fixture_free(&f) || failed;
}
