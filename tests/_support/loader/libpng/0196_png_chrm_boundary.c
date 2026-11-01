/* SPDX-License-Identifier: MIT */
/* Inspect exact XYZ at a white/primary zero or adjacent inside boundary. */
#include <stdio.h>
#include <stdlib.h>
#include "loader_libpng_chrm_grid_test_common.h"

int
test_loader_0196_png_chrm_boundary(int argc, char **argv)
{
    long const coordinates[4][2] = {
        { 34570L, 65430L }, { 68000L, 32000L },
        { 272L, 99728L }, { 15000L, 85000L }
    };
    double xyz[3];
    long ix;
    long iy;
    int point;
    int inside;

    if (argc != 3) {
        return 1;
    }
    point = atoi(argv[1]);
    inside = atoi(argv[2]);
    if (point < 0 || point >= 4 || (inside != 0 && inside != 1)) {
        return 1;
    }
    ix = coordinates[point][0];
    iy = coordinates[point][1] - inside;
    if (!png_chrm_xy_to_xyz(ix / 100000.0, iy / 100000.0, xyz) ||
        xyz[0] != (double)ix / (double)iy || xyz[1] != 1.0 ||
        xyz[2] != (double)inside / (double)iy) {
        fprintf(stderr, "PNG grid boundary point=%d inside=%d differs\n",
            point, inside);
        return 1;
    }
    fprintf(stderr, "PNG_CHRM_GRID|boundary|point=%d|inside=%d|checked=3\n",
        point, inside);
    return 0;
}
