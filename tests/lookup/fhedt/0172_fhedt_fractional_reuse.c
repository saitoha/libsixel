/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_gpu_fhedt_fractional_reuse(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_fractional_reuse_check(SIXEL_LUT_POLICY_FHEDT);
}
