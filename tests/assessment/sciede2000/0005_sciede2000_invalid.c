/* SPDX-License-Identifier: MIT */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include "assessment/scielab.h"

int
test_qa_0043(int argc, char **argv)
{
    double a[3] = {0, 0, 0};
    double b[3] = {1, 1, 1};
    double mean = -1;
    double maximum = -2;
    double mean00 = -3;
    double maximum00 = -4;
    double map = -5;

    (void)argc;
    (void)argv;
    /* Reject incomplete summary pairs and ambiguous map ownership. */
    if (sixel_qa_scielab(a, b, 1, 1, 60, NULL, NULL, NULL,
            &mean00, NULL) != SIXEL_BAD_ARGUMENT
            || sixel_qa_scielab(a, b, 1, 1, 60, NULL, NULL, &map,
                &mean00, &maximum00) != SIXEL_BAD_ARGUMENT) {
        return EXIT_FAILURE;
    }
    b[0] = NAN;
    if (sixel_qa_scielab(a, b, 1, 1, 60, &mean, &maximum, &map,
            &mean00, &maximum00) != SIXEL_BAD_INPUT) {
        return EXIT_FAILURE;
    }
    /* Finite input can still overflow filtering or Lab. No partial
     * summary or CIE76 map may escape from the combined computation. */
    b[0] = DBL_MAX;
    if (sixel_qa_scielab(a, b, 1, 1, 60, &mean, &maximum, &map,
            &mean00, &maximum00) != SIXEL_BAD_INPUT) {
        return EXIT_FAILURE;
    }
    return mean == -1 && maximum == -2 && mean00 == -3
        && maximum00 == -4 && map == -5 ? EXIT_SUCCESS : EXIT_FAILURE;
}
