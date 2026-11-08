/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_lso2_gpu_deferred_rgb241(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    unsigned char expected[LSO2_GPU_PIXELS];
    int outcome;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 0) == SIXEL_OK);
    f.run.delta.metric = SIXEL_DELTA_METRIC_RGB241;
    f.run.delta.threshold = 48;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    memcpy(expected, f.image->indexes, sizeof(expected));
    sixel_indexed_frame_unref(f.image);
    f.image = NULL;
    LSO2_GPU_CHECK(lso2_gpu_fixture_deferred(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(memcmp(f.image->indexes, expected, sizeof(expected)) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
