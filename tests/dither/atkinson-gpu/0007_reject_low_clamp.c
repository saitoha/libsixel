/* SPDX-License-Identifier: MIT */
/* Verify reject low clamp. */
#include <sixel.h>
SIXELSTATUS test_atkinson_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_atkinson_gpu_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_atkinson_gpu_carry_configure(SIXEL_DIFFUSE_ATKINSON,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 0.5, 5.0, 0.0f) !=
        SIXEL_BAD_ARGUMENT;
}
