/* SPDX-License-Identifier: MIT */
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "assessment/scielab.h"

int
test_qa_0031(int argc, char **argv)
{
    double a[3] = {0, 0, 0};
    double const bad_ppd[] = {0, -1, 1024.1, NAN, INFINITY};
    double mean;
    double maximum;
    double map;
    int i;

    (void)argc;
    (void)argv;
    mean = maximum = map = -7;
    for (i = 0; i < 5; ++i) {
        if (sixel_qa_scielab(a, a, 1, 1, bad_ppd[i],
                &mean, &maximum, &map) != SIXEL_BAD_ARGUMENT) {
            return EXIT_FAILURE;
        }
    }
    if (sixel_qa_scielab(a, a, INT_MAX, INT_MAX, 60,
            &mean, &maximum, &map) != SIXEL_BAD_ALLOCATION
            || sixel_qa_scielab(a, a, 0, 1, 60,
                &mean, &maximum, &map) != SIXEL_BAD_ARGUMENT) {
        return EXIT_FAILURE;
    }
    a[0] = NAN;
    if (sixel_qa_scielab(a, a, 1, 1, 60,
            &mean, &maximum, &map) != SIXEL_BAD_INPUT) {
        return EXIT_FAILURE;
    }
    return mean == -7 && maximum == -7 && map == -7
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
