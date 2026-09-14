/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0007(int argc, char **argv)
{
    double a[9 * 3] = {0};
    double b[9 * 3] = {0};
    double mean;
    double maximum;
    size_t count;
    (void)argc;
    (void)argv;
    b[24] = .9642;
    b[25] = 1;
    b[26] = .8249;
    sixel_qa_regions(a, b, 9, 1, 8, &mean, &maximum, &count);
    return fabs(mean - 100.0 / 9.0) < 1e-9
        && fabs(maximum - 100) < 1e-9 && count == 2
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
