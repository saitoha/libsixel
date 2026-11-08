/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

static int
lso2_gpu_cancel_after_callback(void *context)
{
    int *calls;

    calls = context;
    return ++*calls >= 2;
}

int
test_lso2_gpu_cancel_after(int argc, char **argv)
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
    f.run.cancel = lso2_gpu_cancel_after_callback;
    f.run.cancel_context = &calls;
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_INTERRUPTED);
    LSO2_GPU_CHECK(calls == 2 && f.image->rows == 0 &&
        sixel_indexed_frame_get_state(f.image) == SIXEL_INDEXED_FAILED);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
