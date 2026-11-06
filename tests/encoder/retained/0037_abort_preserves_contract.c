/* SPDX-License-Identifier: MIT */
/* Abort does not change the epoch publication contract. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0037_abort_preserves_contract(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 1) != SIXEL_OK) goto end;
    if (f.plane->vtbl->abort(f.plane, f.update) != SIXEL_OK) goto end;
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    sixel_retained_update_unref(f.update);
    f.update = NULL;
    ok = indexed_fixture_begin_kind(&f, 0) == SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
