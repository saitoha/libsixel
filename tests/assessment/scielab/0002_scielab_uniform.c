/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include "assessment/scielab.h"

int
test_qa_0027(int argc, char **argv)
{
    double a[3 * 2 * 3];
    double b[3 * 2 * 3];
    double const white[3] = {0.9642, 1, 0.8249};
    double const ppds[] = {1, 12, 28.5, 60, 224, 1024};
    double mean;
    double maximum;
    int p;
    int c;
    int i;

    (void)argc;
    (void)argv;
    /* Neutral Y=1/8 has L*=42, while Y=1 has L*=100: DE76=58.
     * This analytic oracle detects loss of DC through low-ppd resampling. */
    for (p = 0; p < 6; ++p) {
        for (c = 0; c < 3; ++c) {
            a[3 * p + c] = white[c] / 8;
            b[3 * p + c] = white[c];
        }
    }
    for (i = 0; i < 6; ++i) {
        if (SIXEL_FAILED(sixel_qa_scielab(a, b, 3, 2, ppds[i],
                &mean, &maximum, NULL)) || !isfinite(mean)
                || !isfinite(maximum) || fabs(mean - 58) > 1e-7
                || fabs(maximum - 58) > 1e-7) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
