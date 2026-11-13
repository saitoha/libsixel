/* SPDX-License-Identifier: MIT */
/* Verify live carry changes indexes. */
#include <sixel.h>
int test_atkinson_gpu_carry_observation(int);

int
test_dither_atkinson_gpu_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_atkinson_gpu_carry_observation(1);
}
