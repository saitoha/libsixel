/* SPDX-License-Identifier: MIT */
/* Verify reject low clamp. */
#include <sixel.h>
SIXELSTATUS test_stucki_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_stucki_gpu_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_stucki_gpu_carry_configure(SIXEL_DIFFUSE_STUCKI,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 0.5, 5.0, 0.0f) !=
        SIXEL_BAD_ARGUMENT;
}
