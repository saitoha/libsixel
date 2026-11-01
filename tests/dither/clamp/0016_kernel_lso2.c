/* SPDX-License-Identifier: MIT */
#include "../../_support/processing/dither/clamp/clamp_test_common.h"
int
test_clamp_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return clamp_test_kernel(8);
}
