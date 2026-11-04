/* SPDX-License-Identifier: MIT */
/* A GPU prefix cannot exceed the image height.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/codec/indexed_component_common.h"
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_publish_bounds(int argc, char **argv)
{
    indexed_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = 1;
    ROWS_CHECK(indexed_fixture_init(&f));
    ROWS_CHECK(sixel_indexed_frame_publish_gpu_rows(f.image, 2)
        == SIXEL_BAD_ARGUMENT);
    outcome = 0;
end:
    indexed_fixture_destroy(&f);
    return outcome;
}
