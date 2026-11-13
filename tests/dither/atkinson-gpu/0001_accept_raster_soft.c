/* SPDX-License-Identifier: MIT */
/* Verify accept raster soft. */
#include <sixel.h>
SIXELSTATUS test_atkinson_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_atkinson_gpu_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_atkinson_gpu_carry_configure(SIXEL_DIFFUSE_ATKINSON,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) !=
        SIXEL_OK;
}
