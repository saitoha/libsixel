/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_store_test_common.h"

int
test_palette_store_0038(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_t *peer;
    sixel_palette_store_job_t *old;
    sixel_palette_store_job_t *fresh;
    sixel_palette_selection_t held = {0};
    sixel_palette_selection_t replacement = {0};
    int failed;

    (void)argc;
    (void)argv;
    failed = 1;
    peer = NULL;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    PALETTE_STORE_CHECK(sixel_palette_store_new(&peer, f.allocator)
        == SIXEL_OK);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, NULL,
        SIXEL_OK, &old) == SIXEL_OK);
    sixel_palette_store_job_run(old);
    /* Cancel an already published future before its selection. */
    sixel_palette_store_job_cancel(old);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 0, NULL));
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, NULL,
        SIXEL_OK, &fresh) == SIXEL_OK);
    sixel_palette_store_job_run(fresh);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1, &held));
    PALETTE_STORE_CHECK(!palette_store_test_select(peer, 1, 0, NULL));
    sixel_palette_store_job_release(old);
    /* Cancellation can retire the active store ref without invalidating
     * an application reference, and a same-sequence repair can replace it. */
    sixel_palette_store_job_cancel(fresh);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, NULL,
        SIXEL_OK, &old) == SIXEL_OK);
    sixel_palette_store_job_run(old);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1,
        &replacement));
    PALETTE_STORE_CHECK(replacement.attempt > held.attempt &&
        replacement.snapshot != held.snapshot);
    sixel_palette_store_stop(f.store);
    sixel_palette_store_reset(peer, 1);
    PALETTE_STORE_CHECK(!palette_store_test_select(peer, 2, 0, NULL));
    failed = 0;
end:
    sixel_palette_snapshot_unref(held.snapshot);
    sixel_palette_snapshot_unref(replacement.snapshot);
    sixel_palette_store_free(peer);
    return palette_store_fixture_free(&f) || failed;
}
