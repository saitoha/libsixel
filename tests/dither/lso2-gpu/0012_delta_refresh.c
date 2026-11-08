/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

int
test_lso2_gpu_delta_refresh(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    int outcome;
    uint32_t selection[128];
    int x, y, cell;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 0) == SIXEL_OK);
    memset(selection, 0, sizeof(selection));
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            cell = y*64+x;
            if ((x+y)%2 == 0) selection[cell/32] |= 1u << (cell%32);
        }
    }
    f.run.delta.threshold = 48;
    f.run.delta.selection.columns = f.run.delta.selection.rows = 64;
    f.run.delta.selection.tiled = 1;
    f.run.delta.selection.words = selection;
    f.run.delta.selection.word_count = 128;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(memcmp(f.image->indexes, lso2_gpu_expected_refresh,
        LSO2_GPU_PIXELS) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
