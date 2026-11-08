/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

int
test_lso2_gpu_preparer_lifetime(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;
    sixel_gpu_dither_preparer_interface_t *borrowed;
    sixel_dither_policy_prepare_request_t actual;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    borrowed = f.dither;
    f.dither->vtbl->unref(f.dither);
    f.dither = NULL;
    LSO2_GPU_CHECK(borrowed->vtbl->get_configuration(borrowed, &actual)
        == SIXEL_OK);
    LSO2_GPU_CHECK(actual.options.method_for_diffuse == SIXEL_DIFFUSE_LSO2);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
