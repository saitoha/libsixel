/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "assessment/scielab.h"
#include "tests/data/assessment/sciede2000_reference.h"

int
test_qa_0040(int argc, char **argv)
{
    double mean = 0;
    double maximum = 0;
    double mean76;
    double maximum76;
    double separate_mean;
    double separate_maximum;
    double map[20];
    double separate_map[20];
    int i;
    int p;

    (void)argc;
    (void)argv;
    /* Independent dense filters and vectorized DE00 supply the oracle.
     * The absolute budget includes Lab input and DE00 rounding; see
     * metrics.md. Separate/combined and reversed runs check composition. */
    for (i = 0; i < 4; ++i) {
        if (SIXEL_FAILED(sixel_qa_scielab(sciede2000_reference_xyz,
                sciede2000_output_xyz, 5, 4, sciede2000_reference_ppd[i],
                &mean76, &maximum76, map, &mean, &maximum))
                || !isfinite(mean) || !isfinite(maximum)
                || fabs(mean - sciede2000_reference_mean[i]) > 1e-5
                || fabs(maximum - sciede2000_reference_max[i]) > 1e-5) {
            fprintf(stderr, "spatial DE00 ppd=%g: mean=%g max=%g\n",
                sciede2000_reference_ppd[i], mean, maximum);
            return EXIT_FAILURE;
        }
        if (SIXEL_FAILED(sixel_qa_scielab(sciede2000_output_xyz,
                sciede2000_reference_xyz, 5, 4, sciede2000_reference_ppd[i],
                NULL, NULL, NULL, &separate_mean, &separate_maximum))
                || !isfinite(separate_mean) || !isfinite(separate_maximum)
                || fabs(separate_mean - mean) > 1e-7
                || fabs(separate_maximum - maximum) > 1e-7) {
            return EXIT_FAILURE;
        }
        if (SIXEL_FAILED(sixel_qa_scielab(sciede2000_reference_xyz,
                sciede2000_output_xyz, 5, 4, sciede2000_reference_ppd[i],
                &separate_mean, &separate_maximum, separate_map, NULL, NULL))
                || separate_mean != mean76 || separate_maximum != maximum76) {
            return EXIT_FAILURE;
        }
        for (p = 0; p < 20; ++p) {
            if (map[p] != separate_map[p]) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
