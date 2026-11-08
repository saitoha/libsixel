/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "src/lookup-policy-gpu-legacy.h"

int
test_lso2_gpu_byte_lookup_rejected(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
    sixel_lookup_policy_configuration_t configuration;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    f.lookup->vtbl->unref(f.lookup);
    f.lookup = NULL;
    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = SIXEL_LUT_POLICY_EYTZINGER;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    LSO2_GPU_CHECK(sixel_gpu_lookup_legacy_create(f.allocator,
        &configuration, &f.lookup) == SIXEL_OK);
    f.prepare.gpu_lookup = f.lookup;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_FEATURE_ERROR);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}
