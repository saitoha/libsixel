/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <sixel.h>
#include "assessment/persistence.h"

int
test_qa_0038(int argc, char **argv)
{
    double age;
    double error;
    double weighted;
    double coverage;
    double durations[3] = {0.025, 0.04, 0.135};
    lsqa_persistence_result_t result;
    int i;

    (void)argc;
    (void)argv;
    age = weighted = coverage = 0;
    error = 8;
    for (i = 0; i < 3; ++i) {
        lsqa_persistence_step(&error, &age, 1, durations[i], 2, 0.1,
            &result);
        weighted += result.weighted_excess * durations[i];
        coverage += result.persistent_fraction * durations[i];
    }
    /* The 200 ms integral remains 0.9 CIE76-seconds and 0.1 seconds
     * beyond the ramp, independent of these artificial packet boundaries. */
    return isfinite(weighted) && fabs(weighted - 0.9) < 1e-12
        && fabs(coverage - 0.1) < 1e-12 && fabs(age - 0.2) < 1e-12
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
