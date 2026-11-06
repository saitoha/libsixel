/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_mahalanobis_fractional_ties(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_fractional_check(SIXEL_LUT_POLICY_MAHALANOBIS, 1);
}
