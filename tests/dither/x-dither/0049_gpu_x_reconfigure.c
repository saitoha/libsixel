/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

int
test_positional_gpu_x_reconfigure(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_gpu_reconfigure_check(SIXEL_DIFFUSE_X_DITHER);
}
