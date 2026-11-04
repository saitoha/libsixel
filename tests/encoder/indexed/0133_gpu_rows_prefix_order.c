/* SPDX-License-Identifier: MIT */
/* Completed prefixes include the short final band.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_prefix_order(int argc, char **argv)
{
    gpu_rows_fixture_t f;
    int outcome;
    int expected[8] = {6, 12, 18, 24, 30, 36, 42, 43};

    (void)argc;
    (void)argv;
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    outcome = 1;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    ROWS_CHECK(f.calls == 8);
    ROWS_CHECK(!memcmp(f.ends, expected, sizeof(expected)));
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
