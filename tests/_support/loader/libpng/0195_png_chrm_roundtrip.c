/* SPDX-License-Identifier: MIT */
/* Recover every usable PNG grid coordinate from either double scaling. */
#include <stdio.h>
#include "loader_libpng_chrm_grid_test_common.h"

int
test_loader_0195_png_chrm_roundtrip(int argc, char **argv)
{
    long ix;
    long iy;
    double x;
    double y;
    double xyz[3];
    int mode;

    (void)argv;
    if (argc != 1) {
        return 1;
    }
    for (mode = 0; mode < 2; ++mode) {
        for (ix = 0L; ix < 100000L; ++ix) {
            iy = 100000L - ix;
            x = mode ? ix * 0.00001 : ix / 100000.0;
            y = mode ? iy * 0.00001 : iy / 100000.0;
            if (!png_chrm_xy_to_xyz(x, y, xyz) ||
                xyz[0] != (double)ix / (double)iy) {
                fprintf(stderr, "PNG grid recovery mode=%d x=%ld y=%ld\n",
                    mode, ix, iy);
                return 1;
            }
        }
    }
    fprintf(stderr, "PNG_CHRM_GRID|roundtrip|checked=200000\n");
    return 0;
}
