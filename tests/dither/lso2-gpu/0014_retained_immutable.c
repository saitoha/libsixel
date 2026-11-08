/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_lso2_gpu_retained_immutable(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;
    sixel_retained_read_view_t view;
    size_t i;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 0) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_retained_read_get_view(f.read, &view) == SIXEL_OK);
    for (i = 0; i < LSO2_GPU_PIXELS*3; ++i) LSO2_GPU_CHECK(view.rgb[i] == 31);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
