/* SPDX-License-Identifier: MIT */
/* A painted index updates only its declared plane origin. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_retained_origin(int argc, char **argv)
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
    ok = memcmp(f.view.rgb, "\12\24\36\377\0\0\12\24\36", 9) == 0;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
