/* SPDX-License-Identifier: MIT */
/* Direct publication still requires a receipt. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0034_direct_missing_receipt(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 0) != SIXEL_OK) goto end;
    if (indexed_fixture_seal(&f) != SIXEL_OK) goto end;
    ok = f.plane->vtbl->commit(f.plane, f.update, NULL, &f.stats)
        == SIXEL_BAD_ARGUMENT;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
