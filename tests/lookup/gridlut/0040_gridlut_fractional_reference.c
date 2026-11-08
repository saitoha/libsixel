/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_gpu_gridlut_fractional_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_fractional_check(SIXEL_LUT_POLICY_GRIDLUT, 0);
}
