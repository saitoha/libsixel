/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include "assessment/scielab.h"

int
test_qa_0029(int argc, char **argv)
{
    double reference[16 * 16 * 3];
    double checker[16 * 16 * 3];
    double broad[16 * 16 * 3];
    double fine_mean;
    double broad_mean;
    double maximum;
    double sign;
    int x;
    int y;
    int p;

    (void)argc;
    (void)argv;
    /* The two targets contain the same two colors equally often, with
     * identical Y. Only their spatial arrangement changes. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            p = 3 * (16 * y + x);
            reference[p] = 0.4 * 0.9642;
            reference[p + 1] = 0.4;
            reference[p + 2] = 0.4 * 0.8249;
            sign = (x + y) % 2 ? 1 : -1;
            checker[p] = reference[p] + sign * 0.04;
            checker[p + 1] = reference[p + 1];
            checker[p + 2] = reference[p + 2] - sign * 0.04;
            sign = x < 8 ? 1 : -1;
            broad[p] = reference[p] + sign * 0.04;
            broad[p + 1] = reference[p + 1];
            broad[p + 2] = reference[p + 2] - sign * 0.04;
        }
    }
    if (SIXEL_FAILED(sixel_qa_scielab(reference, checker, 16, 16, 60,
            &fine_mean, &maximum, NULL))
            || SIXEL_FAILED(sixel_qa_scielab(reference, broad, 16, 16, 60,
                &broad_mean, &maximum, NULL))
            || !isfinite(fine_mean) || !isfinite(broad_mean)) {
        return EXIT_FAILURE;
    }
    return fine_mean < broad_mean && broad_mean > 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
