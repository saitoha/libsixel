/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"
#include "src/lookup-policy-gpu.h"

int
test_lso2_gpu_mahalanobis_refresh(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    sixel_lookup_policy_configuration_t configuration;
    int outcome;
    uint32_t selection[128];
    int x, y, cell;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    f.lookup->vtbl->unref(f.lookup);
    f.lookup = NULL;
    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = SIXEL_LUT_POLICY_MAHALANOBIS;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    LSO2_GPU_CHECK(sixel_gpu_lookup_select(f.allocator,
        &configuration, &f.lookup) == SIXEL_OK);
    f.prepare.gpu_lookup = f.lookup;
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
