/* SPDX-License-Identifier: MIT */
/* Resolved publication does not require output acceptance. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0033_advance_without_receipt(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 1) != SIXEL_OK) goto end;
    if (indexed_fixture_seal(&f) != SIXEL_OK) goto end;
    ok = f.plane->vtbl->advance_resolved(f.plane, f.update, &f.stats)
        == SIXEL_OK;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
