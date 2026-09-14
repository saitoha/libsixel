/* SPDX-License-Identifier: MIT */
/* Reject outside grid, unsafe floating values and zero denominators. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "loader_libpng_chrm_grid_test_common.h"

int
test_loader_0197_png_chrm_reject(int argc, char **argv)
{
    double const coordinates[][2] = {
        { 0.34570, 0.65431 }, { 0.68000, 0.32001 },
        { 0.00272, 0.99729 }, { 0.15000, 0.85001 },
        { -0.00001, 0.3 }, { 1.00001, 0.3 },
        { 0.3, 0.0 }, { 0.3, -0.00001 }, { 0.3, 1.00001 },
        { NAN, 0.3 }, { INFINITY, 0.3 }, { -INFINITY, 0.3 },
        { 0.3, NAN }, { 0.3, INFINITY }
    };
    double xyz[3];
    int sample;

    if (argc != 2) {
        return 1;
    }
    sample = atoi(argv[1]);
    if (sample < 0 || sample >=
        (int)(sizeof(coordinates) / sizeof(coordinates[0]))) {
        return 1;
    }
    if (png_chrm_xy_to_xyz(coordinates[sample][0],
                           coordinates[sample][1], xyz)) {
        fprintf(stderr, "PNG grid invalid sample=%d was accepted\n", sample);
        return 1;
    }
    fprintf(stderr, "PNG_CHRM_GRID|reject|sample=%d|checked=1\n", sample);
    return 0;
}
