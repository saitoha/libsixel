/* SPDX-License-Identifier: MIT */
/* A duplicate receipt cannot commit the update twice. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_retained_duplicate(int argc, char **argv)
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
            indexed_fixture_commit(&f) != SIXEL_OK) goto end;
    ok = indexed_fixture_commit(&f) == SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
