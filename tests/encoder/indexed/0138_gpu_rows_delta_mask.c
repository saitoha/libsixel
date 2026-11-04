/* SPDX-License-Identifier: MIT */
/* Row publication preserves exact delta keeps.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_delta_mask(int argc, char **argv)
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
    f.request.sixdelta_enabled = f.request.has_6delta_accumulation = 1;
    f.request.accumulation_pixels = f.history;
    f.request.accumulation_pixels_size = sizeof(f.history);
    f.request.accumulation_valid_mask = f.valid;
    f.request.accumulation_valid_mask_size = sizeof(f.valid);
    f.request.accumulation_keycolor = 2;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    for (i = 0; i < GPU_ROWS_PIXELS; ++i) {
        ROWS_CHECK(f.mask[i] == (unsigned char)(i % 3 == 0));
    }
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
