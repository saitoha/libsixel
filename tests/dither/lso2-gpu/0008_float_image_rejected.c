/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_lso2_gpu_float_image_rejected(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 0);
    if (outcome != 0) goto end;
    outcome = 1;
    f.configuration.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    LSO2_GPU_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
        == SIXEL_FEATURE_ERROR);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
