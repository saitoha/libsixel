/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "palette_store_test_common.h"

int
test_palette_store_0039(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *jobs[2];
    sixel_palette_selection_t selected = {0};
    uint64_t sequence;
    uint64_t previous;
    uint64_t event;
    int pair;
    int slot;
    int finished;
    int failed;

    (void)argc;
    (void)argv;
    failed = 1;
    previous = event = 0;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    for (pair = 0; pair < 8; ++pair) {
        for (slot = 0; slot < 2; ++slot) {
            sequence = (uint64_t)(pair * 2 + slot + 1);
            PALETTE_STORE_CHECK(palette_store_test_admit(&f, sequence,
                NULL, SIXEL_OK, &jobs[slot]) == SIXEL_OK);
            PALETTE_STORE_CHECK(sixel_palette_store_job_start(
                jobs[slot], &f.scheduler)
                == SIXEL_OK);
        }
        for (slot = 0; slot < 2; ++slot) {
            sequence = (uint64_t)(pair * 2 + slot + 1);
            /* Observe live publication without ordering it through a test
             * gate. Only the store mutex orders these reads and writes. */
            do {
                PALETTE_STORE_CHECK(sixel_palette_store_select(f.store,
                    sequence, &selected) == SIXEL_OK);
                PALETTE_STORE_CHECK(selected.selected > event);
                event = selected.selected;
                if (selected.snapshot != NULL) {
                    PALETTE_STORE_CHECK(selected.sequence >= previous &&
                        selected.sequence <= sequence &&
                        selected.published < selected.selected);
                    previous = selected.sequence;
                }
                sixel_palette_snapshot_unref(selected.snapshot);
                selected.snapshot = NULL;
                PALETTE_STORE_CHECK(sixel_palette_store_job_status(
                    jobs[slot], &finished) == SIXEL_OK);
            } while (!finished);
            PALETTE_STORE_CHECK(sixel_palette_store_job_join(jobs[slot],
                SIXEL_OK) == SIXEL_OK);
            PALETTE_STORE_CHECK(!palette_store_test_select(f.store,
                sequence, sequence, NULL));
            previous = sequence;
            sixel_palette_store_job_release(jobs[slot]);
        }
    }
    failed = 0;
end:
    sixel_palette_snapshot_unref(selected.snapshot);
    return palette_store_fixture_free(&f) || failed;
}
