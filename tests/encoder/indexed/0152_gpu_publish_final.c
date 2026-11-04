/* SPDX-License-Identifier: MIT */
/* A completed GPU prefix becomes readable.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/codec/indexed_component_common.h"
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_publish_final(int argc, char **argv)
{
    indexed_fixture_t f;
    sixel_indexed_rows_view_t view;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = 1;
    ROWS_CHECK(indexed_fixture_init(&f));
    ROWS_CHECK(sixel_indexed_frame_publish_gpu_rows(f.image, 1) == SIXEL_OK);
    ROWS_CHECK(sixel_indexed_frame_get_rows(f.image, 0, 1, &view) == SIXEL_OK);
    ROWS_CHECK(view.indexes[0] == 0 &&
        view.skip[1] == SIXEL_INDEXED_KEEP_APPROXIMATE);
    outcome = 0;
end:
    indexed_fixture_destroy(&f);
    return outcome;
}
