/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "assessment/scielab.h"
#include "tests/data/assessment/scielab_reference.h"

int
test_qa_0028(int argc, char **argv)
{
    double mean;
    double maximum;
    double map[20];
    int i;
    int p;

    (void)argc;
    (void)argv;
    /* Absolute DE76 budget covers double convolution, matrix inversion
     * and Lab slopes on the signed fixture. See metrics.md. */
    mean = maximum = 0;
    for (i = 0; i < 4; ++i) {
        if (SIXEL_FAILED(sixel_qa_scielab(scielab_reference_xyz,
                scielab_output_xyz, 5, 4, scielab_reference_ppd[i],
                &mean, &maximum, map)) || !isfinite(mean)
                || !isfinite(maximum)
                || fabs(mean - scielab_reference_mean[i]) > 1e-6
                || fabs(maximum - scielab_reference_max[i]) > 1e-6) {
            fprintf(stderr, "scielab ppd=%g: mean=%g max=%g\n",
                scielab_reference_ppd[i], mean, maximum);
            return EXIT_FAILURE;
        }
        for (p = 0; p < 20; ++p) {
            if (!isfinite(map[p])
                    || fabs(map[p] - scielab_reference_map[20 * i + p])
                        > 1e-6) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
