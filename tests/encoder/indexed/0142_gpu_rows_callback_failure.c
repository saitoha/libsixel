/* SPDX-License-Identifier: MIT */
/* A rejected prefix propagates the consumer error after drain.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_callback_failure(int argc, char **argv)
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
    f.callback_status = SIXEL_BAD_INPUT;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_BAD_INPUT);
    ROWS_CHECK(f.calls == 1 && f.hooks.submitted_chunks > 1 &&
        f.hooks.completed_chunks == f.hooks.submitted_chunks);
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
