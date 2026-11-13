/* SPDX-License-Identifier: MIT */
/* Verify reject lso2 method. */
#include <sixel.h>
SIXELSTATUS test_sierra2_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_sierra2_gpu_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_sierra2_gpu_carry_configure(SIXEL_DIFFUSE_LSO2,
        SIXEL_SCAN_SERPENTINE, SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) !=
        SIXEL_BAD_ARGUMENT;
}
