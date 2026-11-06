/* SPDX-License-Identifier: MIT */
/* Resolved advance cannot publish a direct update. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0036_advance_reject_direct(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 0) != SIXEL_OK) goto end;
    if (indexed_fixture_seal(&f) != SIXEL_OK) goto end;
    ok = f.plane->vtbl->advance_resolved(f.plane, f.update, &f.stats)
        == SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
