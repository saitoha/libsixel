/* SPDX-License-Identifier: MIT */
/* Invalidation cannot race a prepared update. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_retained_busy_invalidate(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    if (indexed_fixture_begin(&f) != SIXEL_OK) goto end;
    ok = f.plane->vtbl->invalidate(f.plane, SIXEL_RETAINED_SEEK) ==
        SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
