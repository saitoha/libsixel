/* SPDX-License-Identifier: MIT */
/* A failed image rejects subsequent GPU publication.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/codec/indexed_component_common.h"
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_publish_failed(int argc, char **argv)
{
    indexed_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = 1;
    ROWS_CHECK(indexed_fixture_init(&f));
    sixel_indexed_frame_fail(f.image);
    ROWS_CHECK(sixel_indexed_frame_publish_gpu_rows(f.image, 1)
        == SIXEL_LOGIC_ERROR);
    outcome = 0;
end:
    indexed_fixture_destroy(&f);
    return outcome;
}
