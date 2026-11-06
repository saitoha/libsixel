/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_fhedt_fractional_r256_f0_d0(int argc, char **argv)
{
    sixel_lookup_policy_configuration_t config;

    (void)argc;
    (void)argv;
    memset(&config, 0, sizeof(config));
    config.kind = SIXEL_LUT_POLICY_FHEDT;
    config.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    config.fhedt_resolution = 256;
    config.fhedt_refine = 0;
    config.fhedt_use_dist2 = 0;
    config.fhedt_weights[0] = config.fhedt_weights[1] =
        config.fhedt_weights[2] = 1.0;
    return phased_fractional_config_check(&config, 0);
}
