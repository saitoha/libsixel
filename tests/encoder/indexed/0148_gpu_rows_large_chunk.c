/* SPDX-License-Identifier: MIT */
/* An oversized chunk retains whole-image dispatch.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_large_chunk(int argc, char **argv)
{
    gpu_rows_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    outcome = 1;
    f.request.rows_per_chunk = GPU_ROWS_HEIGHT + 1;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    ROWS_CHECK(f.hooks.submitted_chunks == 1 && f.calls == 0);
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
