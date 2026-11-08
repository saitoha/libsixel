/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

static int
lso2_gpu_cancel_reuse_callback(void *context)
{
    int *calls;

    calls = context;
    return ++*calls >= 2;
}

int
test_lso2_gpu_cancel_reuse(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;
    int calls = 0;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    f.run.cancel = lso2_gpu_cancel_reuse_callback;
    f.run.cancel_context = &calls;
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_INTERRUPTED);
    f.run.cancel = NULL;
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(memcmp(f.image->indexes, lso2_gpu_expected_standard,
        LSO2_GPU_PIXELS) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
