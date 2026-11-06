/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_none_fractional_exact(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_fractional_check(SIXEL_LUT_POLICY_NONE, 0);
}
