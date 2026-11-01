/* SPDX-License-Identifier: MIT */
/* An out-of-range painted index seals the producer as failed. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_indexed_invalid_paint(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    f.image->indexes[0] = (sixel_index_t)-1;
    (void)sixel_indexed_frame_publish(f.image, 1);
    ok = sixel_indexed_frame_get_state(f.image) == SIXEL_INDEXED_FAILED;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
