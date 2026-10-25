/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_store_test_common.h"

int
test_palette_store_0033(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *job;
    sixel_palette_store_job_t *slow;
    sixel_palette_selection_t chosen = {0};
    sixel_palette_selection_t own = {0};
    int failed;

    (void)argc;
    (void)argv;
    failed = 1;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, &f.gates[0],
        SIXEL_OK, &job) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(job, 1) == SIXEL_OK);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].entered);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 0, NULL));
    palette_store_gate_open(&f.gates[0], 1);
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(job, SIXEL_OK)
        == SIXEL_OK);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1, NULL));
    sixel_palette_store_job_release(job);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 2, &f.gates[1],
        SIXEL_OK, &slow) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(slow, 1) == SIXEL_OK);
    palette_store_gate_wait(&f.gates[1], &f.gates[1].entered);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 3, NULL,
        SIXEL_OK, &job) == SIXEL_OK);
    sixel_palette_store_job_run(job);
    /* Future 3 cannot hide the eligible 1 while own 2 is unfinished. */
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 2, 1, &chosen));
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 3, 3, NULL));
    palette_store_gate_open(&f.gates[1], 1);
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(slow, SIXEL_OK)
        == SIXEL_OK);
    sixel_palette_store_job_result(slow, &own);
    PALETTE_STORE_CHECK(own.sequence == 2 && own.published > chosen.selected);
    PALETTE_STORE_CHECK(chosen.sequence == 1);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 4, 3, NULL));
    failed = 0;
end:
    sixel_palette_snapshot_unref(own.snapshot);
    sixel_palette_snapshot_unref(chosen.snapshot);
    return palette_store_fixture_free(&f) || failed;
}
