/* SPDX-License-Identifier: MIT */
/* Verify reject negative deadzone. */
#include <sixel.h>
SIXELSTATUS test_burkes_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_burkes_gpu_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_burkes_gpu_carry_configure(SIXEL_DIFFUSE_BURKES,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 1.01, -1.0, 0.0f) !=
        SIXEL_BAD_ARGUMENT;
}
