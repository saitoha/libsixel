/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <sixel.h>
#include "assessment/persistence.h"

int
test_qa_0037(int argc, char **argv)
{
    double ages[2] = {0.08, 0.07};
    double errors[2] = {2, 8};
    lsqa_persistence_result_t result;

    (void)argc;
    (void)argv;
    lsqa_persistence_step(errors, ages, 2, 0.04, 2, 0.1, &result);
    /* Equality resets only that pixel. The other spends 30 ms ramping
     * from 0.7 to 1, then 10 ms at 1. Divide both areas by two pixels. */
    return ages[0] == 0 && fabs(ages[1] - 0.11) < 1e-12
        && isfinite(result.weighted_excess)
        && fabs(result.weighted_excess - 2.6625) < 1e-12
        && fabs(result.persistent_fraction - 0.125) < 1e-12
        && fabs(result.max_age - 0.11) < 1e-12
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
