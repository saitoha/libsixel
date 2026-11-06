/* SPDX-License-Identifier: MIT */
/* A stale resolved identity leaves history unchanged. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0040_advance_stale_identity(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 1) != SIXEL_OK) goto end;
    if (indexed_fixture_seal(&f) != SIXEL_OK) goto end;
    ++f.image->identity.base_sequence;
    if (f.plane->vtbl->advance_resolved(f.plane, f.update, &f.stats) !=
            SIXEL_LOGIC_ERROR) goto end;
    if (f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
            sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK) goto end;
    ok = f.view.sequence == 1;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
