/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

/* GPU error diffusion through the policies whose RGB8 GPU contract is the
 * exact first-index nearest entry, on Metal (injected program) or Vulkan
 * (device-less scan). Each must reproduce the NONE lookup's indexes for
 * the fixed-tap and LSO2 kernels, in the monolithic run and in the
 * deferred solve and resolve against history. */

static int
direct_exact_solve(int kind, int method, int deferred,
    unsigned char *indexes)
{
    lso2_gpu_fixture_t f;
    sixel_lookup_policy_configuration_t configuration;
    int outcome;

    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = kind;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    outcome = lso2_gpu_fixture_open_lookup(&f, method, &configuration);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 1) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    if (deferred) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_deferred(&f) == SIXEL_OK);
    } else {
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    }
    memcpy(indexes, f.image->indexes, LSO2_GPU_PIXELS);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}

int
test_gpu_direct_0018_diffuse_exact(int argc, char **argv)
{
    static int const kinds[] = {
        SIXEL_LUT_POLICY_MAHALANOBIS, SIXEL_LUT_POLICY_RBC,
        SIXEL_LUT_POLICY_CERTLUT
    };
    unsigned char none[LSO2_GPU_PIXELS], indexes[LSO2_GPU_PIXELS];
    size_t n;
    int k, deferred, method, outcome;

    (void)argc;
    (void)argv;
    for (k = 0; k < 2; ++k) {
        method = k == 0 ? SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_LSO2;
        for (deferred = 0; deferred < 2; ++deferred) {
            outcome = direct_exact_solve(SIXEL_LUT_POLICY_NONE, method,
                deferred, none);
            if (outcome != 0) return outcome;
            for (n = 0; n < sizeof(kinds) / sizeof(*kinds); ++n) {
                outcome = direct_exact_solve(kinds[n], method, deferred,
                    indexes);
                if (outcome != 0) return outcome;
                if (memcmp(indexes, none, LSO2_GPU_PIXELS) != 0) {
                    fprintf(stderr, "direct exact: kind %d method %d "
                        "deferred %d mismatch\n", kinds[n], method,
                        deferred);
                    return 1;
                }
            }
        }
    }
    return 0;
}
