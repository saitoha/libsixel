/* SPDX-License-Identifier: MIT */
/* Row publication retains input transparency.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_transparency(int argc, char **argv)
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
    f.request.transparent_mask = f.transparent;
    f.request.transparent_mask_size = sizeof(f.transparent);
    f.request.transparent_keycolor = 2;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    ROWS_CHECK(f.calls == 8 &&
        !memcmp(f.mask, f.transparent, sizeof(f.mask)));
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
