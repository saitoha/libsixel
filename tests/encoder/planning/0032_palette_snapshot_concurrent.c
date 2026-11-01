/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "../../_support/planner/pipeline/palette_stage2_common.h"
#include "src/threading.h"

typedef struct palette_snapshot_reader {
    sixel_palette_snapshot_t *snapshot;
    sixel_mutex_t *start;
    int failed;
} palette_snapshot_reader_t;

static int
palette_snapshot_read(void *context)
{
    palette_snapshot_reader_t *reader;
    sixel_dither_t *copy;
    sixel_palette_entries_view_t view;
    unsigned char pixel[3] = {32, 64, 128};
    int index;
    int color;

    reader = context;
    sixel_mutex_lock(reader->start);
    sixel_mutex_unlock(reader->start);
    for (index = 0; index < 16; ++index) {
        copy = NULL;
        if (SIXEL_FAILED(sixel_palette_snapshot_clone(reader->snapshot,
                &copy))) {
            reader->failed = 1;
            break;
        }
        color = copy->lookup_policy->vtbl->map_pixel(copy->lookup_policy,
                                                     pixel);
        reader->failed |= color < 0 || color >= 2;
        copy->palette->vtbl->get_entries(copy->palette, &view);
        memset((unsigned char *)view.entries, 0, 9);
        sixel_dither_unref(copy);
    }
    sixel_palette_snapshot_unref(reader->snapshot);
    return 0;
}

int
test_palette_stage2_0032(int argc, char **argv)
{
    sixel_palette_snapshot_t *snapshot;
    sixel_mutex_t start;
    sixel_thread_t threads[2];
    palette_snapshot_reader_t readers[2];
    int created[2];
    int index;

    (void)argc;
    (void)argv;
    if (palette_stage2_snapshot(NULL, 0, SIXEL_LUT_POLICY_FHEDT, 1,
            &snapshot)) {
        return 1;
    }
    if (sixel_mutex_init(&start) != 0) {
        sixel_palette_snapshot_unref(snapshot);
        return 1;
    }
    sixel_mutex_lock(&start);
    for (index = 0; index < 2; ++index) {
        readers[index].snapshot = snapshot;
        readers[index].start = &start;
        readers[index].failed = 0;
        sixel_palette_snapshot_ref(snapshot);
        created[index] = sixel_thread_create(&threads[index],
            palette_snapshot_read, &readers[index]) == 0;
    }
    /* Readers now own all references, even when the publisher disappears. */
    sixel_palette_snapshot_unref(snapshot);
    sixel_mutex_unlock(&start);
    for (index = 0; index < 2; ++index) {
        if (created[index]) {
            sixel_thread_join(&threads[index]);
        } else {
            palette_snapshot_read(&readers[index]);
        }
    }
    sixel_mutex_destroy(&start);
    return readers[0].failed || readers[1].failed;
}
