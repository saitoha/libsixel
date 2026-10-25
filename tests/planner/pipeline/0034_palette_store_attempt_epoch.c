/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_store_test_common.h"

int
test_palette_store_0034(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *old;
    sixel_palette_store_job_t *fresh;
    sixel_palette_selection_t a = {0};
    sixel_palette_selection_t b = {0};
    int failed;

    (void)argc;
    (void)argv;
    failed = 1;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, &f.gates[0],
        SIXEL_OK, &old) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(old,
        &f.scheduler) == SIXEL_OK);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].entered);
    sixel_palette_store_job_cancel(old);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, NULL,
        SIXEL_OK, &fresh) == SIXEL_OK);
    sixel_palette_store_job_run(fresh);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1, &a));
    palette_store_gate_open(&f.gates[0], 1);
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(old, SIXEL_OK)
        == SIXEL_OK);
    sixel_palette_store_job_result(old, &b);
    PALETTE_STORE_CHECK(b.published == 0 && b.attempt < a.attempt);
    sixel_palette_snapshot_unref(b.snapshot);
    b.snapshot = NULL;
    sixel_palette_store_job_release(old);
    /* Reset also removes an already published attempt; held readers live. */
    sixel_palette_store_reset(f.store, 1);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 2, 0, NULL));
    sixel_palette_store_job_release(fresh);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 2, &f.gates[1],
        SIXEL_OK, &old) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(old,
        &f.scheduler) == SIXEL_OK);
    palette_store_gate_wait(&f.gates[1], &f.gates[1].entered);
    sixel_palette_store_reset(f.store, 2);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 3, NULL,
        SIXEL_OK, &fresh) == SIXEL_OK);
    sixel_palette_store_job_run(fresh);
    palette_store_gate_open(&f.gates[1], 1);
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(old, SIXEL_OK)
        == SIXEL_OK);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 3, 3, &b));
    PALETTE_STORE_CHECK(b.epoch > a.epoch && b.attempt > a.attempt);
    failed = 0;
end:
    sixel_palette_snapshot_unref(a.snapshot);
    sixel_palette_snapshot_unref(b.snapshot);
    return palette_store_fixture_free(&f) || failed;
}
