/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_lso2_gpu_invalid_gain_atomic(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;
    sixel_dither_policy_prepare_request_t actual;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 0);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
        == SIXEL_OK);
    f.configuration.options.lso2_gain = (sixel_lso2_gain_t)9;
    LSO2_GPU_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
        == SIXEL_BAD_ARGUMENT);
    LSO2_GPU_CHECK(f.dither->vtbl->get_configuration(f.dither, &actual)
        == SIXEL_OK &&
        actual.options.lso2_gain == SIXEL_LSO2_GAIN_STANDARD);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
