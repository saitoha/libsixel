/* SPDX-License-Identifier: MIT */
/* A receipt cannot publish a resolved update. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0035_direct_reject_resolved(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 1) != SIXEL_OK) goto end;
    if (indexed_fixture_seal(&f) != SIXEL_OK ||
            indexed_fixture_accept(&f) != SIXEL_OK) goto end;
    ok = indexed_fixture_commit(&f) == SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
