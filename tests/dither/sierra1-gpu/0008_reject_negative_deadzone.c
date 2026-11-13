/* SPDX-License-Identifier: MIT */
/* Verify reject negative deadzone. */
#include <sixel.h>
SIXELSTATUS test_sierra1_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_sierra1_gpu_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_sierra1_gpu_carry_configure(SIXEL_DIFFUSE_SIERRA1,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 1.01, -1.0, 0.0f) !=
        SIXEL_BAD_ARGUMENT;
}
