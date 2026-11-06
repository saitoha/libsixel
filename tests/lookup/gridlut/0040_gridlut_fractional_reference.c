/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_gridlut_fractional_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_fractional_check(SIXEL_LUT_POLICY_GRIDLUT, 0);
}
