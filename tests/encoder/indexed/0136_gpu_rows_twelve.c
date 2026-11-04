/* SPDX-License-Identifier: MIT */
/* A twelve-row group preserves the index oracle.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_twelve(int argc, char **argv)
{
    gpu_rows_fixture_t f;
    int outcome;
    int i;

    (void)argc;
    (void)argv;
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    outcome = 1;
    f.request.rows_per_chunk = 12;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    ROWS_CHECK(f.calls == 4);
    for (i = 0; i < GPU_ROWS_PIXELS; ++i) {
        ROWS_CHECK(f.output[i] == (unsigned char)(i % 3 != 0));
    }
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
