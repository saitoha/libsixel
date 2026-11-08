/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

int
test_lso2_gpu_small_error_indexes(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    f.configuration.options.lso2_gain = SIXEL_LSO2_GAIN_SMALL_ERROR;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(memcmp(f.image->indexes, lso2_gpu_expected_small_error,
        LSO2_GPU_PIXELS) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
