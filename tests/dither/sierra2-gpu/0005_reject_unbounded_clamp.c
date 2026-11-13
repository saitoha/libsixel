/* SPDX-License-Identifier: MIT */
/* Verify reject unbounded clamp. */
#include <sixel.h>
SIXELSTATUS test_sierra2_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_sierra2_gpu_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_sierra2_gpu_carry_configure(SIXEL_DIFFUSE_SIERRA2,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_NONE, 1.01, 5.0, 0.0f) !=
        SIXEL_FEATURE_ERROR;
}
