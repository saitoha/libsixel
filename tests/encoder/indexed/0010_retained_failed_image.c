/* SPDX-License-Identifier: MIT */
/* A failed producer cannot commit an accepted stream. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_retained_failed_image(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    if (indexed_fixture_begin(&f) != SIXEL_OK ||
            indexed_fixture_seal(&f) != SIXEL_OK ||
            indexed_fixture_accept(&f) != SIXEL_OK) goto end;
    sixel_indexed_frame_fail(f.image);
    ok = indexed_fixture_commit(&f) == SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
