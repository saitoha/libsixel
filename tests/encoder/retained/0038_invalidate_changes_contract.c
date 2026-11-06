/* SPDX-License-Identifier: MIT */
/* Invalidation permits a different publication contract. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0038_invalidate_changes_contract(int argc, char **argv)
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
    if (f.plane->vtbl->invalidate(f.plane, SIXEL_RETAINED_SEEK) != SIXEL_OK)
        goto end;
    memset(&f.image->identity, 0, sizeof(f.image->identity));
    ok = indexed_fixture_begin_kind(&f, 0) == SIXEL_OK;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
