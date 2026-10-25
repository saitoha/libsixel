/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include "palette_store_test_common.h"

/* This case runs synchronously so allocator observation needs no lock. */
static void *palette_store_watched;
static int palette_store_destroyed;

static void
palette_store_observe_free(void *pointer)
{
    if (pointer == palette_store_watched && pointer != NULL) {
        ++palette_store_destroyed;
    }
    free(pointer);
}

int
test_palette_store_0035(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_palette_store_job_t *a;
    sixel_palette_store_job_t *b;
    sixel_palette_store_job_t *extra;
    sixel_palette_selection_t held = {0};
    sixel_dither_t *copy;
    int failed;

    (void)argc;
    (void)argv;
    failed = 1;
    copy = NULL;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    sixel_palette_store_free(f.store);
    f.store = NULL;
    sixel_allocator_unref(f.allocator);
    f.allocator = NULL;
    PALETTE_STORE_CHECK(sixel_allocator_new(&f.allocator, NULL, NULL,
        NULL, palette_store_observe_free) == SIXEL_OK);
    PALETTE_STORE_CHECK(sixel_palette_store_new(&f.store, f.allocator)
        == SIXEL_OK);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 1, NULL,
        SIXEL_OK, &a) == SIXEL_OK);
    sixel_palette_store_job_run(a);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 1, 1, &held));
    palette_store_watched = held.snapshot;
    palette_store_destroyed = 0;
    sixel_palette_store_job_release(a);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 2, NULL,
        SIXEL_OK, &a) == SIXEL_OK);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 3, NULL,
        SIXEL_OK, &b) == SIXEL_OK);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 4, NULL,
        SIXEL_OK, &extra) == SIXEL_FALSE && extra == NULL);
    /* Completing 3 first must retain 2 as a distinct future record. */
    sixel_palette_store_job_run(b);
    sixel_palette_store_job_run(a);
    sixel_palette_store_job_release(a);
    sixel_palette_store_job_release(b);
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 4, NULL,
        SIXEL_OK, &extra) == SIXEL_FALSE);
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 2, 2, NULL));
    PALETTE_STORE_CHECK(!palette_store_test_select(f.store, 3, 3, NULL));
    PALETTE_STORE_CHECK(palette_store_test_admit(&f, 4, NULL,
        SIXEL_OK, &extra) == SIXEL_OK);
    sixel_palette_store_free(f.store);
    f.store = NULL;
    PALETTE_STORE_CHECK(palette_store_destroyed == 0);
    PALETTE_STORE_CHECK(sixel_palette_snapshot_clone(held.snapshot, &copy)
        == SIXEL_OK);
    sixel_palette_snapshot_unref(held.snapshot);
    held.snapshot = NULL;
    PALETTE_STORE_CHECK(palette_store_destroyed == 1);
    failed = 0;
end:
    sixel_dither_unref(copy);
    sixel_palette_snapshot_unref(held.snapshot);
    palette_store_watched = NULL;
    return palette_store_fixture_free(&f) || failed;
}
