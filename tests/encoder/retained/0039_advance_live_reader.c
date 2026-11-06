/* SPDX-License-Identifier: MIT */
/* A live read prevents resolved publication without mutation. */
#include "../../_support/codec/indexed_component_common.h"

int
test_retained_0039_advance_live_reader(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f) ||
            indexed_fixture_begin_kind(&f, 1) != SIXEL_OK) goto end;
    if (sixel_indexed_frame_publish(f.image, 1) != SIXEL_OK ||
            sixel_indexed_frame_complete(f.image) != SIXEL_OK) goto end;
    if (f.plane->vtbl->advance_resolved(f.plane, f.update, &f.stats) !=
            SIXEL_LOGIC_ERROR) goto end;
    ok = f.view.sequence == 1 &&
        memcmp(f.view.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
