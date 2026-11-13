/* SPDX-License-Identifier: MIT */
/* Verify reject other method. */
#include <sixel.h>
SIXELSTATUS test_stucki_gpu_carry_configure(int, int, int, double,
    double, float);

int
test_dither_stucki_gpu_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_stucki_gpu_carry_configure(SIXEL_DIFFUSE_FS,
        SIXEL_SCAN_RASTER, SIXEL_DITHER_CLAMP_SOFT, 1.01, 5.0, 0.0f) !=
        SIXEL_BAD_ARGUMENT;
}
