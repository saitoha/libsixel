/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_store_test_common.h"

static int
palette_store_shutdown(void *context)
{
    sixel_palette_store_free(context);
    return 0;
}

int
test_palette_store_0037(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *job;
    sixel_palette_store_job_t *extra;
    sixel_palette_selection_t held = {0};
    sixel_thread_t shutdown;
    int failed;
    int started;

    (void)argc;
    (void)argv;
    failed = 1;
    started = 0;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, &f.gates[0],
        SIXEL_OK, &job) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(job,
        &f.scheduler) == SIXEL_OK);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].entered);
    palette_store_gate_open(&f.gates[0], 0);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].published);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1, &held));
    sixel_palette_store_stop(f.store);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 2, NULL,
        SIXEL_OK, &extra) == SIXEL_INTERRUPTED);
    PALETTE_STORE_CHECK(sixel_thread_create(&shutdown,
        palette_store_shutdown, f.store) == 0);
    started = 1;
    palette_store_gate_wait(&f.gates[0], &f.gates[0].join);
    /* The worker is still inside after_publish; shutdown must join it
     * before releasing its request, palette and store synchronization. */
    palette_store_gate_open(&f.gates[0], 1);
    sixel_thread_join(&shutdown);
    started = 0;
    f.store = NULL;
    PALETTE_STORE_CHECK(sixel_palette_snapshot_profile(held.snapshot)
        ->paintable_colors == 2);
    failed = 0;
end:
    if (started) {
        palette_store_gate_open(&f.gates[0], 1);
        sixel_thread_join(&shutdown);
        f.store = NULL;
    }
    sixel_palette_snapshot_unref(held.snapshot);
    return palette_store_fixture_free(&f) || failed;
}
