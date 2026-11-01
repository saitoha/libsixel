/* SPDX-License-Identifier: MIT */
/* Unpublished rows cannot be consumed. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_indexed_unpublished(int argc, char **argv)
{
    indexed_fixture_t f;
    sixel_indexed_rows_view_t rows;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    ok = sixel_indexed_frame_get_rows(f.image, 0, 1, &rows) == SIXEL_FALSE;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
