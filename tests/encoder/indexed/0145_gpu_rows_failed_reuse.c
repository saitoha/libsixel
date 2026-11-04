/* SPDX-License-Identifier: MIT */
/* A failed request releases its lookup lease for the next run.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_failed_reuse(int argc, char **argv)
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
    f.hooks.fault = 3;
    f.hooks.fault_after_chunks = 1;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_RUNTIME_ERROR);
    f.hooks.fault = 0;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    for (i = 0; i < GPU_ROWS_PIXELS; ++i) {
        ROWS_CHECK(f.output[i] == (unsigned char)(i % 3 != 0));
    }
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
