/* SPDX-License-Identifier: MIT */
#include "../../_support/processing/dither/clamp/clamp_test_common.h"
int
test_clamp_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return clamp_test_kernel(7);
}
