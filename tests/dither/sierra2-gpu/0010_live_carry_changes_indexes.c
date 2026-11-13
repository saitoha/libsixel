/* SPDX-License-Identifier: MIT */
/* Verify live carry changes indexes. */
#include <sixel.h>
int test_sierra2_gpu_carry_observation(int);

int
test_dither_sierra2_gpu_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_sierra2_gpu_carry_observation(1);
}
