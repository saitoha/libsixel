/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0001(int argc, char **argv)
{
    /* Sharma/Wu/Dalal supplementary pairs, including hue discontinuity.
     * https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/ */
    static double const pairs[][7] = {
        {50, 2.6772, -79.7751, 50, 0, -82.7485, 2.0425},
        {50, 0, 0, 50, -1, 2, 2.3669},
        /* Zero chroma on either side has zero hue, not a NaN angle. */
        {50, -1, 2, 50, 0, 0, 2.3669},
        {0, 0, 0, 100, 0, 0, 100},
        {0, 0, 0, 0, 0, 0, 0},
        {50, 2.49, -0.001, 50, -2.49, 0.0009, 7.1792},
        {50, 2.49, -0.001, 50, -2.49, 0.0011, 7.2195},
        {2.0776, .0795, -1.135, .9033, -.0636, -.5514, .9082}
    };
    size_t i;
    double result;
    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        result = sixel_qa_de00(pairs[i], pairs[i] + 3);
        if (!isfinite(result) || fabs(result - pairs[i][6]) > 0.00005) {
            fprintf(stderr, "CIEDE2000 pair %lu: %.9g != %.9g\n",
                    (unsigned long)i, result, pairs[i][6]);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
