/* SPDX-License-Identifier: MIT */
/* Completion requires every source row to be published. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_indexed_early_complete(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    ok = sixel_indexed_frame_complete(f.image) == SIXEL_LOGIC_ERROR;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
