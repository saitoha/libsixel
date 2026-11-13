/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_positional_gpu_a_isolation(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_gpu_component_check(SIXEL_DIFFUSE_A_DITHER, 2);
}
