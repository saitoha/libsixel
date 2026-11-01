/* SPDX-License-Identifier: MIT */
/* Presentation marks identify accepted paint only. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_retained_paint_marks(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    if (indexed_fixture_begin(&f) != SIXEL_OK ||
            indexed_fixture_seal(&f) != SIXEL_OK ||
            indexed_fixture_accept(&f) != SIXEL_OK ||
            indexed_fixture_commit(&f) != SIXEL_OK ||
            f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
            sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK) goto end;
    ok = f.stats.paint_size == 2 && f.stats.paint[0] == 1 &&
        f.stats.paint[1] == 0;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
