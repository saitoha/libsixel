/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0006(int argc, char **argv)
{
    double a[16 * 8 * 3];
    double b[16 * 8 * 3];
    double white[3] = {.9642, 1, .8249};
    double mean;
    double maximum;
    size_t count;
    int i;
    int c;
    (void)argc;
    (void)argv;
    /* Global means match, but each 8x8 region is maximally wrong. */
    for (i = 0; i < 128; ++i) {
        for (c = 0; c < 3; ++c) {
            a[i * 3 + c] = (i % 16 < 8) * white[c];
            b[i * 3 + c] = (i % 16 >= 8) * white[c];
        }
    }
    sixel_qa_regions(a, b, 16, 8, 8, &mean, &maximum, &count);
    return fabs(mean - 100) < 1e-9 && fabs(maximum - 100) < 1e-9
        && count == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
}
