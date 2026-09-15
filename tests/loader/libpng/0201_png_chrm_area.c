/* SPDX-License-Identifier: MIT */
/* Exact signed areas from independent homogeneous determinant elimination. */
#include <stdio.h>
#include <stdlib.h>
#include "loader_libpng_chrm_area_test_common.h"
#include "loader_libpng_chrm_grid_test_common.h"

int
test_loader_0201_png_chrm_area(int argc, char **argv)
{
    static int const coordinates[][6] = {
        { 18750, 8, 18044, 9, 17338, 10 },
        { 18750, 8, 17338, 10, 18044, 9 },
        { 18044, 9, 18750, 8, 17338, 10 },
        { 18044, 9, 17338, 10, 18750, 8 },
        { 17338, 10, 18750, 8, 18044, 9 },
        { 17338, 10, 18044, 9, 18750, 8 },
        { 40210, 8, 39976, 9, 39742, 10 },
        { 40210, 8, 39742, 10, 39976, 9 },
        { 39976, 9, 40210, 8, 39742, 10 },
        { 39976, 9, 39742, 10, 40210, 8 },
        { 39742, 10, 40210, 8, 39976, 9 },
        { 39742, 10, 39976, 9, 40210, 8 },
        { 100, 100, 101, 100, 100, 101 },
        { 101, 100, 100, 100, 100, 101 },
        { 0, 1, 99999, 1, 0, 100000 },
        { 99999, 1, 0, 1, 0, 100000 },
    };
    static int64_t const expected[] = {
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(0),
        INT64_C(1),
        -INT64_C(1),
        INT64_C(9999800001),
        -INT64_C(9999800001),
    };
    double xy[6];
    double xyz[3];
    int64_t actual;
    int sample;
    int mode;
    int i;

    if (argc != 2) {
        return 1;
    }
    sample = atoi(argv[1]);
    if (sample < 0 || sample >=
        (int)(sizeof(expected) / sizeof(expected[0]))) {
        return 1;
    }
    /* Both expressions occur in floating getters. The expected determinant
     * is a literal from original integer columns, never a rounded product
     * result or a copy of the production two-product area calculation. */
    for (mode = 0; mode < 2; ++mode) {
        for (i = 0; i < 6; ++i) {
            xy[i] = mode ? coordinates[sample][i] * 0.00001
                         : coordinates[sample][i] / 100000.0;
        }
        for (i = 0; i < 6; i += 2) {
            if (!png_chrm_xy_to_xyz(xy[i], xy[i + 1], xyz)) {
                return 1;
            }
        }
        actual = png_chrm_primary_area(xy[0], xy[1], xy[2], xy[3],
                                        xy[4], xy[5]);
        if (actual != expected[sample]) {
            fprintf(stderr, "PNG primary area sample=%d mode=%d: "
                "%lld != %lld\n", sample, mode, (long long)actual,
                (long long)expected[sample]);
            return 1;
        }
    }
    fprintf(stderr, "PNG_CHRM_AREA|sample=%d|checked=2\n", sample);
    return 0;
}
