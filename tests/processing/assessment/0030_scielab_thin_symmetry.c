/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include "assessment/scielab.h"

int
test_qa_0030(int argc, char **argv)
{
    double const a[15] = {0, 0, 0, .2, .1, .3, .4, .5, .6,
                          .1, .2, .3, .6, .7, .8};
    double const b[15] = {.1, .2, .3, .3, .1, .4, .6, .8, .2,
                          .3, .1, .4, .4, .5, .6};
    double mean1;
    double mean2;
    double max1;
    double max2;
    double map1[5];
    double map2[5];
    int p;

    (void)argc;
    (void)argv;
    /* Transpose and swap the inputs together. Isotropic kernels and a
     * symmetric color distance preserve the entire error map. */
    if (SIXEL_FAILED(sixel_qa_scielab(a, b, 5, 1, 60,
            &mean1, &max1, map1))
            || SIXEL_FAILED(sixel_qa_scielab(b, a, 1, 5, 60,
                &mean2, &max2, map2))) {
        return EXIT_FAILURE;
    }
    for (p = 0; p < 5; ++p) {
        if (!isfinite(map1[p]) || !isfinite(map2[p]) || map1[p] <= 0
                || fabs(map1[p] - map2[p]) > 1e-7) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
