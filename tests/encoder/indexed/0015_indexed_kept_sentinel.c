/* SPDX-License-Identifier: MIT */
/* A skipped sentinel is never interpreted as a palette index. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_indexed_kept_sentinel(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    ok = sixel_indexed_frame_publish(f.image, 1) == SIXEL_OK;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
