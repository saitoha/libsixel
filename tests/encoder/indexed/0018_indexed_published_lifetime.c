/* SPDX-License-Identifier: MIT */
/* Published rows survive release of the producer reference. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_indexed_published_lifetime(int argc, char **argv)
{
    indexed_fixture_t f;
    sixel_indexed_rows_view_t rows;
    sixel_indexed_frame_t *consumer;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    if (sixel_indexed_frame_publish(f.image, 1) != SIXEL_OK) goto end;
    consumer = f.image;
    sixel_indexed_frame_ref(consumer);
    sixel_indexed_frame_unref(f.image);
    f.image = NULL;
    ok = sixel_indexed_frame_get_rows(consumer, 0, 1, &rows) == SIXEL_OK &&
        rows.indexes[0] == 0;
    sixel_indexed_frame_unref(consumer);
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
