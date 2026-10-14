/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <sixel.h>
#include "assessment/persistence.h"

int
test_qa_0036(int argc, char **argv)
{
    double short_age;
    double long_age;
    double error;
    lsqa_persistence_result_t short_result;
    lsqa_persistence_result_t long_result;

    (void)argc;
    (void)argv;
    short_age = long_age = 0;
    error = 8;
    lsqa_persistence_step(&error, &short_age, 1, 0.025, 2, 0.1,
        &short_result);
    lsqa_persistence_step(&error, &long_age, 1, 0.2, 2, 0.1,
        &long_result);
    /* Triangle area gives mean weight 1/8 for 25 ms; triangle plus
     * rectangle gives 3/4 for 200 ms. Excess is exactly 8 - 2 = 6.
     * 1e-12 bounds the few double operations on these decimal inputs. */
    return isfinite(short_result.weighted_excess)
        && fabs(short_result.weighted_excess - 0.75) < 1e-12
        && isfinite(long_result.weighted_excess)
        && fabs(long_result.weighted_excess - 4.5) < 1e-12
        && short_result.persistent_fraction == 0
        && fabs(long_result.persistent_fraction - 0.5) < 1e-12
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
