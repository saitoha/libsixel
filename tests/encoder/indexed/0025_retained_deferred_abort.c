/* SPDX-License-Identifier: MIT */
/* The last resource user safely drains an abandoned update. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_retained_deferred_abort(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    if (indexed_fixture_begin(&f) != SIXEL_OK) goto end;
    sixel_indexed_frame_acquire_user(f.image);
    sixel_retained_update_unref(f.update);
    f.update = NULL;
    sixel_indexed_frame_release_user(f.image);
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    ok = f.plane->vtbl->invalidate(f.plane, SIXEL_RETAINED_SEEK) == SIXEL_OK;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
