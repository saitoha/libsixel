/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "palette_store_test_common.h"

int
test_palette_store_0036(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *job;
    sixel_palette_selection_t selected = {0};
    sixel_palette_selection_t own = {0};
    int finished;
    int failed;

    (void)argc;
    (void)argv;
    failed = 1;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, NULL,
        SIXEL_OK, &job) == SIXEL_OK);
    sixel_palette_store_job_run(job);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1, NULL));
    sixel_palette_store_job_release(job);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 2, &f.gates[0],
        SIXEL_BAD_ALLOCATION, &job) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_job_start(job,
        &f.scheduler) == SIXEL_OK);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].entered);
    PALETTE_STORE_CHECK(sixel_palette_store_job_status(job, &finished)
        == SIXEL_OK && !finished);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 2, 1, &selected));
    palette_store_gate_open(&f.gates[0], 1);
    palette_store_gate_wait(&f.gates[0], &f.gates[0].published);
    PALETTE_STORE_CHECK(sixel_palette_store_job_status(job, &finished)
        == SIXEL_BAD_ALLOCATION && finished);
    /* Selection after known failure cannot mask it with the older result. */
    PALETTE_STORE_CHECK(sixel_palette_store_select(f.store, 2, &own)
        == SIXEL_BAD_ALLOCATION && own.snapshot == NULL);
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(job, SIXEL_OK)
        == SIXEL_BAD_ALLOCATION);
    PALETTE_STORE_CHECK(strstr(sixel_helper_get_additional_message(),
        "palette store completion fault") != NULL);
    sixel_palette_store_job_result(job, &own);
    PALETTE_STORE_CHECK(own.snapshot == NULL && own.published == 0);
    PALETTE_STORE_CHECK(selected.sequence == 1);
    sixel_helper_set_additional_message("earlier write error");
    PALETTE_STORE_CHECK(sixel_palette_store_job_join(job, SIXEL_INTERRUPTED)
        == SIXEL_INTERRUPTED);
    sixel_palette_store_job_release(job);
    PALETTE_STORE_CHECK(strcmp(sixel_helper_get_additional_message(),
        "earlier write error") == 0);
    failed = 0;
end:
    sixel_palette_snapshot_unref(selected.snapshot);
    sixel_palette_snapshot_unref(own.snapshot);
    return palette_store_fixture_free(&f) || failed;
}
