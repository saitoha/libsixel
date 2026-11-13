/* SPDX-License-Identifier: MIT */
/* Verify reject perturbation. */
#include <sixel.h>
SIXELSTATUS test_sierra3_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_sierra3_gpu_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_sierra3_gpu_carry_configure(SIXEL_DIFFUSE_SIERRA3,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.5f) !=
        SIXEL_FEATURE_ERROR;
}
