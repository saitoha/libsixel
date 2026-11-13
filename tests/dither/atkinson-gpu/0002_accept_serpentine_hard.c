/* SPDX-License-Identifier: MIT */
/* Verify accept serpentine hard. */
#include <sixel.h>
SIXELSTATUS test_atkinson_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_atkinson_gpu_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_atkinson_gpu_carry_configure(SIXEL_DIFFUSE_ATKINSON,
        SIXEL_SCAN_SERPENTINE, SIXEL_DITHER_CLAMP_HARD, 1.0, 0.0, 0.0f) !=
        SIXEL_OK;
}
